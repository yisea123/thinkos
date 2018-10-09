/* 
 * File:	 thinkos-dbgmon.c
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
#define __THINKOS_IDLE__
#include <thinkos/idle.h>

#if THINKOS_ENABLE_OFAST
_Pragma ("GCC optimize (\"Ofast\")")
#endif
#include <thinkos.h>

#include <sys/stm32f.h>
#include <arch/cortex-m3.h>
#include <sys/param.h>
#include <stdbool.h>

#include <sys/dcclog.h>


#if (THINKOS_ENABLE_MONITOR)

#if (THINKOS_ENABLE_DEBUG_BKPT && !THINKOS_ENABLE_THREAD_VOID)
#error "Need THINKOS_ENABLE_THREAD_VOID"
#endif

#define DBGMON_PERISTENT_MASK ((1 << DBGMON_RESET) | (1 << DBGMON_SOFTRST))

#define NVIC_IRQ_REGS ((THINKOS_IRQ_MAX + 31) / 32)

struct thinkos_dbgmon {
	uint32_t * ctx;           /* monitor context */
	struct dbgmon_comm * comm;
	volatile uint32_t mask;   /* events mask */
	volatile uint32_t events; /* events bitmap */
	void * param;             /* user supplied parameter */
	uint32_t xpsr;
#if THINKOS_DBGMON_ENABLE_IRQ_MGMT
	uint8_t irq_en_lst[4]; /* list of interrupts forced enable */
	uint32_t nvic_ie[NVIC_IRQ_REGS]; /* interrupt state */
#endif
	void (* task)(const struct dbgmon_comm *, void *);
};

struct thinkos_dbgmon thinkos_dbgmon_rt;

uint32_t __attribute__((aligned(8))) thinkos_dbgmon_stack[THINKOS_DBGMON_STACK_SIZE / 4];
const uint16_t thinkos_dbgmon_stack_size = sizeof(thinkos_dbgmon_stack);

int dbgmon_context_swap(uint32_t ** pctx); 

/**
  * __dmon_irq_force_enable:
  *
  * Enable interrupts listed in the IRQ forced enable list.
  */
void __dmon_irq_force_enable(void)
{
#if THINKOS_DBGMON_ENABLE_IRQ_MGMT
	int cnt = 0;

	while (cnt < 4) {
		int irq;
		int i;
		int k;

		/* get the next irq from the list */
		irq = thinkos_dbgmon_rt.irq_en_lst[cnt++];
		i = irq / 32;
		k = irq % 32;

		thinkos_dbgmon_rt.nvic_ie[i] |= (1 << k);
		CM3_NVIC->iser[i] = (1 << k);
	}
#endif
}

/**
  * __dmon_irq_disable_all:
  *
  * Disable all interrupts by clearing the interrupt enable bit
  * of all interrupts on the Nested Vector Interrupt Controller (NVIC).
  *
  * Also the interrupt enable backup is cleared to avoid 
  * interrupts being reenabled by calling __dmon_irq_restore_all().
  *
  * The systick interrupt is not disabled.
  */
static void __dmon_irq_disable_all(void)
{
	int i;

	DCC_LOG(LOG_WARNING, "NIVC all interrupts disabled!!!");

	for (i = 0; i < NVIC_IRQ_REGS; ++i) {
#if THINKOS_DBGMON_ENABLE_IRQ_MGMT
		thinkos_dbgmon_rt.nvic_ie[i] = 0;
#endif
		CM3_NVIC->icer[i] = 0xffffff; /* disable interrupts */
		/* FIXME: clearing the pending interrupt may have a side effect 
		   on the comms irq used by the debug monitor. An alternative 
		   would be to use the force enable list to avoid clearing those
		   in the list. */
#if 0
		CM3_NVIC->icpr[i] = 0xffffff; /* clear pending interrupts */
#endif
	}
}

#if THINKOS_DBGMON_ENABLE_IRQ_MGMT
/**
  * __dmon_irq_pause_all:
  *
  * Save the state of the interrupt enable registers and 
  * disable all interrupts.
  */
void __dmon_irq_pause_all(void)
{
	int i;

	for (i = 0; i < NVIC_IRQ_REGS; ++i) {
		/* save interrupt state */
		thinkos_dbgmon_rt.nvic_ie[i] = CM3_NVIC->iser[i];
		CM3_NVIC->icer[i] = 0xffffffff; /* disable all interrupts */
	}
}

/**
  * __dmon_irq_restore_all:
  *
  * Restore the state of the interrupt enable registers.
  */
void __dmon_irq_restore_all(void)
{
	int i;

	for (i = 0; i < NVIC_IRQ_REGS; ++i) {
		/* restore interrupt state */
		CM3_NVIC->iser[i] = thinkos_dbgmon_rt.nvic_ie[i];
	}

	DCC_LOG(LOG_TRACE, "....");

}
#else
void __dmon_irq_restore_all(void) {
}
void __dmon_irq_pause_all(void) {
}
#endif

#if THINKOS_DBGMON_ENABLE_RST_VEC

/**
 * __reset_ram_vectors:
 *
 * Copy the default values for the IRQ vectors from the flash into RAM. 
 * 
 * When the a new application replaces the existing one through the GDB
 * or Ymodem some interrupts can be fired due to wrong sequencig of
 * interrupt programming in the application. To avoid potential system
 * crashes the vectors should be initialized to a default value.
 *
 */

void __reset_ram_vectors(void)
{
	/* XXX: 
	   this function assumes the exception vectors defaults to be located 
	   just after the .text section! */
	extern unsigned int __text_end;
	extern unsigned int __ram_vectors;
	extern unsigned int __sizeof_ram_vectors;

	unsigned int size = __sizeof_ram_vectors;
	void * src = &__text_end;
	void * dst = &__ram_vectors;

	DCC_LOG3(LOG_MSG, "dst=%08x src=%08x size=%d", dst, src, size); 
	__thinkos_memcpy32(dst, src, size); 
}
#endif

