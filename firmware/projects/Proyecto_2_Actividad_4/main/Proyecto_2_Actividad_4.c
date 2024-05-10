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
 * | 24/04/2024 | Document creation		                         |
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
#include "gpio_mcu.h"
#include "timer_mcu.h"
#include "uart_mcu.h"
#include "analog_io_mcu.h"
/*==================[macros and definitions]=================================*/
#define TIEMPO_MUESTREO_AD 2000 // fm = 500 Hz --> Tm AD = 2000 us
#define TIEMPO_MUESTREO_DA 4000 // fm = 250 Hz --> Tm DA = 4000 us
#define BUFFER_SIZE 231
/*==================[internal data definition]===============================*/
TaskHandle_t ConversorDA_handle = NULL;
TaskHandle_t ConversorAD_handle = NULL;
uint16_t datosAD;
const char ecg[BUFFER_SIZE] = {
    76, 77, 78, 77, 79, 86, 81, 76, 84, 93, 85, 80,
    89, 95, 89, 85, 93, 98, 94, 88, 98, 105, 96, 91,
    99, 105, 101, 96, 102, 106, 101, 96, 100, 107, 101,
    94, 100, 104, 100, 91, 99, 103, 98, 91, 96, 105, 95,
    88, 95, 100, 94, 85, 93, 99, 92, 84, 91, 96, 87, 80,
    83, 92, 86, 78, 84, 89, 79, 73, 81, 83, 78, 70, 80, 82,
    79, 69, 80, 82, 81, 70, 75, 81, 77, 74, 79, 83, 82, 72,
    80, 87, 79, 76, 85, 95, 87, 81, 88, 93, 88, 84, 87, 94,
    86, 82, 85, 94, 85, 82, 85, 95, 86, 83, 92, 99, 91, 88,
    94, 98, 95, 90, 97, 105, 104, 94, 98, 114, 117, 124, 144,
    180, 210, 236, 253, 227, 171, 99, 49, 34, 29, 43, 69, 89,
    89, 90, 98, 107, 104, 98, 104, 110, 102, 98, 103, 111, 101,
    94, 103, 108, 102, 95, 97, 106, 100, 92, 101, 103, 100, 94, 98,
    103, 96, 90, 98, 103, 97, 90, 99, 104, 95, 90, 99, 104, 100, 93,
    100, 106, 101, 93, 101, 105, 103, 96, 105, 112, 105, 99, 103, 108,
    99, 96, 102, 106, 99, 90, 92, 100, 87, 80, 82, 88, 77, 69, 75, 79,
    74, 67, 71, 78, 72, 67, 73, 81, 77, 71, 75, 84, 79, 77, 77, 76, 76,
};
/*==================[internal functions declaration]=========================*/
/**
 * @brief Función invocada en la interrupción del timer conversionAD
 */
void FuncTimerConversorDA()
{
    vTaskNotifyGiveFromISR(ConversorDA_handle, pdFALSE); /* Envía una notificación a la tarea asociada a medir */
}

void FuncTimerConversorAD()
{
    vTaskNotifyGiveFromISR(ConversorAD_handle, pdFALSE); /* Envía una notificación a la tarea asociada a medir */
}

static void ConversorDA_Task()
{
    uint8_t i = 0;
    while (true)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        AnalogOutputWrite(ecg[i]);
        i++;

        if (i == (BUFFER_SIZE-1))
            i = 0;
    }
}

void EscribirEnMonitor(){
    UartSendString(UART_PC, (char*)UartItoa(datosAD, 10));
    UartSendString(UART_PC, "\r");
}

static void ConversorAD_Task()
{
    while (true)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        AnalogInputReadSingle(CH1, &datosAD);
        EscribirEnMonitor();
    }
}
/*==================[external functions definition]==========================*/
void app_main(void)
{
    /*Inicializacion del timer conversion DA*/
    timer_config_t timer_conversor_DA = {
        .timer = TIMER_A,
        .period = TIEMPO_MUESTREO_DA, // los timers trabajan en microsegundos
        .func_p = FuncTimerConversorDA,
        .param_p = NULL};
    TimerInit(&timer_conversor_DA);

    timer_config_t timer_conversor_AD = {
        .timer = TIMER_B,
        .period = TIEMPO_MUESTREO_AD, // los timers trabajan en microsegundos
        .func_p = FuncTimerConversorAD,
        .param_p = NULL};
    TimerInit(&timer_conversor_AD);

    /*Inicializacion de terminal PC*/
    serial_config_t terminal_PC = {
        .port = UART_PC,
        .baud_rate = 115200,
        .func_p = NULL,
        .param_p = NULL};
    UartInit(&terminal_PC);

    /*Inicializacion entrada analogica*/
    analog_input_config_t conv_AD = {
        .input = CH1,
        .mode = ADC_SINGLE,
        .func_p = NULL,
        .param_p = NULL,
        .sample_frec = 0};
    AnalogInputInit(&conv_AD);

    AnalogOutputInit();

    /* Creacion de tareas */
    xTaskCreate(&ConversorDA_Task, "Conversor DA", 1024, NULL, 5, &ConversorDA_handle);
    xTaskCreate(&ConversorAD_Task, "Conversor AD", 4096, NULL, 5, &ConversorAD_handle);

    /* Inicio del conteo de timers */
    TimerStart(timer_conversor_DA.timer);
    TimerStart(timer_conversor_AD.timer);
}