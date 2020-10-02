/* 
 * File:	 usb-cdc.c
 * Author:   Robinson Mittmann (bobmittmann@gmail.com)
 * Target:
 * Comment:
 * Copyright(C) 2011 Bob Mittmann. All Rights Reserved.
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

#define __THINKOS_DBGMON__
#include <thinkos/dbgmon.h>

#define __THINKOS_BOOTLDR__
#include <thinkos/bootldr.h>

#include <thinkos.h>
#include <sys/dcclog.h>

const char * __retstr(uint32_t __ret);

void dbgmon_print_exception(const struct dbgmon_comm * comm, 
							struct thinkos_except * xcpt)
{
#if (THINKOS_ENABLE_MPU)
	uint32_t mmfsr;
#endif
#if (THINKOS_ENABLE_BUSFAULT)
	uint32_t bfsr;
#endif
#if (THINKOS_ENABLE_USAGEFAULT)
	uint32_t ufsr;
#endif
	uint32_t sp;
	uint32_t ret;
	int ipsr;

	dbgmon_printf(comm, " Error %d at ", xcpt->errno);

	ipsr = xcpt->ctx.core.xpsr & 0x1ff;
	if (ipsr == 0) {
		dbgmon_printf(comm, "thread %d", xcpt->active + 1);
	} else if (ipsr > 15) {
		dbgmon_printf(comm, "IRQ %d", ipsr - 16);
	} else {
		switch (ipsr) {
		case CM3_EXCEPT_SVC:
			dbgmon_printf(comm, "SVCall, thread %d", xcpt->active + 1);
			break;
		case CM3_EXCEPT_DEBUG_MONITOR:
			dbgmon_printf(comm, "Monitor");
			break;
		case CM3_EXCEPT_PENDSV:
			dbgmon_printf(comm, "PendSV");
			break;
		case CM3_EXCEPT_SYSTICK:
			dbgmon_printf(comm, "sysTick");
			break;
		}
	}

	dbgmon_printf(comm, ": %s\r\n", thinkos_err_name_lut[xcpt->errno]);

#if ((THINKOS_ENABLE_IDLE_MSP) || (THINKOS_ENABLE_FPU))
	sp = xcpt->ctx.core.sp;
	ret = xcpt->ctx.core.ret;
#else
	ret = 0xffffff00 | (xcpt->ret & 0xff);
	sp = (ret & CM3_EXC_RET_SPSEL) ? xcpt->psp : xcpt->msp;
#endif

	dbgmon_print_context(comm, &xcpt->ctx.core, sp);
						
	dbgmon_printf(comm, " ret=%08x [ %s ] PSP=%08x MSP=%08x\r\n", 
				  ret, __retstr(ret), xcpt->psp, xcpt->msp);

	switch (xcpt->errno) {
#if THINKOS_ENABLE_MPU 
	case THINKOS_ERR_MEM_MANAGE:
		mmfsr = SCB_CFSR_MMFSR_GET(xcpt->cfsr);
		dbgmon_printf(comm, "mmfsr=%02x [", mmfsr);
		if (mmfsr & MMFSR_MMARVALID)
			dbgmon_printf(comm, " MMARVALID");
		if (mmfsr & MMFSR_MLSPERR)
			dbgmon_printf(comm, " MLSPERR");
		if (mmfsr & MMFSR_MSTKERR)
			dbgmon_printf(comm, " MSTKERR");
		if (mmfsr & MMFSR_MUNSTKERR)
			dbgmon_printf(comm, " MUNSTKERR");
		if (mmfsr & MMFSR_DACCVIOL)  
			dbgmon_printf(comm, " DACCVIOL");
		if (mmfsr & MMFSR_IACCVIOL)  
			dbgmon_printf(comm, " IACCVIOL");
		dbgmon_printf(comm, " ]\r\n");
		if (mmfsr & MMFSR_MMARVALID) 
			dbgmon_printf(comm, " Fault address --> %08x\r\n", xcpt->mmfar);
		break;
#endif

#if THINKOS_ENABLE_BUSFAULT
	case THINKOS_ERR_BUS_FAULT:
		bfsr = SCB_CFSR_BFSR_GET(xcpt->cfsr);
		dbgmon_printf(comm, " bfsr=%02x [", bfsr);
		if (bfsr & BFSR_BFARVALID)  
			dbgmon_printf(comm, " BFARVALID");
		if (bfsr & BFSR_LSPERR)
			dbgmon_printf(comm, " LSPERR");
		if (bfsr & BFSR_STKERR)  
			dbgmon_printf(comm, " STKERR");
		if (bfsr & BFSR_UNSTKERR)  
			dbgmon_printf(comm, " UNSTKERR");
		if (bfsr & BFSR_IMPRECISERR)  
			dbgmon_printf(comm, " IMPRECISERR");
		if (bfsr & BFSR_PRECISERR)
			dbgmon_printf(comm, " PRECISERR");
		if (bfsr & BFSR_IBUSERR)  
			dbgmon_printf(comm, " IBUSERR");
		dbgmon_printf(comm, " ]\r\n");
		if (bfsr & BFSR_BFARVALID) 
			dbgmon_printf(comm, " Fault address --> %08x\r\n", xcpt->bfar);
		break;
#endif

#if THINKOS_ENABLE_USAGEFAULT 
	case THINKOS_ERR_USAGE_FAULT: 
		ufsr = SCB_CFSR_UFSR_GET(xcpt->cfsr);
		dbgmon_printf(comm, " ufsr=%04x [", ufsr);
		if (ufsr & UFSR_DIVBYZERO)  
			dbgmon_printf(comm, " DIVBYZERO");
		if (ufsr & UFSR_UNALIGNED)  
			dbgmon_printf(comm, " UNALIGNED");
		if (ufsr & UFSR_NOCP)  
			dbgmon_printf(comm, " NOCP");
		if (ufsr & UFSR_INVPC)  
			dbgmon_printf(comm, " INVPC");
		if (ufsr & UFSR_INVSTATE)  
			dbgmon_printf(comm, " INVSTATE");
		if (ufsr & UFSR_UNDEFINSTR)  
			dbgmon_printf(comm, " UNDEFINSTR");
		dbgmon_printf(comm, " ]\r\n");
		break;
#endif
	}

}