/* -------------------------------------------------------------------------
 * Debug Monitor API
 * ------------------------------------------------------------------------- */

void dbgmon_signal(int sig) 
{
	struct cm3_dcb * dcb = CM3_DCB;
	uint32_t evset;
	uint32_t demcr;
	
	do {
		/* avoid possible race condition on dbgmon.events */
		evset = __ldrex((uint32_t *)&thinkos_dbgmon_rt.events);
		evset |= (1 << sig);
	} while (__strex((uint32_t *)&thinkos_dbgmon_rt.events, evset));

	do {
		demcr = __ldrex((uint32_t *)&dcb->demcr);
		demcr |= DCB_DEMCR_MON_PEND;
	} while (__strex((uint32_t *)&dcb->demcr, demcr));

	asm volatile ("isb\n" :  :  : );
}

bool dbgmon_is_set(int sig) 
{
	return thinkos_dbgmon_rt.events &  (1 << sig) ? true : false;
}

void dbgmon_unmask(int sig)
{
	uint32_t mask;

	do {
		mask = __ldrex((uint32_t *)&thinkos_dbgmon_rt.mask);
		mask |= (1 << sig);
	} while (__strex((uint32_t *)&thinkos_dbgmon_rt.mask, mask));
}

void dbgmon_mask(int sig)
{
	uint32_t mask;

	do {
		mask = __ldrex((uint32_t *)&thinkos_dbgmon_rt.mask);
		mask &= ~(1 << sig);
	} while (__strex((uint32_t *)&thinkos_dbgmon_rt.mask, mask));
}

void dbgmon_clear(int sig)
{
	uint32_t active;
	uint32_t evset;

	do {
		/* avoid possible race condition on dbgmon.events */
		evset = __ldrex((uint32_t *)&thinkos_dbgmon_rt.events);
		evact = evset & (1 << sig);
		evset ^= evact;
	} while (__strex((uint32_t *)&thinkos_dbgmon_rt.events, evset));

	if (evact & DBGMON_PERISTENT_MASK) {
		DCC_LOG(LOG_WARN, "critical signal, swapping...");
		dbgmon_context_swap(&thinkos_dbgmon_rt.ctx); 
	} 
}

/* wait for multiple events, return the highest priority (smaller number)
   don't clear the event upon exiting. */
int dbgmon_select(uint32_t evmsk)
{
	int evset;
	uint32_t save;
	int sig;

	save = thinkos_dbgmon_rt.mask;
	DCC_LOG1(LOG_TRACE, "evmask=%08x ........ ", evmsk);
	/* set the local mask */
	evmsk |= save | DBGMON_PERISTENT_MASK;
	thinkos_dbgmon_rt.mask = evmsk;
	for(;;) {
		evset = thinkos_dbgmon_rt.events;
		/* apply local and global masks, 
		   making sure not to mask non maskable events */
		evset &= evmsk;
		/* select the event with highest priority */
		sig = __clz(__rbit(evset &evmsk));

		if (sig < 32)
			break;

		DCC_LOG2(LOG_TRACE, "waiting for evmsk=%08x sleeping...", 
				 sig, evmsk);
		dbgmon_context_swap(&thinkos_dbgmon_rt.ctx); 
		DCC_LOG(LOG_MSG, "wakeup...");
	} 
	thinkos_dbgmon_rt.mask = save;

	DCC_LOG1(LOG_MSG, "event=%d", sig);

	return sig;
}

/* wait for an event and clear the event */
int dbgmon_expect(int sig)
{
	uint32_t save;
	uint32_t evset;
	uint32_t evmsk;
	uint32_t result;

	save = thinkos_dbgmon_rt.mask;
	/* set the local mask */
	evmsk = save | (1 << sig) | DBGMON_PERISTENT_MASK;
	thinkos_dbgmon_rt.mask = evmsk;

	for (;;) {
		/* avoid possible race condition on dbgmon.events */
		do {
			evset = __ldrex((uint32_t *)&thinkos_dbgmon_rt.events);
			/* apply local and global masks, 
			   making sure not to mask non maskable events */
			result = evset;
			result &= evmsk;
			if (result & (1 << sig))
				evset &= ~(1 << sig); /* clear the event */
		} while (__strex((uint32_t *)&thinkos_dbgmon_rt.events, evset));

		if (result != 0)
			break;

		DCC_LOG2(LOG_TRACE, "waiting for %d (evmsk=%08x) sleeping...", 
				 sig, evmsk);
		dbgmon_context_swap(&thinkos_dbgmon_rt.ctx); 
		DCC_LOG1(LOG_TRACE, "wakeup... evset=%08x", evset);
	}

	thinkos_dbgmon_rt.mask = save;

	if ((result & (1 << sig)) == 0) {
		/* unexpected event received */
		sig = -1;
		DCC_LOG1(LOG_TRACE, "unexpected event=%08x!!", result);
	}

	return sig;
}

/* wait for a single event, clear the event,
 return the event or -1 if another unmasked event 
 is received. */
int dbgmon_wait(int sig)
{
	if ((sig = dbgmon_expect(sig)) < 0)
		return sig;

	return sig;
}

int dbgmon_sleep(unsigned int ms)
{
	dbgmon_clear(DBGMON_ALARM);
#if THINKOS_ENABLE_DMCLOCK
	/* set the clock */
	thinkos_rt.dmclock = thinkos_rt.ticks + ms;
#endif
	/* wait for signal */
	return dbgmon_wait(DBGMON_ALARM);
}

void dbgmon_alarm(unsigned int ms)
{
	dbgmon_clear(DBGMON_ALARM);
	dbgmon_unmask(DBGMON_ALARM);
#if THINKOS_ENABLE_DMCLOCK
	/* set the clock */
	thinkos_rt.dmclock = thinkos_rt.ticks + ms;
#endif
}

