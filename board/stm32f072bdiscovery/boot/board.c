/* board.c - bootloader descriptor and hardware initialization
 * -------
 *
 *   ************************************************************************
 *   **            Company Confidential - For Internal Use Only            **
 *   **          Mircom Technologies Ltd. & Affiliates ("Mircom")          **
 *   **                                                                    **
 *   **   This information is confidential and the exclusive property of   **
 *   ** Mircom.  It is intended for internal use and only for the purposes **
 *   **   provided,  and may not be disclosed to any third party without   **
 *   **                prior written permission from Mircom.               **
 *   **                                                                    **
 *   **                        Copyright 2017-2018                         **
 *   ************************************************************************
 *
 */

/** 
 * @file board.c
 * @author Robinson Mittmann <bobmittmann@gmail.com>
 * @brief Bootloader descriptor and hardware initialization
 * 
 */

#include <sys/stm32f.h>
#include <sys/delay.h>
#include <sys/dcclog.h>
#define __THINKOS_DBGMON__
#include <thinkos/dbgmon.h>
#define __THINKOS_BOOTLDR__
#include <thinkos/bootldr.h>
#include <thinkos.h>
#include <trace.h>
#include <vt100.h>

#include "board.h"
#include "version.h"

void __puts(const char *s)
{
	int rem = __thinkos_strlen(s, 64);

	while (rem) {
		int n = thinkos_console_write(s, rem);
		s += n;
		rem -= n;
	}
}

void io_init(void)
{
	stm32_clk_enable(STM32_RCC, STM32_CLK_GPIOA);
	stm32_clk_enable(STM32_RCC, STM32_CLK_GPIOB);
	stm32_clk_enable(STM32_RCC, STM32_CLK_GPIOC);
	stm32_clk_enable(STM32_RCC, STM32_CLK_GPIOD);
	stm32_clk_enable(STM32_RCC, STM32_CLK_GPIOE);

	/* - USB ----------------------------------------------------*/
	stm32_gpio_af(IO_USB_FS_DP, GPIO_AF14);
	stm32_gpio_af(IO_USB_FS_DM, GPIO_AF14);

	stm32_gpio_mode(IO_USB_FS_DP, ALT_FUNC, PUSH_PULL | SPEED_HIGH);
	stm32_gpio_mode(IO_USB_FS_DM, ALT_FUNC, PUSH_PULL | SPEED_HIGH);

	/* - LEDs ---------------------------------------------------------*/
	stm32_gpio_mode(IO_LED3, OUTPUT, PUSH_PULL | SPEED_LOW);
	stm32_gpio_mode(IO_LED4, OUTPUT, PUSH_PULL | SPEED_LOW);
	stm32_gpio_mode(IO_LED5, OUTPUT, PUSH_PULL | SPEED_LOW);
	stm32_gpio_mode(IO_LED6, OUTPUT, PUSH_PULL | SPEED_LOW);
	stm32_gpio_set(IO_LED3);
	stm32_gpio_set(IO_LED4);
	stm32_gpio_set(IO_LED5);
	stm32_gpio_set(IO_LED6);
}

void board_on_softreset(void)
{
	struct stm32_rcc * rcc = STM32_RCC;

	/* disable all peripherals clock sources except USB_FS, 
	   GPIOA and GPIOB */
	DCC_LOG1(LOG_TRACE, "ahbenr=0x%08x", rcc->ahbenr);
	DCC_LOG1(LOG_TRACE, "apb1enr=0x%08x", rcc->apb1enr);
	DCC_LOG1(LOG_TRACE, "apb2enr=0x%08x", rcc->apb2enr);

	/* disable all peripherals clock sources except USB_FS, 
	   GPIOA and GPIOB */
	rcc->ahbenr |=  RCC_IOPAEN | RCC_IOPBEN | RCC_IOPCEN;
	rcc->apb1enr |= (1 << RCC_USB);
	rcc->apb2enr = 0;

	/* Reset all peripherals except USB_OTG and GPIOA */
	rcc->ahbrstr = ~((1 << RCC_GPIOA) |
					  (1 << RCC_GPIOB) | (1 << RCC_GPIOC)); 
	rcc->apb1rstr = ~(1 << RCC_USB);
	rcc->apb2rstr = ~(0);

	rcc->ahbrstr = 0;
	rcc->apb1rstr = 0;
	rcc->apb2rstr = 0;

	/* Enable USB_OTG and GPIOA, GPIOB and GPIOC */
	rcc->ahbenr =  (1 << RCC_GPIOA) | 
		(1 << RCC_GPIOB) | (1 << RCC_GPIOC); 
	rcc->apb1enr |= (1 << RCC_USB);
	rcc->apb2enr = 0;

	/* reinitialize IO's */
	io_init();

	/* Adjust USB FS interrupts priority */
	cm3_irq_pri_set(STM32_IRQ_USB_FS, MONITOR_PRIORITY);

	/* Enable USB FS interrupts */
	cm3_irq_enable(STM32_IRQ_USB_FS);
}

int board_init(void)
{
	board_on_softreset();

	stm32_gpio_set(IO_LED3);
	stm32_gpio_set(IO_LED4);

	return 0;
}

