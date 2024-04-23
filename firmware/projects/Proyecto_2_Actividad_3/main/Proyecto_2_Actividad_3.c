/*! @mainpage Blinking
 *
 * \section genDesc General Description
 *
 * This example makes LED_1, LED_2 and LED_3 blink at different rates, using FreeRTOS tasks.
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 17/04/2024 | Document creation		                         |
 *
 * @author Guillermina Zaragoza
 *
 */
/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "gpio_mcu.h"
#include "hc_sr04.h"
#include "lcditse0803.h"
#include "switch.h"
#include "timer_mcu.h"
#include "uart_mcu.h"
/*==================[macros and definitions]=================================*/
#define REFRESCO_MEDICION 1000000
#define REFRESCO_DISPLAY 100000
/*==================[internal data definition]===============================*/
uint16_t distancia = 0;
bool hold;
bool on;
TaskHandle_t Medir_task_handle = NULL;
TaskHandle_t Mostrar_task_handle = NULL;
/*==================[internal functions declaration]=========================*/
/**
 * @brief Función invocada en la interrupción del timer Medir
 */
void FuncTimerMedir()
{
    vTaskNotifyGiveFromISR(Medir_task_handle, pdFALSE); /* Envía una notificación a la tarea asociada a medir */
}

/**
 * @brief Función invocada en la interrupción del timer MostrarDisplay
 */
void FuncTimerMostrarDisplay()
{
    vTaskNotifyGiveFromISR(Mostrar_task_handle, pdFALSE); /* Envía una notificación a la tarea asociada a mostrar display */
}

static void MedirDistanciaTask()
{
    while (true)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (on)
        {
            distancia = HcSr04ReadDistanceInCentimeters();
        }
    }
}

static void MostrarTask()
{
    while (true)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (on)
        {
            if (distancia < 10)
            {
                LedsOffAll();
            }
            else if ((distancia > 10) & (distancia < 20))
            {
                LedOn(LED_1);
                LedOff(LED_2);
                LedOff(LED_3);
            }
            else if ((distancia > 20) & (distancia < 30))
            {
                LedOn(LED_1);
                LedOn(LED_2);
                LedOff(LED_3);
            }
            else if (distancia > 30)
            {
                LedOn(LED_1);
                LedOn(LED_2);
                LedOn(LED_3);
            }
            if (!hold)
            {
                LcdItsE0803Write(distancia);
            }
            UartSendString(UART_PC, "distancia: ");
            UartSendString(UART_PC, (char*)UartItoa(distancia, 10));
            UartSendString(UART_PC, " cm\r\n");
        }
        else
        {
            LcdItsE0803Off();
            LedsOffAll();
        }
    }
}

void Tecla1()
{
    on = !on;
}

void Tecla2()
{
    if (on)
    {
        hold = !hold;
    }
}

void ControlTeclas()
{
    uint8_t letra;
    UartReadByte(UART_PC, &letra);
    switch (letra)
    {
    case 'O':
        on = !on;
        break;
    case 'H':
        hold = !hold;
        break;
    }
}
/*==================[external functions definition]==========================*/
void app_main(void)
{
    LedsInit();
    LcdItsE0803Init();
    HcSr04Init(GPIO_3, GPIO_2);
    SwitchesInit();

    /*Inicializacion del timer medir*/
    timer_config_t timer_medicion = {
        .timer = TIMER_A,
        .period = REFRESCO_MEDICION, // los timers trabajan en microsegundos
        .func_p = FuncTimerMedir,
        .param_p = NULL};
    TimerInit(&timer_medicion);

    /*Inicializacion del timer mostrar display*/
    timer_config_t timer_mostrarDisplay = {
        .timer = TIMER_B,
        .period = REFRESCO_DISPLAY,
        .func_p = FuncTimerMostrarDisplay,
        .param_p = NULL};
    TimerInit(&timer_mostrarDisplay);

    /*Inicializacion de terminal PC*/
    serial_config_t terminal_PC = {
        .port = UART_PC,
        .baud_rate = 115200,
        .func_p = &ControlTeclas,
        .param_p = NULL};
    UartInit(&terminal_PC);

    /*Interrupciones de teclas*/
    SwitchActivInt(SWITCH_1, &Tecla1, NULL);
    SwitchActivInt(SWITCH_2, &Tecla2, NULL);

    /* Creacion de tareas */
    xTaskCreate(&MedirDistanciaTask, "MedirDistancia", 512, NULL, 5, &Medir_task_handle);
    xTaskCreate(&MostrarTask, "Mostrar_LEDsyDisplay", 512, NULL, 5, &Mostrar_task_handle);

    /* Inicio del conteo de timers */
    TimerStart(timer_medicion.timer);
    TimerStart(timer_mostrarDisplay.timer);
}