void dbgmon_alarm_stop(void)
{
#if THINKOS_ENABLE_DMCLOCK
	/* set the clock in the past so it won't generate a signal */
	thinkos_rt.dmclock = thinkos_rt.ticks - 1;
#endif
	/* make sure the signal is cleared */
	dbgmon_clear(DBGMON_ALARM);
	/* mask the signal */
	dbgmon_mask(DBGMON_ALARM);
}

int dbgmon_wait_idle(void)
{
	int ret;

	/* Issue an idle hook request */
	__idle_hook_req(IDLE_HOOK_NOTIFY_DBGMON);
	DCC_LOG(LOG_TRACE, "idle request");

	/* wait for response */
	if ((ret = dbgmon_wait(DBGMON_IDLE)) < 0)
		return ret;

	DCC_LOG(LOG_TRACE, "[IDLE] zzz zzz zzz zzz");

	return 0;
}

void dbgmon_reset(void)
{
	dbgmon_signal(DBGMON_RESET);
	dbgmon_context_swap(&thinkos_dbgmon_rt.ctx); 
}

void __attribute__((naked)) 
	dbgmon_exec(void (* task)(const struct dbgmon_comm *, void *), 
				void * param)
{
	DCC_LOG1(LOG_MSG, "task=%p", task);
	thinkos_dbgmon_rt.task = task;
	thinkos_dbgmon_rt.param = param;
	dbgmon_reset();
}

#if THINKOS_ENABLE_DEBUG_BKPT

/* -------------------------------------------------------------------------
 * Debug Breakpoint
 * ------------------------------------------------------------------------- */
#define BP_DEFSZ 2
/* (Flash Patch) Number of instruction address comparators */
#define CM3_FP_NUM_CODE 6
/* (Flash Patch) Number of literal address comparators */
#define CM3_FP_NUM_LIT  2

bool dmon_breakpoint_set(uint32_t addr, uint32_t size)
{
	struct cm3_fpb * fpb = CM3_FPB;
	uint32_t comp;
	int i;

	for (i = 0; i < CM3_FP_NUM_CODE; ++i) {
		if ((fpb->comp[i] & COMP_ENABLE) == 0) 
			break;
	}

	if (i == CM3_FP_NUM_CODE) {
		DCC_LOG(LOG_WARNING, "no more breakpoints");
		return false;
	}

	/* use default size if zero */
	size = (size == 0) ? BP_DEFSZ : size;

	if (size == 2) {
		if (addr & 0x00000002) {
			comp = COMP_BP_HIGH | (addr & 0x0ffffffc) | COMP_ENABLE;
		} else {
			comp = COMP_BP_LOW | (addr & 0x0ffffffc) | COMP_ENABLE;
		}
	} else {
		comp = COMP_BP_WORD | (addr & 0x0ffffffc) | COMP_ENABLE;
	}

	fpb->comp[i] = comp;

	DCC_LOG4(LOG_INFO, "bp=%d addr=0x%08x size=%d comp=0x%08x ", i, addr, 
			 size, fpb->comp[i]);

	return true;
}

bool dmon_breakpoint_clear(uint32_t addr, uint32_t size)
{
	struct cm3_fpb * fpb = CM3_FPB;
	uint32_t comp;
	int i;

	size = (size == 0) ? BP_DEFSZ : size;

	if (size == 2) {
		if (addr & 0x00000002) {
			comp = COMP_BP_HIGH | (addr & 0x0ffffffc) | COMP_ENABLE;
		} else {
			comp = COMP_BP_LOW | (addr & 0x0ffffffc) | COMP_ENABLE;
		}
	} else {
		comp = COMP_BP_WORD | (addr & 0x0ffffffc) | COMP_ENABLE;
	}

	DCC_LOG2(LOG_INFO, "addr=0x%08x size=%d", addr, size);

	for (i = 0; i < CM3_FP_NUM_CODE; ++i) {
		if ((fpb->comp[i] | COMP_ENABLE) == comp) {
			fpb->comp[i] = 0;
			return true;
		}
	}

	DCC_LOG1(LOG_WARNING, "breakpoint 0x%08x not found!", addr);

	return false;
}

bool dmon_breakpoint_disable(uint32_t addr)
{
	struct cm3_fpb * fpb = CM3_FPB;
	int i;

	for (i = 0; i < CM3_FP_NUM_CODE; ++i) {
		if ((fpb->comp[i] & 0x0ffffffc) == (addr & 0x0ffffffc)) {
			fpb->comp[i] &= ~COMP_ENABLE;
			return true;
		}
	}

	DCC_LOG1(LOG_WARNING, "breakpoint 0x%08x not found!", addr);

	return false;
}

void dmon_breakpoint_clear_all(void)
{
	struct cm3_fpb * fpb = CM3_FPB;

	__thinkos_memset32(fpb->comp, 0, (CM3_FP_NUM_CODE + CM3_FP_NUM_LIT) * 4);
}

#endif /* THINKOS_ENABLE_DEBUG_BKPT */




#if THINKOS_ENABLE_DEBUG_WPT

/* -------------------------------------------------------------------------
 * Debug Watchpoint
 * ------------------------------------------------------------------------- */

#define DWT_MATCHED            (1 << 24)

#define DWT_DATAVADDR1(ADDR)   ((ADDR) << 16)
#define DWT_DATAVADDR0(ADDR)   ((ADDR) << 12)

#define DWT_DATAVSIZE_BYTE     (0 << 10)
#define DWT_DATAVSIZE_HALFWORD (1 << 10)
#define DWT_DATAVSIZE_WORD     (2 << 10)

#define DWT_LNK1ENA            (1 << 9)
#define DWT_DATAVMATCH         (1 << 8)
#define DWT_CYCMATCH           (1 << 7)
#define DWT_EMITRANGE          (1 << 5)

#define DWT_FUNCTION           (0xf << 0)

