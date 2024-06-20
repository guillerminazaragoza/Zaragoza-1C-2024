/*! @mainpage Proyecto Integrador - Liebre electrónica
 *
 * @section genDesc General Description
 *
 * Este proyecto proporciona a los atletas un medio visual para mantener el ritmo durante una carrera mediante 
 * LEDs que se encienden y apagan secuencialmente, señalando el lugar por donde deberían estar para llegar 
 * a tiempo a su objetivo. Se tienen en cuenta las condiciones de humedad y temperatura para ajustar la velocidad.
 *
 * @section hardConn Hardware Connection
 *
 * |    Peripheral  |   ESP32   	|
 * |:--------------:|:--------------|
 * |    Si7007	 	|   GPIO_3      |
 * | 	Vcc Si7007	| 	3V3		    |
 * | 	PWM_1	 	| 	CH1		    |
 * | 	PWM_2	 	| 	CH2		    |
 * | 	NeoPixel 	| 	GPIO_9	    |
 * | 	Vcc NeoPixel| 	5V		    |
 * | 	PIN_X	 	| 	GPIO_X	    |
 * | 	GND	 	    | 	GND		    |
 *
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 15/05/2024 | Document creation		                         |
 *
 * @author Guillermina Zaragoza (guillerminazf@gmail.com)
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "timer_mcu.h"
#include "led.h"
#include "neopixel_stripe.h"
#include "ble_mcu.h"
#include "ws2812b.h"
#include "Si7007.h"
/*==================[macros and definitions]=================================*/
#define TIMER_HyT 5000000 // los timers trabajan en [us]

#define UT_BAJO 10.0 // Umbral bajo de temperatura [°C]
#define UT_ALTO 28.0 // Umbral alto de temperatura [°C]

#define UH_BAJO 30.0 // Umbral bajo de humedad [%]
#define UH_ALTO 80.0 // Umbral alto de humedad [%]

#define NUM_LEDS 20
/*==================[internal data definition]===============================*/
bool encendido = false;

TaskHandle_t MedirHyT_handle = NULL;
TaskHandle_t ControlHyT_handle = NULL;

float temperatura = 0;
float humedad = 0;

float tiempoIdeal = 20; // Para que se puedan ver los LEDs prendidos al inicio
float tiempoIdealMS = 0;
float tiempoLED = 0;
float distancia = NUM_LEDS / 30.0; // Distancia considerando que hay 30 pixeles por metro
/*==================[internal functions declaration]=========================*/
/**
 * @brief función a ejecutarse ante una interrupción de recepción a través de la conexión BLE.
 * Lee los datos de la aplicación: encendido/apagado y tiempo objetivo del usuario.
 * @param data      Puntero a arreglo de datos recibidos
 * @param length    Longitud del arreglo de datos recibidos
 */
void leerDatos(uint8_t *data, uint8_t length)
{
    uint8_t i = 1;

    if (data[0] == 'O')
    {
        encendido = true;
        printf("Se prende\n");
    }

    else if (data[0] == 'o')
    {
        encendido = false;
        printf("Se apaga\n");
    }

    if (encendido)
    {
        if (data[0] == 'M') 
        {
            printf("Lee tiempo: ");
            /* El cuadro de texto envía los datos con el formato "M" + value + "m" */
            while (data[i] != 'm')
            {
                /* Convertir el valor ASCII a un valor entero */
                tiempoIdeal = tiempoIdeal * 10;
                tiempoIdeal = tiempoIdeal + (data[i] - '0');
                i++;
            }
            printf("%.2f segundos\n", tiempoIdeal);

            tiempoIdealMS = tiempoIdeal * 1000; // paso a milisegundos el dato leido
            tiempoLED = tiempoIdealMS / NUM_LEDS;

            char cadena_tiempo[128];
            strcpy(cadena_tiempo, "");
            sprintf(cadena_tiempo, "*d%.2f\r\n*", tiempoIdeal);
            BleSendString(cadena_tiempo);

            tiempoIdeal = 0;
        }
    }
}

/**
 * @brief Función invocada en la interrupción del timer A
 */
void funcTimer_HyT()
{
    vTaskNotifyGiveFromISR(MedirHyT_handle, pdFALSE); /* Envía una notificación a la tarea asociada */
    vTaskNotifyGiveFromISR(ControlHyT_handle, pdFALSE);
}

/**
 * @brief Tarea encargada de medir humedad y temperatura mediante el sensor Si7007 
 * y enviarlos a la aplicación para ser visualizados
 */
static void MedirHyT_Task()
{
    while (true)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (encendido)
        {
            temperatura = Si7007MeasureTemperature();
            humedad = Si7007MeasureHumidity();
            printf("Temperatura: %.2f°C \nHumedad: %.2f%%\n\n", temperatura, humedad);

            char cadena_temp[128];
            strcpy(cadena_temp, "");
            sprintf(cadena_temp, "*T%.2f\n*", temperatura);
            BleSendString(cadena_temp);

            char cadena_hum[128];
            strcpy(cadena_hum, "");
            sprintf(cadena_hum, "*H%.2f\n*", humedad);
            BleSendString(cadena_hum);
        }
    }
}