/* ----------------------------------------------------------------------------
 * Preboot: this task runs once at power up only.
 * It's used to delay booting up the application ... 
 * ----------------------------------------------------------------------------
 */

#define PREBOOT_TIME_SEC 5

int board_preboot_task(void *ptr)
{
	uint32_t tick;

	__puts("- board preboot\r\n");

	/* Time window autoboot */
	for (tick = 0; tick < 4*PREBOOT_TIME_SEC; ++tick) {
		thinkos_sleep(250);

		__puts(".");

		switch (tick & 0x3) {
		case 0:
			stm32_gpio_clr(IO_LED3);
			break;
		case 1:
			stm32_gpio_clr(IO_LED4);
			break;
		case 2:
			stm32_gpio_set(IO_LED3);
			break;
		case 3:
			stm32_gpio_set(IO_LED4);
			break;
		}
	}

	thinkos_sleep(250);
	stm32_gpio_clr(IO_LED3);
	stm32_gpio_clr(IO_LED4);

	return 0;
}

int board_configure_task(void *ptr)
{
	__puts("- board configuration\r\n");
	return 0;
}

int board_selftest_task(void *ptr)
{
	__puts("- board self test\r\n");
	return 0;
}

/* ----------------------------------------------------------------------------
 * This function runs as the main thread's task when the bootloader 
 * fails to run the application ... 
 * ----------------------------------------------------------------------------
 */

int board_default_task(void *ptr)
{
	uint32_t tick;

	__puts("- board default\r\n");

	for (tick = 0;; ++tick) {
		thinkos_sleep(128);

		switch (tick & 0x7) {
		case 0:
			stm32_gpio_clr(IO_LED6);
			break;
		case 1:
			stm32_gpio_clr(IO_LED5);
			break;
		case 2:
			stm32_gpio_set(IO_LED6);
			break;
		case 3:
			stm32_gpio_set(IO_LED5);
			break;
		case 4:
			stm32_gpio_clr(IO_LED3);
			break;
		case 5:
			stm32_gpio_clr(IO_LED4);
			break;
		case 6:
			stm32_gpio_set(IO_LED3);
			break;
		case 7:
			stm32_gpio_set(IO_LED4);
			break;
		}
	}
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

const struct magic_blk thinkos_10_app_magic = {
	.hdr = {
		.pos = 0,
		.cnt = 3},
	.rec = {
		{0xffffffff, 0x0a0de004},
		{0xffffffff, 0x6e696854},
		{0xffffffff, 0x00534f6b},
		{0x00000000, 0x00000000}
		}
};

/* Bootloader board description  
   Bootloader and debugger, memory description  
 */

const struct mem_desc sram_desc = {
	.tag = "RAM",
	.blk = {
		{"STACK",  0x10000000, M_RW, SZ_64K, 1}, /*  CCM - Main Stack */
		{"KERN",   0x20000000, M_RO, SZ_4K, 1},	 /* Bootloader: 4KiB */
		{"DATA",   0x20001000, M_RW, SZ_4K, 27}, /* Application: 108KiB */
		{"SRAM2",  0x2001c000, M_RW, SZ_16K,  1 }, /* SRAM 2: 16KiB */
		{"SRAM3",  0x20020000, M_RW, SZ_64K,  1 }, /* SRAM 3: 64KiB */

		{"", 0x00000000, 0, 0, 0}
		}
};

const struct mem_desc flash_desc = {
	.tag = "FLASH",
	.blk = {
		{"BOOT", 0x08000000, M_RO, SZ_16K, 4},	/* Bootloader: 64 KiB */
		{"CONF", 0x08010000, M_RO, SZ_64K, 1},	/* Configuration: 64 KiB */
		{"APP",  0x08020000, M_RW, SZ_128K, 7},	/* Application:  */
		{"", 0x00000000, 0, 0, 0}
		}
};

const struct mem_desc peripheral_desc = {
	.tag = "PERIPH",
	.blk = {
		{"RTC", 0x40002800, M_RW, SZ_1K, 1}, /* RTC - 1K */
		{"", 0x00000000, 0, 0, 0}
		}
};

/* Bootloader board description  */
const struct thinkos_board this_board = {
	.name = "STM32F4DISCOVERY",
	.desc = "STM32F407VGT6 Microcontroller Kit",
	.hw = {
	       .tag = "STM32F4DISC1",
	       .ver = {.major = 0, .minor = 1}
	       },
	.sw = {
	       .tag = "ThinkOS",
	       .ver = {
		       .major = VERSION_MAJOR,
		       .minor = VERSION_MINOR,
		       .build = VERSION_BUILD}
	       },
	.memory = {
		   .cnt = 3,
		   .flash = &flash_desc,
		   .ram = &sram_desc,
		   .periph = &peripheral_desc},
	.application = {
			.tag = "",
			.start_addr = 0x08020000,
			.block_size = (128 * 7) * 1024,
			.magic = &thinkos_10_app_magic},
	.init = board_init,
	.softreset = board_on_softreset,
	.upgrade = NULL,
	.preboot_task = board_preboot_task,
	.configure_task = board_configure_task,
	.selftest_task = board_selftest_task,
	.default_task = board_default_task
};

#pragma GCC diagnostic pop

const struct dbgmon_comm * custom_comm_init(void)
{
	return NULL;
}