#define DWT_DATAV_RO_BKP       (5 << 0)
#define DWT_DATAV_WO_BKP       (6 << 0)
#define DWT_DATAV_RW_BKP       (7 << 0)

#define DWT_DATAV_RO_CMP       (9 << 0)
#define DWT_DATAV_WO_CMP       (10 << 0)
#define DWT_DATAV_RW_CMP       (11 << 0)

#define CM3_DWT_NUMCOMP 4

bool dmon_watchpoint_set(uint32_t addr, uint32_t size, int access)
{
	struct cm3_dwt * dwt = CM3_DWT;
	uint32_t func;
	int i;

	for (i = 0; i < CM3_DWT_NUMCOMP; ++i) {
		if ((dwt->wp[i].function & DWT_FUNCTION) == 0) 
			break;
	}

	if (i == CM3_DWT_NUMCOMP) {
		DCC_LOG(LOG_WARNING, "no more watchpoints");
		return false;
	}

	if (size == 0)
		return false;

	if (size > 4) {
		/* FIXME: implement ranges... */
		return false;
	}

	dwt->wp[i].comp = addr;

	if (size == 4) {
		func = DWT_DATAVSIZE_WORD;
		dwt->wp[i].mask = 2;
	} else if (size == 2) {
		func = DWT_DATAVSIZE_HALFWORD;
		dwt->wp[i].mask = 1;
	} else {
		func = DWT_DATAVSIZE_BYTE;
		dwt->wp[i].mask = 0;
	}

	if (access == 1) {
		func |= DWT_DATAV_RO_BKP;
	} else if (access == 2) {
		func |= DWT_DATAV_WO_BKP;
	} else {
		func |= DWT_DATAV_RW_BKP;
	}

	dwt->wp[i].function = func;

	DCC_LOG3(LOG_TRACE, "wp=%d addr=0x%08x size=%d", i, addr, size);

	return true;
}

bool dmon_watchpoint_clear(uint32_t addr, uint32_t size)
{
	struct cm3_dwt * dwt = CM3_DWT;
	int i;

	DCC_LOG2(LOG_INFO, "addr=0x%08x size=%d", addr, size);

	for (i = 0; i < CM3_DWT_NUMCOMP; ++i) {
		if (((dwt->wp[i].function & DWT_FUNCTION) != 0) && 
			dwt->wp[i].comp == addr) { 
			dwt->wp[i].function = 0;
			dwt->wp[i].comp = 0;
			return true;
		}
	}

	DCC_LOG1(LOG_WARNING, "watchpoint 0x%08x not found!", addr);

	return false;
}


#define DWT_MATCHED            (1 << 24)

#define DWT_DATAVADDR1(ADDR)   ((ADDR) << 16)
#define DWT_DATAVADDR0(ADDR)   ((ADDR) << 12)

#define DWT_DATAVSIZE_BYTE     (0 << 10)
#define DWT_DATAVSIZE_HALFWORD (1 << 10)
#define DWT_DATAVSIZE_WORD     (2 << 10)

#define DWT_LNK1ENA            (1 << 9)
#define DWT_DATAVMATCH         (1 << 8)
#define DWT_CYCMATCH           (1 << 7)
#define DWT_EMITRANGE          (1 << 5)

#define DWT_DATAV_RO_BKP       (5 << 0)
#define DWT_DATAV_WO_BKP       (6 << 0)
#define DWT_DATAV_RW_BKP       (7 << 0)

#define DWT_DATAV_RO_CMP       (9 << 0)
#define DWT_DATAV_WO_CMP       (10 << 0)
#define DWT_DATAV_RW_CMP       (11 << 0)

void dmon_watchpoint_clear_all(void)
{
	struct cm3_dwt * dwt = CM3_DWT;
	int i;
	int n;

	n = (dwt->ctrl & DWT_CTRL_NUMCOMP) >> 28;

	DCC_LOG1(LOG_TRACE, "TWD NUMCOMP=%d.", n);

	for (i = 0; i < n; ++i)
		dwt->wp[i].function = 0;
}

#endif /* THINKOS_ENABLE_DEBUG_WPT */





#if THINKOS_ENABLE_DEBUG_STEP

/* -------------------------------------------------------------------------
 * Thread stepping
 * ------------------------------------------------------------------------- */

int dmon_thread_step(unsigned int thread_id, bool sync)
{
	int ret;

	DCC_LOG2(LOG_TRACE, "step_req=%08x thread_id=%d", 
			 thinkos_rt.step_req, thread_id + 1);

	if (CM3_DCB->dhcsr & DCB_DHCSR_C_DEBUGEN) {
		DCC_LOG(LOG_ERROR, "can't step: DCB_DHCSR_C_DEBUGEN !!");
		return -1;
	}

	if (thread_id == THINKOS_THREAD_VOID) {
		DCC_LOG(LOG_ERROR, "void thread, IRQ step!");
		return -1;
//		dmon_context_swap_ext(&thinkos_dbgmon_rt.ctx, 1); 
	} else {
		if (thread_id >= THINKOS_THREADS_MAX) {
			DCC_LOG1(LOG_ERROR, "thread %d is invalid!", thread_id + 1);
			return -1;
		}

		if (__bit_mem_rd(&thinkos_rt.step_req, thread_id)) {
			DCC_LOG1(LOG_WARNING, "thread %d is step waiting already!", 
					 thread_id + 1);
			return -1;
		}

		DCC_LOG(LOG_MSG, "setting the step_req bit");
		/* request stepping the thread  */
		__bit_mem_wr(&thinkos_rt.step_req, thread_id, 1);
		/* resume the thread */
		__thinkos_thread_resume(thread_id);
		/* make sure to run the scheduler */
		__thinkos_defer_sched();
	}

	if (sync) {
		DCC_LOG(LOG_MSG, "synchronous step, waiting for signal...");
		if ((ret = dbgmon_wait(DBGMON_THREAD_STEP)) < 0)
			return ret;
	}

	return 0;
}