/**
 * @brief Tarea encargada de controlar el tiempo de retardo entre encencido y apagado de los LEDs,
 * teniendo en cuenta las condiciones ambientales. Luego envía las variables calculadas a la aplicación.
 */
static void ControlHyT_Task()
{
    uint8_t factor = 1; // Factor de corrección del tiempo de encendido entre cada LED

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(300)); // Sin esto no anda

        float tiempoTotal = 0;
        float tiempoProm = 0;
        float velocidad = 0;

        if (encendido)
        {
            for (int i = 0; i < NUM_LEDS; i++)
            {
                if (((temperatura > UT_BAJO) && (temperatura < UT_ALTO)) && ((humedad > UH_BAJO) && (humedad < UH_ALTO)))
                {
                    factor = 1;
                }
                else if ((temperatura < UT_BAJO) || (temperatura > UT_ALTO))
                {
                    factor = 1.5;
                }
                else if ((humedad < UH_BAJO) || (humedad > UH_ALTO))
                {
                    factor = 2; // Las condiciones de humedad afectan más que las de temperatura
                }
                if (i % 2 == 0)
                {
                    NeoPixelSetPixel(i, NEOPIXEL_COLOR_RED);
                    vTaskDelay(pdMS_TO_TICKS(tiempoLED * factor));
                    tiempoTotal += (tiempoLED * factor); // Calculo el tiempo total real (va a cambiar si fue afectado por las condiciones ambientales)
                    NeoPixelSetPixel(i, NeoPixelRgb2Color(0, 0, 0));
                }
                else
                {
                    NeoPixelSetPixel(i, NEOPIXEL_COLOR_BLUE);
                    vTaskDelay(pdMS_TO_TICKS(tiempoLED * factor));
                    tiempoTotal += (tiempoLED * factor);
                    NeoPixelSetPixel(i, NeoPixelRgb2Color(0, 0, 0));
                }
            }
            NeoPixelAllOff();

            tiempoTotal *= 0.001;                // al valor en milisegundos lo muestro como segundos
            tiempoProm = tiempoTotal / NUM_LEDS;
            velocidad = distancia / tiempoTotal;

            /* Mostrar los datos por pantalla */
            printf("Tiempo promedio (por LED): %.2f s\n", tiempoProm);
            printf("Tiempo total: %.2f s\n", tiempoTotal);
            printf("Velocidad en la vuelta: %.5f m/s\n\n", velocidad);

            /* Mostrar los datos por la aplicación */
            char cadena_vel[128];
            strcpy(cadena_vel, "");
            sprintf(cadena_vel, "*V%.2f\n*", velocidad);
            BleSendString(cadena_vel);

            char cadena_tiempoProm[128];
            strcpy(cadena_tiempoProm, "");
            sprintf(cadena_tiempoProm, "*P%.2f\n*", tiempoProm);
            BleSendString(cadena_tiempoProm);

            char cadena_tiempoTotal[128];
            strcpy(cadena_tiempoTotal, "");
            sprintf(cadena_tiempoTotal, "*R%.2f\n*", tiempoTotal);
            BleSendString(cadena_tiempoTotal);
        }
    }
}
/*==================[external functions definition]==========================*/
void app_main(void)
{
    /* Inicialización de BLE */
    ble_config_t configuracionBLE = {
        "Liebre Electrónica",
        leerDatos // Funcion de interrupción. 
    };
    BleInit(&configuracionBLE);

    /* Inicialización del sensor Si7007 */
    Si7007_config sensorHyT = {
        .select = GPIO_3,
        .PWM_1 = CH1,
        .PWM_2 = CH2};
    Si7007Init(&sensorHyT);

    /* Inicialización de LEDs*/
    LedsInit();

    /* Inicializacion del timer */
    timer_config_t timer_HyT = {
        .timer = TIMER_A,
        .period = TIMER_HyT, 
        .func_p = funcTimer_HyT,
        .param_p = NULL};
    TimerInit(&timer_HyT);

    /* Creación de tareas */
    xTaskCreate(&MedirHyT_Task, "Medir humedad y temperatura", 4096, NULL, 5, &MedirHyT_handle);
    xTaskCreate(&ControlHyT_Task, "Controlar las variables", 4096, NULL, 5, &ControlHyT_handle);
    
    /* Inicialización del conteo del timer */
    TimerStart(timer_HyT.timer);

    /* Inicialización de la tira NeoPixel */
    static neopixel_color_t tiraLed[NUM_LEDS];
    NeoPixelInit(GPIO_9, NUM_LEDS, &tiraLed);
}
/*==================[end of file]============================================*/