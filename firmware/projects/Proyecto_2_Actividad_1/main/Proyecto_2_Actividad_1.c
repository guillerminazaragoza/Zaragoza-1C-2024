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
 * | 10/04/2024 | Document creation		                         |
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
/*==================[macros and definitions]=================================*/
#define REFRESCO_MEDICION 1000
#define REFRESCO_TECLAS 50
#define REFRESCO_DISPLAY 100
/*==================[internal data definition]===============================*/
uint16_t distancia = 0;
bool hold = false;
bool on = false;
/*==================[internal functions declaration]=========================*/
static void MedirDistanciaTask(void *pvParameter)
{
    while (true) // bucle infinito. La función permanece activa hasta que el programa se detenga o la tarea se elimine
    {
        if (on)
        {
            distancia = HcSr04ReadDistanceInCentimeters();
        }
        vTaskDelay(REFRESCO_MEDICION / portTICK_PERIOD_MS);
    }
}

static void MostrarTask(void *pvParameter)
{
    while (true) 
    {
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
            if (!hold) // la distancia se actualiza en el display cuando hold es falso. Si es verdadero se muestra el estado almacenado
            {
                LcdItsE0803Write(distancia);
            }
        }
        else
        {
            LcdItsE0803Off();
            LedsOffAll();
        }
        vTaskDelay(REFRESCO_DISPLAY / portTICK_PERIOD_MS);
    }
}

static void TeclasTask(void *pvParameter)
{
    uint8_t teclas;
    while (true)
    {
        teclas = SwitchesRead();
        switch (teclas)
        {
        case SWITCH_1:
            on = !on;
            break;
        case SWITCH_2:
            hold = !hold; // al presionar la tecla 2, el estado de hold se invierte
            break;
        }
        vTaskDelay(REFRESCO_TECLAS / portTICK_PERIOD_MS);
    }
}
/*==================[external functions definition]==========================*/
void app_main(void)
{
    LedsInit();
    LcdItsE0803Init();
    HcSr04Init(GPIO_3, GPIO_2);
    SwitchesInit();
    xTaskCreate(&MedirDistanciaTask, "MedirDistancia", 512, NULL, 5, NULL);
    xTaskCreate(&MostrarTask, "Mostrar_LEDsyDisplay", 512, NULL, 5, NULL);
    xTaskCreate(&TeclasTask, "Teclas", 512, NULL, 5, NULL);
}