#endif /* THINKOS_ENABLE_DEBUG_STEP */



/* -------------------------------------------------------------------------
 * Debug Monitor Core
 * ------------------------------------------------------------------------- */

static void dbgmon_null_task(const struct dbgmon_comm * comm, void * param)
{
#if DEBUG
	thinkos_dbgmon_rt.mask = 0;

	for (;;) {
		uint32_t buf[64 / 4];
		int n;

		/* Loopback COMM */
		if ((n = dbgmon_comm_recv(comm, buf, sizeof(buf))) > 0) {
			dbgmon_comm_send(comm, buf, n);
		}
	}
#else
	for (;;) {
		dbgmon_context_swap(&thinkos_dbgmon_rt.ctx); 
	}
#endif
}

static void __attribute__((naked, noreturn)) dbgmon_bootstrap(void)
{
	const struct dbgmon_comm * comm = thinkos_dbgmon_rt.comm; 
	void (* dbgmon_task)(const struct dbgmon_comm *, void *);
	void * param = thinkos_dbgmon_rt.param; 

	param = thinkos_dbgmon_rt.param; 

	/* Get the new task */
	dbgmon_task = thinkos_dbgmon_rt.task; 
	/* Set the new task to NULL */
	thinkos_dbgmon_rt.task = dbgmon_null_task;
	
	/* set the clock in the past so it won't generate signals in 
	 the near future */
#if THINKOS_ENABLE_DMCLOCK
	thinkos_rt.dmclock = thinkos_rt.ticks - 1;
#endif

	dbgmon_task(comm, param);

	DCC_LOG(LOG_WARNING, "Debug monitor task returned!");

	dbgmon_reset();
}

	/* exception frame */ 
