/*! @mainpage Recuperatorio Parcial
 *
 * @section genDesc General Description
 *
 * This section describes how the program works.
 *
 *
 * @section hardConn Hardware Connection
 *
 * |    Peripheral  |   ESP32   	|
 * |:--------------:|:--------------|
 * | 	Termopila	 	| 	CH1		|
 * | 	HCSR04	 	| 	GPIO_2 y GPIO_3		|
 * | 	Alarma	 	| 	GPIO_9		|
 *
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 18/06/2024 | Document creation		                         |
 *
 * @author Guillermina Zaragoza (guillerminazf@gmail.com)
 *
 */
/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <gpio_mcu.h>
#include "analog_io_mcu.h"
#include "uart_mcu.h"
#include "hc_sr04.h"
#include "led.h"
/*==================[macros and definitions]=================================*/
#define TIEMPO_MEDICION_PRESENCIA 1000	// 1000 ms = 1 s
#define TIEMPO_MEDICION_TEMPERATURA 100 // 100 ms entre cada medicion
#define TEMPERATURA_MAXIMA 50			// Tmax que lee el sensor
#define VREF 3300						// 3300 mV = 3.3 V
#define DISTANCIA_MINIMA 8				// Distanciam mínima del rango de medición
#define DISTANCIA_MAXIMA 12				// Distancia máxima del rango de medición
#define DISTANCIA_NUEVA_MEDICION 140	// Distancia límite. Se toman nuevas medidas
#define TEMPERATURA_MAXIMA 37.5			// Temperatura para la cual se prende la alarma
/*==================[internal data definition]===============================*/
float distancia = 0;			// Almacena la distancia medida por el sensor ultrasónico
float temperatura = 0;			// Almacena la temperatura medida por la termopila
float promedio_temperatura = 0; // Almacena el promedio de 10 muestras de temperatura
float Vin = 0;					// Tensión de entrada de la señal analógica
/*==================[internal functions declaration]=========================*/
/**
 * @brief Tarea que mide la distancia a intervalos regulares (de 1 segundo)
 * 		  y prende LEDs para indicar si está dentro o fuera del rango establecido.
 */
static void MedirDistanciaTask(void *pvParameter)
{
	while (true)
	{
		/* Medir la distancia en cm */
		distancia = HcSr04ReadDistanceInCentimeters();

		/* Si está cerca */
		if (distancia < DISTANCIA_MINIMA)
		{
			LedOn(LED_1); /* Prende LED1, apaga LED2 y LED3 */
			LedOff(LED_2);
			LedOff(LED_3);
		}
		/* Si está lejos */
		else if (distancia > DISTANCIA_MAXIMA)
		{
			LedOff(LED_1);
			LedOff(LED_2);
			LedOn(LED_3); /* Prende LED3, apaga LED1 y LED2 */
		}
		/* Si está dentro del rango */
		else
		{
			LedOff(LED_1);
			LedOn(LED_2); /* Prende LED2, apaga LED1 y LED3 */
			LedOff(LED_3);
		}
		LedsOffAll(); // Apago todos los LEDs después de la medición de distancia

		vTaskDelay(pdMS_TO_TICKS(TIEMPO_MEDICION_PRESENCIA)); // Cada 'TIEMPO_MEDICION_PRESENCIA' milisegundos
	}
}

/**
 * @brief Función que escribe los datos de temperatura promedio y distancia y los envía al monitor de la PC por UART
 */
void EscribirEnMonitor()
{
	UartSendString(UART_PC, (char *)UartItoa(promedio_temperatura, 10)); // Envía el valor promediado de la temperatura por UART al monitor de PC
	UartSendString(UART_PC, "°C persona a ");							 // Convierte promedio_temperatura a una cadena de caracteres
	UartSendString(UART_PC, (char *)UartItoa(distancia, 10));			 // Escribe la distancia
	UartSendString(UART_PC, " cm\r\n");									 // Escribe cm y caracteres de salto de línea
}

/**
 * @brief Tarea que lee la temperatura a intervalos regulares (de 100 milisegundos)
 * 		  si la persona está dentro del rango de distancia especificado
 */
static void MedirTemperaturaTask(void *pvParameter)
{
	while (true)
	{
		/* Si está dentro del rango */
		if ((distancia > DISTANCIA_MINIMA) & (distancia < DISTANCIA_MAXIMA))
		{
			/* Lee el valor de la entrada analógica CH1 (en mV) y lo almacena en Vin */
			AnalogInputReadSingle(CH1, &Vin);
			for (int i = 0; i < 10; i++)
			{
				/*Sumar las 10 temperaturas*/
				temperatura += Vin * TEMPERATURA_MAXIMA / VREF;
				// 3.3V representan 50°C
				// Vin representa a temp °C --> temp =  Vin * 50°C / 3.3V
			}
			promedio_temperatura = temperatura / 10; // Calculo promedio
			EscribirEnMonitor();					 // Llamo a la función para que escriba en monitor
		}

		/* Si la persona se mueve más de 140 cm */
		if (distancia > DISTANCIA_NUEVA_MEDICION) 
		{
			temperatura = 0; 						// Vuelvo a inicializar en 0 para otro ciclo de lectura
			promedio_temperatura = 0;
		}

		vTaskDelay(pdMS_TO_TICKS(TIEMPO_MEDICION_TEMPERATURA)); // Cada 'TIEMPO_MEDICION_TEMPERATURA' milisegundos
	}
}

/**
 * @brief función que prende una alarma si la temperatura se excede de un límite. Controlada mediante un GPIO
 */
void EncenderAlarma()
{
	/* Si la temperatura promedio está por debajo de 37.5°C */
	if (promedio_temperatura < TEMPERATURA_MAXIMA)
	{
		GPIOOff(GPIO_9); /* La alarma no se prende --> GPIO_9 en 0*/
	}
	/* Si la temperatura está por encima de 37.5°C */
	else
		GPIOOn(GPIO_9); /* La alarma se prende --> GPIO_9 en 1 */
}
/*==================[external functions definition]==========================*/
void app_main(void)
{
	/* Inicialización */
	LedsInit();					   // Inicialización de LEDs
	HcSr04Init(GPIO_3, GPIO_2);	   // Inicialización del sensor de ultrasonido con los pines especificados
	GPIOInit(CH1, GPIO_INPUT);	   // Inicialización del GPIO que se usa para la termopila
	GPIOInit(GPIO_9, GPIO_OUTPUT); // Inicialización del GPIO que se usa para la alarma

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

	// Creación de tareas
	xTaskCreate(&MedirDistanciaTask, "Medir la distancia", 1024, NULL, 5, NULL);
	xTaskCreate(&MedirTemperaturaTask, "Medir la temperatura", 1024, NULL, 5, NULL);
}
/*==================[end of file]============================================*/