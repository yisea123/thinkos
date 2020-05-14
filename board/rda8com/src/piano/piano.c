/* 
 * Copyright(C) 2020 Robinson Mittmann. All Rights Reserved.
 * 
 * This file is part of the usb-serial converter.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3.0 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You can receive a copy of the GNU Lesser General Public License from 
 * http://www.gnu.org/
 */

/** 
 * @file piano.c
 * @brief
 * @author Robinson Mittmann <bobmittmann@gmail.com>
 */ 

#include "board.h"

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <fixpt.h>
#include <sys/param.h>
#include <sys/delay.h>
#include <sys/null.h>
#include <sys/console.h>
#include <thinkos.h>
#include <stdio.h>
#include "dac.h"
#include "spidrv.h"
#include "encoder.h"
#include "addsynth.h"
#include "keyboard.h"

void io_init(void)
{
	stm32_clk_enable(STM32_RCC, STM32_CLK_GPIOC);

	/* Rotary Switch */
	stm32_gpio_set(IO_SW0);
	stm32_gpio_set(IO_SW1);
	stm32_gpio_set(IO_SW2);
	stm32_gpio_set(IO_SW3);
	stm32_gpio_mode(IO_SW0, INPUT, PULL_UP | SPEED_LOW);
	stm32_gpio_mode(IO_SW1, INPUT, PULL_UP | SPEED_LOW);
	stm32_gpio_mode(IO_SW2, INPUT, PULL_UP | SPEED_LOW);
	stm32_gpio_mode(IO_SW3, INPUT, PULL_UP | SPEED_LOW);

	/* SPI */
	stm32_gpio_mode(IO_ICE40_SPI_SS, ALT_FUNC, PUSH_PULL | SPEED_HIGH);
	stm32_gpio_af(IO_ICE40_SPI_SS, ICE40_SPI_AF);
	stm32_gpio_mode(IO_ICE40_SPI_SCK, ALT_FUNC, PUSH_PULL | SPEED_HIGH);
	stm32_gpio_af(IO_ICE40_SPI_SCK, ICE40_SPI_AF);
	stm32_gpio_mode(IO_ICE40_SPI_SDI, ALT_FUNC, PUSH_PULL | SPEED_HIGH);
	stm32_gpio_af(IO_ICE40_SPI_SDI, ICE40_SPI_AF);
	stm32_gpio_mode(IO_ICE40_SPI_SDO, ALT_FUNC, PUSH_PULL | SPEED_HIGH);
	stm32_gpio_af(IO_ICE40_SPI_SDO, ICE40_SPI_AF);

	/* DAC */
	stm32_gpio_mode(IO_DAC1, ANALOG, 0);
}

unsigned int io_sw_val(void)
{
	unsigned int addr;

	addr = (stm32_gpio_stat(IO_SW3) ? 0 : 1) +
	    (stm32_gpio_stat(IO_SW2) ? 0 : 2) +
	    (stm32_gpio_stat(IO_SW1) ? 0 : 4) +
	    (stm32_gpio_stat(IO_SW0) ? 0 : 8);

	return (addr);
}

void stdio_init(void)
{
	FILE * f;

	f = console_fopen();
//	f = null_fopen(0);
	/* initialize STDIO */
	stderr = f;
	stdout = f;
	stdin = f;
}

const struct keyboard_cfg keyboard_piano_cfg = {
	.keymap_cnt = 16,
	.keymap = {
		 [0] = { .key =  1, .code = MIDI_C4},
		 [1] = { .key =  2, .code = MIDI_D4},
		 [2] = { .key =  3, .code = MIDI_E4},
		 [3] = { .key =  4, .code = MIDI_F4},
		 [4] = { .key =  8, .code = MIDI_G4},
		 [5] = { .key =  5, .code = MIDI_A4},
		 [6] = { .key =  6, .code = MIDI_B4},

		 [7] = { .key =  7, .code = MIDI_C5},

		 [8] = { .key =  9, .code = MIDI_D5},
		 [9] = { .key = 10, .code = MIDI_E5},
		[10] = { .key = 11, .code = MIDI_F5},
		[11] = { .key = 12, .code = MIDI_G5},
		[12] = { .key = 13, .code = MIDI_A5},
		[13] = { .key = 14, .code = MIDI_B5},

		[14] = { .key = 15, .code = MIDI_C6},
		[15] = { .key = 16, .code = MIDI_D6}
	}
};

int main(int argc, char ** argv)
{
	struct addsynth_instrument * instr;

	stdio_init();

	printf("\n\r\n\r");
	printf("-------------------\n");
	printf("  JUJU Synthesizer \n");
	printf("-------------------\n");
	printf("\n\r");

	io_init();

	spidrv_master_init(200000);

	printf("DAC init...\n\r");
	thinkos_sleep(10);
	dac_init();

	printf("Keyboard init...\n\r");
	thinkos_sleep(10);
	keyboard_init();

	printf("Keyboard config...\n\r");
	thinkos_sleep(10);
	keyboard_config(&keyboard_piano_cfg);

	printf("Instrument init...\n\r");
	thinkos_sleep(10);
	instr = addsynth_instrument_getinstance();
	addsynth_instr_init(instr);

	printf("Instrument config...\n\r");
	thinkos_sleep(10);
	addsynth_instr_config(instr, &addsynth_piano_cfg);

	printf("Sequencer ...\n\r");
	thinkos_sleep(10);
	dac_start();

	printf("------------------------------------------------------------\n\r");
	thinkos_sleep(10);
	/* sequencer */
	for (;;) {
		int event;
		int opc;
		int arg;
		int32_t code;
		int32_t vel;

		event = keyboard_event_wait();
		(void)event;
		
		opc = KBD_EVENT_OPC(event);
		arg = KBD_EVENT_ARG(event);

		switch (opc) {
		case KBD_EV_KEY_ON:
			code = arg;
			vel = 1;
			addsynth_instr_note_on(instr, code, vel); 
//			printf("<%2d> KEY_ON\n", arg);
			break;
		case KBD_EV_KEY_OFF:
			code = arg;
			vel = 1;
			addsynth_instr_note_off(instr, code, vel); 
//			printf("<%2d> KEY_OFF\n", arg);
			break;

		case KBD_EV_SWITCH_OFF:
			printf("<%2d> SWITCH OFF\n", arg);
			break;
		}
	}

	return 0;
}