int thinkos_dbgmon_isr(struct cm3_except_context * ctx)
{
	uint32_t sigset = thinkos_dbgmon_rt.events;
	uint32_t sigmsk = thinkos_dbgmon_rt.mask;
	uint32_t xpsr = ctx->xpsr;
	uint32_t * sp = (uint32_t *)&thinkos_except_buf.ctx.core;

	thinkos_dbgmon_rt.xpsr = xpsr;

#if THINKOS_ENABLE_DEBUG_BKPT
	uint32_t dfsr;

	/* read SCB Debug Fault Status Register */
	if ((dfsr = CM3_SCB->dfsr) != 0) {
		uint32_t demcr;

		/* clear the fault */
		CM3_SCB->dfsr = dfsr;

		DCC_LOG5(LOG_INFO, "DFSR=(EXT=%c)(VCATCH=%c)"
				 "(DWTRAP=%c)(BKPT=%c)(HALT=%c)", 
				 dfsr & SCB_DFSR_EXTERNAL ? '1' : '0',
				 dfsr & SCB_DFSR_VCATCH ? '1' : '0',
				 dfsr & SCB_DFSR_DWTTRAP ? '1' : '0',
				 dfsr & SCB_DFSR_BKPT ? '1' : '0',
				 dfsr & SCB_DFSR_HALTED ? '1' : '0');
	
		demcr = CM3_DCB->demcr;

		DCC_LOG3(LOG_INFO, "DEMCR=(REQ=%c)(PEND=%c)(STEP=%c)", 
				 demcr & DCB_DEMCR_MON_REQ ? '1' : '0',
				 demcr & DCB_DEMCR_MON_PEND ? '1' : '0',
				 demcr & DCB_DEMCR_MON_STEP ? '1' : '0');
#if THINKOS_ENABLE_FPU_LS 
		DCC_LOG3(LOG_TRACE, "FPCCR=%08x FPCAR=%08x CTRL=%01x",
				 CM3_SCB->fpccr, CM3_SCB->fpcar, 
				 cm3_control_get()); 
		if (CM3_SCB->fpccr & SCB_FPCCR_LSPACT) {
			/* Save FP context if lazy flag is enabled. */
			uint32_t fpacr = CM3_SCB->fpcar;
			uint32_t fpscr;
			asm volatile ("vstmia %1!, {s0-s15}\n"
						  "vmrs %0, FPSCR\n"
						  "str  %0, [%1]\n"
						  : "=r" (fpscr) : "r" (fpacr));
			DCC_LOG1(LOG_TRACE, "FPSCR=%08x", fpscr);
			/* Clear LSEN flag, preserving the FP lazy save flag */
			CM3_SCB->fpccr = SCB_FPCCR_ASPEN | SCB_FPCCR_LSPEN;
		}
#endif

		if (dfsr & SCB_DFSR_BKPT) {
			unsigned int insn;
			unsigned int code; 
			uint16_t * pc;
			int ipsr;

			ipsr = xpsr & 0x1ff;
			pc = (uint16_t *)ctx->pc;
			insn = pc[0];
			code = insn & 0x00ff;
			insn &= 0xff00;

			/* this is a breakpoint intruction */
			if ((insn == 0xbe00) && (code > THINKOS_BKPT_EXCEPT_OFF)) {
				int err = code - THINKOS_BKPT_EXCEPT_OFF;
				uint32_t * psp;

				psp = (uint32_t *)cm3_psp_get();

				DCC_LOG4(LOG_ERROR, "<<ERROR %d>>: pc=%08x thread=%d psp=%08x", 
						 err, pc, thinkos_rt.active + 1, psp);
				/* Skip the breakpoint intruction */
				ctx->pc += 2;

				DCC_LOG4(LOG_ERROR, "r0=%08x r1=%08x, r2=%08x r3=%08x", 
						 ctx->r0, ctx->r1, ctx->r2, ctx->r3);
				DCC_LOG4(LOG_ERROR, "r12=%08x lr=%08x, pc=%08x xpsr=%08x", 
						 ctx->r12, ctx->lr, ctx->pc, xpsr);

				/* FIXME: a breakpoint is used to indicate a fault or wrong
				   usage of a system call in thinkOS. */
				sigset |= (1 << DBGMON_THREAD_FAULT);
				sigmsk |= (1 << DBGMON_THREAD_FAULT);
				thinkos_dbgmon_rt.events = sigset;
				/* record the break thread id */
				thinkos_rt.break_id = thinkos_rt.active;
				thinkos_rt.void_ctx = (struct thinkos_context *)sp;
				/* clear IPSR to indicate a thread error */
				thinkos_rt.xcpt_ipsr = 0;
				thinkos_except_buf.active = thinkos_rt.active;
				thinkos_except_buf.type = THINKOS_ERR_OFF + err;
#if (THINKOS_ENABLE_DEBUG_FAULT)
				/* flag the thread as faulty */
				__bit_mem_wr(&thinkos_rt.wq_fault, thinkos_rt.active, 1);
#endif
				/* copy the thread exception context to the exception buffer. */
				__thinkos_memcpy32(&thinkos_except_buf.ctx.core.r0, psp,
								sizeof(struct armv7m_except_frame)); 

				/* suspend the current thread */
				__thinkos_thread_pause(thinkos_rt.active);
				thinkos_rt.active = THINKOS_THREAD_VOID;
				__thinkos_defer_sched();
			} else if ((CM3_SCB->icsr & SCB_ICSR_RETTOBASE) == 0) {
				DCC_LOG3(LOG_ERROR, "<<BREAKPOINT>>: "
						 "except=%d pc=%08x insn=%04x", ipsr, pc, insn);
				DCC_LOG(LOG_ERROR, "invalid breakpoint on exception!!!");
				sigset |= (1 << DBGMON_BREAKPOINT);
				sigmsk |= (1 << DBGMON_BREAKPOINT);
				thinkos_dbgmon_rt.events = sigset;
				/* FIXME: add support for breakpoints on IRQ */

				/* if this is a breakpoint intruction, skip it */
				if (insn == 0xbe00) {
					/* Skip thr breakpoint intruction */
					ctx->pc += 2;
					/* suspend the current thread */
					__thinkos_thread_pause(thinkos_rt.active);
					/* record the break thread id */
					thinkos_rt.break_id = thinkos_rt.active;
					__thinkos_defer_sched();
				} else {
					/* record the break thread id */
					thinkos_rt.break_id = THINKOS_THREAD_VOID;
					thinkos_rt.void_ctx = (struct thinkos_context *)sp;
					thinkos_rt.xcpt_ipsr = ipsr;
#if (THINKOS_ENABLE_FPU) 
					__thinkos_memcpy32(sp, ctx, 
									   sizeof(struct v7m_extended_frame)); 
#else
					__thinkos_memcpy32(sp, ctx, 
									   sizeof(struct v7m_basic_frame)); 
#endif
					__thinkos_pause_all();
					/* diasble all breakpoints */
					dmon_breakpoint_clear_all();
				}
			} else if ((uint32_t)thinkos_rt.active < THINKOS_THREADS_MAX) {
				sigset |= (1 << DBGMON_BREAKPOINT);
				sigmsk |= (1 << DBGMON_BREAKPOINT);
				thinkos_dbgmon_rt.events = sigset;
				DCC_LOG2(LOG_TRACE, "<<BREAKPOINT>>: thread_id=%d pc=%08x ---", 
						 thinkos_rt.active + 1, ctx->pc);
				/* suspend the current thread */
				__thinkos_thread_pause(thinkos_rt.active);
				/* record the break thread id */
				thinkos_rt.break_id = thinkos_rt.active;
				__thinkos_defer_sched();
				/* diasble this breakpoint */
				dmon_breakpoint_disable(ctx->pc);
			} else {
				DCC_LOG2(LOG_ERROR, "<<BREAKPOINT>>: thread_id=%d pc=%08x ---", 
						 thinkos_rt.active + 1, ctx->pc);
				DCC_LOG(LOG_ERROR, "invalid active thread!!!");
				sigset |= (1 << DBGMON_BREAKPOINT);
				sigmsk |= (1 << DBGMON_BREAKPOINT);
				thinkos_dbgmon_rt.events = sigset;
				/* record the break thread id */
				thinkos_rt.break_id = thinkos_rt.active;
				__thinkos_pause_all();
				/* diasble all breakpoints */
				dmon_breakpoint_clear_all();
			}
		}

#if (THINKOS_ENABLE_DEBUG_WPT)
		if (dfsr & SCB_DFSR_DWTTRAP) {
			if ((CM3_SCB->icsr & SCB_ICSR_RETTOBASE) == 0) {
				DCC_LOG2(LOG_ERROR, "<<WATCHPOINT>>: exception=%d pc=%08x", 
						 xpsr & 0x1ff, ctx->pc);
				DCC_LOG(LOG_ERROR, "invalid breakpoint on exception!!!");
				sigset |= (1 << DBGMON_BREAKPOINT);
				sigmsk |= (1 << DBGMON_BREAKPOINT);
				thinkos_dbgmon_rt.events = sigset;
				/* FIXME: add support for breakpoints on IRQ */
				/* record the break thread id */
				thinkos_rt.break_id = thinkos_rt.active;
				__thinkos_pause_all();
			} else if ((uint32_t)thinkos_rt.active < THINKOS_THREADS_MAX) {
				sigset |= (1 << DBGMON_BREAKPOINT);
				sigmsk |= (1 << DBGMON_BREAKPOINT);
				thinkos_dbgmon_rt.events = sigset;
				DCC_LOG2(LOG_TRACE, "<<WATCHPOINT>>: thread_id=%d pc=%08x ---", 
						 thinkos_rt.active + 1, ctx->pc);
				/* suspend the current thread */
				__thinkos_thread_pause(thinkos_rt.active);
				/* record the break thread id */
				thinkos_rt.break_id = thinkos_rt.active;
				__thinkos_defer_sched();
			} else {
				DCC_LOG2(LOG_ERROR, "<<WATCHPOINT>>: thread_id=%d pc=%08x ---", 
						 thinkos_rt.active + 1, ctx->pc);
				DCC_LOG(LOG_ERROR, "invalid active thread!!!");
				sigset |= (1 << DBGMON_BREAKPOINT);
				sigmsk |= (1 << DBGMON_BREAKPOINT);
				thinkos_dbgmon_rt.events = sigset;
				/* record the break thread id */
				thinkos_rt.break_id = thinkos_rt.active;
				__thinkos_pause_all();
			}
		}
#endif /* THINKOS_ENABLE_DEBUG_WPT */

#if THINKOS_ENABLE_DEBUG_STEP
		if (dfsr & SCB_DFSR_HALTED) {
			if (demcr & DCB_DEMCR_MON_STEP) {
				int thread_id = thinkos_rt.step_id;
				/* Restore interrupts. The base priority was
				   set in the scheduler to perform a single step.  */
				cm3_basepri_set(0);

				if ((unsigned int)thread_id < THINKOS_THREADS_MAX) {
					int ipsr = (xpsr & 0x1ff);
					DCC_LOG4(LOG_TRACE, "<<STEP>> thread_id=%d PC=%08x" 
							 " SP=%08x IPSR=%d", thread_id + 1, ctx->pc, 
							 cm3_psp_get(), ipsr);
					if (ipsr != 0) {
						DCC_LOG(LOG_ERROR, "invalid step on exception !!!");
						goto step_done;
					}

					/* suspend the thread, this will clear the 
					   step request flag */
					__thinkos_thread_pause(thread_id);
					/* signal the monitor */
					sigset |= (1 << DBGMON_THREAD_STEP);
					sigmsk |= (1 << DBGMON_THREAD_STEP);
					thinkos_dbgmon_rt.events = sigset;
					__thinkos_defer_sched();
				} else {
					DCC_LOG1(LOG_ERROR, "invalid stepping thread %d !!!", 
							 thread_id + 1);
				}
				thinkos_rt.break_id = thread_id;
step_done:
				CM3_DCB->demcr = demcr & ~DCB_DEMCR_MON_STEP;
			} else {
				DCC_LOG(LOG_WARNING, "/!\\ SCB_DFSR_HALTED /!\\");
			}
		}
#endif /* THINKOS_ENABLE_DEBUG_STEP */
	}
#endif /* THINKOS_ENABLE_DEBUG_BKPT */

	if (sigset & (1 << DBGMON_RESET)) {
		DCC_LOG(LOG_TRACE, "DBGMON_RESET");
		sp = &thinkos_dbgmon_stack[(sizeof(thinkos_dbgmon_stack) / 4) - 10];
		sp[0] = CM_EPSR_T + CM3_EXCEPT_DEBUG_MONITOR; /* CPSR */
		sp[9] = ((uint32_t)dbgmon_bootstrap) | 1; /* LR */
		thinkos_dbgmon_rt.ctx = sp;
		/* clear the RESET event */
		thinkos_dbgmon_rt.events = sigset & ~(1 << DBGMON_RESET);
		/* make sure the RESET event is not masked */
		sigmsk |= (1 << DBGMON_RESET);
	}

	/* Process monitor events */
	if ((sigset & sigmsk) != 0) {
		DCC_LOG1(LOG_MSG, "monitor ctx=%08x", thinkos_dbgmon_rt.ctx);
#if DEBUG
		/* TODO: this stack check is very usefull... 
		   Some sort of error to the developer should be raised or
		 force a fault */
		if (thinkos_dbgmon_rt.ctx < thinkos_dbgmon_stack) {
			DCC_LOG(LOG_PANIC, "stack overflow!");
			DCC_LOG2(LOG_PANIC, "monitor.ctx=%08x dbgmon.stack=%08x", 
					 thinkos_dbgmon_rt.ctx, thinkos_dbgmon_stack);
			DCC_LOG2(LOG_PANIC, "sigset=%08x sigmsk=%08x", sigset, sigmsk);
		}
#endif
		return dbgmon_context_swap(&thinkos_dbgmon_rt.ctx); 
	}

	DCC_LOG2(LOG_WARNING, "Unhandled signal sigset=%08x sigmsk=%08x", 
			 sigset, sigmsk);
	return 0;
}

