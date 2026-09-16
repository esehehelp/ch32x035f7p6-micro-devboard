/*
 * pins_arduino.h — Pin mapping for CH32X035F7P6 Micro Devboard.
 *
 * CH32X035F7P6 (TSSOP20) available GPIO:
 *   PA0-PA7, PB1, PB12, PC1, PC3, PC18(SWDIO), PC19(SWCLK)
 *   PC14/15/16/17 reserved for USB CDC (CC1, CC2, D-, D+)
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef pins_arduino_h
#define pins_arduino_h

#define NUM_DIGITAL_PINS    14
#define NUM_ANALOG_INPUTS   8

/* Stable Arduino numbers. The board silk uses MCU port names, so sketches can
 * use either PA0/PC1/etc. or D0/D10/etc. without guessing numeric values. */
#define PIN_PA0             0
#define PIN_PA1             1
#define PIN_PA2             2
#define PIN_PA3             3
#define PIN_PA4             4
#define PIN_PA5             5
#define PIN_PA6             6
#define PIN_PA7             7
#define PIN_PB1             8
#define PIN_PB12            9
#define PIN_PC1             10
#define PIN_PC3             11
#define PIN_PC18            12
#define PIN_PC19            13

#define PA0                 PIN_PA0
#define PA1                 PIN_PA1
#define PA2                 PIN_PA2
#define PA3                 PIN_PA3
#define PA4                 PIN_PA4
#define PA5                 PIN_PA5
#define PA6                 PIN_PA6
#define PA7                 PIN_PA7
#define PB1                 PIN_PB1
#define PB12                PIN_PB12
#define PC1                 PIN_PC1
#define PC3                 PIN_PC3
#define PC18                PIN_PC18
#define PC19                PIN_PC19

#define D0                  PIN_PA0
#define D1                  PIN_PA1
#define D2                  PIN_PA2
#define D3                  PIN_PA3
#define D4                  PIN_PA4
#define D5                  PIN_PA5
#define D6                  PIN_PA6
#define D7                  PIN_PA7
#define D8                  PIN_PB1
#define D9                  PIN_PB12
#define D10                 PIN_PC1
#define D11                 PIN_PC3
#define D12                 PIN_PC18
#define D13                 PIN_PC19

/* Board functions. PC3 is shared by the LED, RESET switch, and the MCU's
 * NRST/PC3 pad. PC18/PC19 remain usable as GPIO but doing so occupies SWD. */
#define LED_BUILTIN         PIN_PC3
#define PIN_LED             PIN_PC3
#define PIN_SWDIO           PIN_PC18
#define PIN_SWCLK           PIN_PC19

/* Peripheral-capable pins exposed by this package. These names describe the
 * hardware routing; SPI/UART classes are not implemented in this minimal core. */
#define PIN_SPI_SS          PIN_PA4
#define PIN_SPI_SCK         PIN_PA5
#define PIN_SPI_MISO        PIN_PA6
#define PIN_SPI_MOSI        PIN_PA7
#define PIN_HARDWARE_SERIAL_TX PIN_PA2
#define PIN_HARDWARE_SERIAL_RX PIN_PA3

/*
 * Arduino pin  ->  GPIO
 *   D0  = PA0       (A0)
 *   D1  = PA1       (A1)
 *   D2  = PA2       (A2)
 *   D3  = PA3       (A3)
 *   D4  = PA4       (A4)
 *   D5  = PA5       (A5)
 *   D6  = PA6       (A6)
 *   D7  = PA7       (A7)
 *   D8  = PB1
 *   D9  = PB12
 *   D10 = PC1
 *   D11 = PC3       (LED)
 *   D12 = PC18      (SWDIO — usable as GPIO)
 *   D13 = PC19      (SWCLK — usable as GPIO)
 */

/* Analog pin aliases */
#define A0  PIN_PA0
#define A1  PIN_PA1
#define A2  PIN_PA2
#define A3  PIN_PA3
#define A4  PIN_PA4
#define A5  PIN_PA5
#define A6  PIN_PA6
#define A7  PIN_PA7

#endif /* pins_arduino_h */
