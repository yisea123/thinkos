/* 
 * File:	 serial-test.c
 * Author:   Robinson Mittmann (bobmittmann@gmail.com)
 * Target:
 * Comment:
 * Copyright(C) 2013 Bob Mittmann. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <sys/stm32f.h>
#include <arch/cortex-m3.h>
#include <sys/delay.h>
#include <sys/serial.h>
#include <sys/param.h>
#include <sys/file.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define __THINKOS_IRQ__
#include <thinkos_irq.h>
#include <thinkos.h>

#include <sys/dcclog.h>

/* GPIO pin description */ 
struct stm32f_io {
	struct stm32_gpio * gpio;
	uint8_t pin;
};

/* ----------------------------------------------------------------------
 * LEDs 
 * ----------------------------------------------------------------------
 */

const struct stm32f_io led_io[] = {
	{ STM32_GPIOC, 1 },
	{ STM32_GPIOC, 14 },
	{ STM32_GPIOC, 7 },
	{ STM32_GPIOC, 8 }
};

void led_on(int id)
{
#if defined(STM32F405)
	stm32_gpio_set(led_io[id].gpio, led_io[id].pin);
#endif
}

void led_off(int id)
{
#if defined(STM32F405)
	stm32_gpio_clr(led_io[id].gpio, led_io[id].pin);
#endif
}

void leds_init(void)
{
#if defined(STM32F405)
	int i;

	for (i = 0; i < 5; ++i) {
		stm32_gpio_mode(led_io[i].gpio, led_io[i].pin,
						 OUTPUT, PUSH_PULL | SPEED_LOW);

		stm32_gpio_clr(led_io[i].gpio, led_io[i].pin);
	}
#endif
}

/* ----------------------------------------------------------------------
 * Console 
 * ----------------------------------------------------------------------
 */

#define USART1_TX STM32_GPIOB, 6
#define USART1_RX STM32_GPIOB, 7

#define UART5_TX STM32_GPIOC, 12
#define UART5_RX STM32_GPIOD, 2


struct file stm32f_uart1_file = {
	.data = STM32_USART1, 
	.op = &stm32_usart_fops 
};

void stdio_init(void)
{
	struct stm32_usart * us = STM32_USART1;
#if defined(STM32F1x)
	struct stm32f_afio * afio = STM32F_AFIO;
#endif

	/* USART1_TX */
	stm32_gpio_mode(USART1_TX, ALT_FUNC, PUSH_PULL | SPEED_LOW);

#if defined(STM32F1X)
	/* USART1_RX */
	stm32_gpio_mode(USART1_RX, INPUT, PULL_UP);
	/* Use alternate pins for USART1 */
	afio->mapr |= AFIO_USART1_REMAP;
#elif defined(STM32F4X)
	stm32_gpio_mode(USART1_RX, ALT_FUNC, PULL_UP);
	stm32_gpio_af(USART1_RX, GPIO_AF7);
	stm32_gpio_af(USART1_TX, GPIO_AF7);
#endif

	stm32_usart_init(us);
	stm32_usart_baudrate_set(us, 115200);
	stm32_usart_mode_set(us, SERIAL_8N1);
	stm32_usart_enable(us);

	stdin = &stm32f_uart1_file;
	stdout = &stm32f_uart1_file;
	stderr = &stm32f_uart1_file;
}

/* ----------------------------------------------------------------------
 * IO
 * ----------------------------------------------------------------------
 */
void io_init(void)
{
	DCC_LOG(LOG_MSG, "Configuring GPIO pins...");

	stm32_gpio_clock_en(STM32_GPIOA);
	stm32_gpio_clock_en(STM32_GPIOB);
	stm32_gpio_clock_en(STM32_GPIOC);

	/* USART1_TX */
	stm32_gpio_mode(UART5_TX, ALT_FUNC, PUSH_PULL | SPEED_LOW);

#if defined(STM32F2X) 
	stm32_gpio_mode(UART5_RX, ALT_FUNC, PULL_UP);
	stm32_gpio_af(UART5_RX, GPIO_AF7);
	stm32_gpio_af(UART5_TX, GPIO_AF7);
#elif defined(STM32F4X)
	stm32_gpio_mode(UART5_RX, ALT_FUNC, PULL_UP);
	stm32_gpio_af(UART5_RX, GPIO_AF8);
	stm32_gpio_af(UART5_TX, GPIO_AF8);
#endif
}