#if DEBUG
void __attribute__((noinline, noreturn)) 
	dbgmon_panic(struct thinkos_except * xcpt)
{
	asm volatile ("ldmia %0!, {r4-r11}\n"
				  "add   %0, %0, #16\n"
				  "mov   r12, sp\n"
				  "msr   PSP, r12\n"
				  "ldr   r12, [%0]\n"
				  "ldr   r13, [%0, #4]\n"
				  "ldr   r14, [%0, #8]\n"
				  "sub   %0, %0, #16\n"
				  "ldmia %0, {r0-r4}\n"
				  : : "r" (xcpt));
	for(;;);
}
#endif


/* 
 * ThinkOS exception handler hook
 */
#if THINKOS_ENABLE_EXCEPTIONS
void thinkos_exception_dsr(struct thinkos_except * xcpt)
{
	struct thinkos_context * ctx = &xcpt->ctx.core;
	int ipsr;

	ipsr = ctx->xpsr & 0x1ff;
#if THINKOS_ENABLE_DEBUG_BKPT
	thinkos_rt.xcpt_ipsr = ipsr;
#endif
	if ((ipsr == 0) || (ipsr == CM3_EXCEPT_SVC)) {
		DCC_LOG1(LOG_WARNING, "Fault at thread %d !!!", 
				 xcpt->active + 1);
#if THINKOS_ENABLE_DEBUG_BKPT
		thinkos_rt.break_id = xcpt->active;
#endif
		__dmon_irq_disable_all();
#if THINKOS_DBGMON_ENABLE_IRQ_MGMT
		__dmon_irq_force_enable();
#else
		dbgmon_signal(DBGMON_SOFTRST);
#endif
		dbgmon_signal(DBGMON_THREAD_FAULT);
	} else {
#if THINKOS_ENABLE_DEBUG_BKPT
		DCC_LOG1(LOG_ERROR, "Exception at IRQ: %d !!!", 
				 ipsr - 16);
		/* exceptions on IRQ */
		thinkos_rt.break_id = -1;
		thinkos_rt.void_ctx = ctx;
		DCC_LOG2(LOG_WARNING, "VOID context=%08x active=%d!", 
				 thinkos_rt.void_ctx, thinkos_rt.active + 1);

		if (ipsr == CM3_EXCEPT_DEBUG_MONITOR) {
/* FIXME: this is a dire situation, probably the only resource left
   is to restart the system. */
#if DEBUG
			dbgmon_panic(xcpt);
#else
#endif
		} else 
#endif
		{
			__dmon_irq_disable_all();
#if THINKOS_DBGMON_ENABLE_IRQ_MGMT
			__dmon_irq_force_enable();
#else
			dbgmon_signal(DBGMON_SOFTRST);
#endif
			DCC_LOG(LOG_TRACE, "DBGMON_EXCEPT");
			dbgmon_signal(DBGMON_EXCEPT);
		}
	}
}
#endif

