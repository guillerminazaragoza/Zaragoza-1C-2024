/*! @mainpage Ejercicios 4, 5 y 6 del Proyecto 1
 *
 * @section genDesc General Description
 *
 * Ejercicio 6:
 * Se escribe una función que muestra por display el valor que recibe.
 * Para ello se reutilizan las funciones creadas en los puntos 4 y 5.
 *
 * @section hardConn Hardware Connection
 *
 * |    Peripheral  |   ESP32   	|
 * |:--------------:|:--------------|
 * | 	PIN_X	 	| 	GPIO_X		|
 *
 *
 * @section changelog Changelog
 *
 * |   Date	    | Description                                    |
 * |:----------:|:-----------------------------------------------|
 * | 03/04/2024 | Document creation		                         |
 *
 * @author Guillermina Zaragoza
 *
 */

/*==================[inclusions]=============================================*/
#include <stdio.h>
#include <stdint.h>
#include "gpio_mcu.h"
/*==================[macros and definitions]=================================*/

/*==================[internal data definition]===============================*/
/**
 * @struct gpioConf_t
 * @brief estructura que representa la configuracion de un pin GPIO (numero y direccion)
 */
typedef struct
{
	gpio_t pin; /*!< GPIO pin number */
	io_t dir;	/*!< GPIO direction '0' IN;  '1' OUT*/
} gpioConf_t;

/*==================[internal functions declaration]=========================*/

/**
 * @fn void convertToBcdArray(uint32_t data, uint8_t digits, uint8_t *bcd_number)
 * @brief convierte un numero decimal a BCD, guardando cada dígito de salida en un arreglo
 * @param data dato numerico que se debe convertir a BCD
 * @param digits cantidad de dígitos del numero
 * @param bcd_number puntero a un arreglo donde se almacenan los N dígitos BCD
 * @return
 */
void convertToBcdArray(uint32_t data, uint8_t digits, uint8_t *bcd_number)
{
	for (uint32_t d = 0; d < digits; d++)
	{
		bcd_number[d] = data % 10;
		data = data / 10;
	}
}

/**
 * @fn void cambiarEstadosGPIO(uint8_t valor, gpioConf_t *arregloGPIO)
 * @brief cambia el estado de cada GPIO a '0' o a '1' según el estado del bit correspondiente en el BCD ingresado
 * @param valor valor binario que determina los estados de los pines GPIO
 * @param arregloGPIO puntero a un arreglo de estructuras gpioConf_t que contiene la configuración de cada GPIO
 * @return
 */
void cambiarEstadosGPIO(uint8_t valor, gpioConf_t *arregloGPIO)
{
	uint8_t mascara = 1;
	for (int i = 0; i < 4; i++)
	{
		if (valor & mascara)
		{								/*!< si la operacion AND entre valor y mascara es distinta de cero */
			GPIOOn(arregloGPIO[i].pin); /*!< el GPIO se setea */
		}
		else
		{								 /*!< si la operacion AND entre valor y  mascara es cero */
			GPIOOff(arregloGPIO[i].pin); /*!< el GPIO se resetea */
		}
		mascara = mascara << 1; /*!< el '1' de la mascara se corre una posicion hacia la izquierda */
	}
}

/**
 * @fn void mostrarEnDisplay(uint32_t dato, uint8_t cantDigitos, gpioConf_t *vectorPines, gpioConf_t *vectorDigitos)
 * @brief muestra por display el valor recibido
 * @param dato es el dato que se va a convertir a BCD y mostrar en pantalla
 * @param cantDigitos cantidad de digitos que tiene dato
 * @param vectorPines puntero a un arreglo de 4 elementos GPIO que representa cada digito del BCD en binario
 * @param vectorDigitos puntero a un arreglo de 3 elementos que contiene la configuracion de los GPIO que seleccionan los digitos del display
 * @return
 */
void mostrarEnDisplay(uint32_t dato, uint8_t cantDigitos, gpioConf_t *vectorPines, gpioConf_t *vectorDigitos)
{
	uint8_t arregloDigitos[3];
	convertToBcdArray(dato, cantDigitos, arregloDigitos);
	for (int i = 0; i < 3; i++){
		cambiarEstadosGPIO(arregloDigitos[i], vectorPines);
		GPIOOn(vectorDigitos[i].pin);
		GPIOOff(vectorDigitos[i].pin);
	}
}

/*==================[external functions definition]==========================*/
void app_main(void)
{
	// vector de estructuras del tipo 'gpioConf_t' de tamaño 4
	gpioConf_t pines[4] = {{GPIO_20, GPIO_OUTPUT}, {GPIO_21, GPIO_OUTPUT}, {GPIO_22, GPIO_OUTPUT}, {GPIO_23, GPIO_OUTPUT}};

	/*!< inicializacion del vector de pines GPIO que representan al codigo BCD */
	for (int i = 0; i < 4; i++)
	{
		GPIOInit(pines[i].pin, pines[i].dir);
	}

	gpioConf_t digitos[3] = {{GPIO_9, GPIO_OUTPUT}, {GPIO_18, GPIO_OUTPUT}, {GPIO_19, GPIO_OUTPUT}};

	/*!< inicializacion del vector de pines GPIO que representan a cada digito */
	for (int i = 0; i < 3; i++)
	{
		GPIOInit(digitos[i].pin, digitos[i].dir);
	}

	mostrarEnDisplay(138, 3, pines, digitos);
}
/*==================[end of file]============================================*/