int __attribute__((noreturn)) serial_send_task(struct serial_dev * ser)
{
	int thread_id = thinkos_thread_self();
	char buf[256];
	int i;

	DCC_LOG1(LOG_TRACE, "<%d> started...", thread_id); 
	thinkos_sleep(100);

	for (i = 0; i < 256; ++i) {
		buf[i] = thread_id + ((i % 10) == 0 ? 'A' :'0');
	}

	for (;;) {
		serial_send(ser, buf, 150);
		thinkos_sleep(5);
	}
}

int main(int argc, char ** argv)
{
	struct serial_dev * ser5;
	uint32_t send1_stack[512];
	const struct thinkos_thread_inf send1_inf = {
		.stack_ptr = send1_stack, 
		.stack_size = sizeof(send1_stack), 
		.priority = 32,
		.thread_id = 1, 
		.paused = 0,
		.tag = "SEND1"
	};

	uint32_t send2_stack[512];
	const struct thinkos_thread_inf send2_inf = {
		.stack_ptr = send2_stack, 
		.stack_size = sizeof(send2_stack), 
		.priority = 32,
		.thread_id = 2, 
		.paused = 0,
		.tag = "SEND2"
	};

	uint32_t send3_stack[512];
	const struct thinkos_thread_inf send3_inf = {
		.stack_ptr = send3_stack, 
		.stack_size = sizeof(send3_stack), 
		.priority = 32,
		.thread_id = 3, 
		.paused = 0,
		.tag = "SEND2"
	};


	int i = 0;

	DCC_LOG_INIT();
	DCC_LOG_CONNECT();

	/* calibrate usecond delay loop */
	cm3_udelay_calibrate();

	DCC_LOG(LOG_TRACE, "1. io_init()");
	io_init();

	DCC_LOG(LOG_TRACE, "2. leds_init()");
	leds_init();

	DCC_LOG(LOG_TRACE, "3. thinkos_init()");
	thinkos_init(THINKOS_OPT_PRIORITY(0) | THINKOS_OPT_ID(0));

	DCC_LOG(LOG_TRACE, "4. stdio_init()");
	stdio_init();

	printf("\n\n");
	printf("-----------------------------------------\n");
	printf(" Serial console test\n");
	printf("-----------------------------------------\n");
	printf("\n");

	DCC_LOG(LOG_TRACE, "5. serial init...");
	ser5 = stm32f_uart5_serial_init(115200, SERIAL_8N1);

	DCC_LOG(LOG_TRACE, "6. Serial send threads...");
	thinkos_thread_create_inf((void *)serial_send_task, 
							  (void *)ser5, &send1_inf);
	thinkos_thread_create_inf((void *)serial_send_task, 
							  (void *)ser5, &send2_inf);
	thinkos_thread_create_inf((void *)serial_send_task, 
							  (void *)ser5, &send3_inf);
/*
	thinkos_thread_create_inf((void *)serial_recv_task, 
							  (void *)&ser5, &recv1_inf);
	thinkos_thread_create_inf((void *)serial_recv_task, 
							  (void *)&ser5, &recv2_inf);
*/
	serial_send(ser5, "\r\n\r\n", 4);
	thinkos_sleep(500);

	for (i = 0; ; i++) {
		DCC_LOG1(LOG_TRACE, "%d", i);

		led_on(0);
		led_on(1);
		printf(" - %4d The quick brown fox jumps over the lazy dog!\n", i);
		thinkos_sleep(100);
		led_off(0);
		led_off(1);
		thinkos_sleep(400);

		led_on(2);
		led_on(3);
		thinkos_sleep(100);

		led_off(2);
		led_off(3);

		thinkos_sleep(40000);
	}

	return 0;
}