/**
 * dmon_soft_reset:
 *
 * Reinitialize the plataform by reseting all ThinkOS subsystems.
 * 
 */

void dbgmon_soft_reset(void)
{
	/* Disable Iterrutps */
	cm3_cpsid_i();

	DCC_LOG(LOG_TRACE, "1. disable all interrupt on NVIC "); 
	__dmon_irq_disable_all();

	DCC_LOG(LOG_TRACE, "2. raise DBGMON_SOFTRST signal");
	dbgmon_signal(DBGMON_SOFTRST);

	/* Issue a system reset request */
	DCC_LOG(LOG_TRACE, "3. stsstem reset request...");
	__idle_hook_req(IDLE_HOOK_SYSRST);

	/* Return to the Monitor applet with the SOFTRST signal set.
	   The applet should clear the hardware and restore the core interrups.
	   __thinkos_system_reset() should finish the process... */

}

#if THINKOS_DBGMON_ENABLE_IRQ_MGMT
/**
  * __dmon_irq_init:
  *
  * Initialize the IRQ track subsystem
  */
static void __dmon_irq_init(void)
{
	int cnt;
	int i;
	
	cnt = 0;
	thinkos_dbgmon_rt.irq_en_lst[0] = 0xff;
	thinkos_dbgmon_rt.irq_en_lst[1] = 0xff;
	thinkos_dbgmon_rt.irq_en_lst[2] = 0xff;
	thinkos_dbgmon_rt.irq_en_lst[3] = 0xff;
	for (i = 0; i < NVIC_IRQ_REGS; ++i) {
		uint32_t mask;
		int k;

		mask = CM3_NVIC->iser[i];
		/* save the current interrupts */
		thinkos_dbgmon_rt.nvic_ie[i] = mask;
		DCC_LOG1(LOG_INFO, "nvic.iser[i]=0x%08x.", mask);

		/* probe interrupts */
		while ((cnt < 4) && (mask != 0)) {
			int irq;

			k = __clz(__rbit(mask));
			mask &= ~(1 << k);

			irq = (i * 32) + k;
			thinkos_dbgmon_rt.irq_en_lst[cnt++] = irq;

			DCC_LOG1(LOG_TRACE, "IRQ %d always enabled.", irq);
		}	
	}
}
#endif

/* -------------------------------------------------------------------------
 * ThinkOS kernel level API
 * ------------------------------------------------------------------------- */

void thinkos_dbgmon_svc(int32_t arg[], int self)
{
	void (* task)(const struct dbgmon_comm *, void *) = (void *)arg[0] ;
	struct dbgmon_comm * comm = (void *)arg[1];
	void * param = (void *)arg[2];
	struct cm3_dcb * dcb = CM3_DCB;
	uint32_t demcr; 
	
	demcr = dcb->demcr;

	/* disable monitor and clear semaphore */
//	dcb->demcr = demcr & ~(DCB_DEMCR_MON_EN | DCB_DEMCR_MON_PEND);

	DCC_LOG2(LOG_TRACE, "comm=%p task=%p", comm, task);
	
	thinkos_dbgmon_rt.events = (1 << DBGMON_RESET) | 
		(((demcr & DCB_DEMCR_MON_EN) == 0) ? (1 << DBGMON_STARTUP) : 0);
	/* Set the persistent signals */
	thinkos_dbgmon_rt.mask = DBGMON_PERISTENT_MASK | (1 << DBGMON_STARTUP);
	thinkos_dbgmon_rt.comm = comm;
	thinkos_dbgmon_rt.task = task;
	thinkos_dbgmon_rt.param = param;
	thinkos_dbgmon_rt.ctx = 0;

	if (thinkos_dbgmon_rt.events & (1 << DBGMON_STARTUP)) {
		DCC_LOG(LOG_INFO, "system startup!!!");
	}

#if THINKOS_DBGMON_ENABLE_IRQ_MGMT
	__dmon_irq_init();
#endif

#if THINKOS_ENABLE_STACK_INIT
	__thinkos_memset32(thinkos_dbgmon_stack, 0xdeadbeef, 
					   sizeof(thinkos_dbgmon_stack));
#endif

#if THINKOS_ENABLE_DEBUG_STEP
	/* clear the step request */
	demcr &= ~DCB_DEMCR_MON_STEP;
	/* enable the FPB unit */
	CM3_FPB->ctrl = FP_KEY | FP_ENABLE;
#endif
	/* enable monitor and send the reset event */
	demcr |= DCB_DEMCR_MON_EN | DCB_DEMCR_MON_PEND;

	dcb->demcr = demcr;
}

#endif /* THINKOS_ENABLE_MONITOR */

