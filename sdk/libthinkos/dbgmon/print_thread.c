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


void dmon_print_thread(struct dmon_comm * comm, unsigned int thread_id)
{
	struct thinkos_rt * rt = &thinkos_rt;
	int32_t timeout;
	uint32_t cyccnt;
	int sched_val;
	int sched_pri;
	int type;
	int tmw;
	int wq;
#if !THINKOS_ENABLE_THREAD_STAT
	int i;
#endif

	if ((thread_id >= THINKOS_THREADS_MAX) || (rt->ctx[thread_id] == NULL)) {
		return;
	}

#if THINKOS_ENABLE_THREAD_STAT
	wq = rt->th_stat[thread_id] >> 1;
	tmw = rt->th_stat[thread_id] & 1;
#else
	for (i = 0; i < THINKOS_WQ_LST_END; ++i) {
		if (rt->wq_lst[i] & (1 << thread_id))
			break;
	}
	if (i == THINKOS_WQ_LST_END)
		return ; /* not found */
	wq = i;
#if THINKOS_ENABLE_CLOCK
	tmw = rt->wq_clock & (1 << thread_id) ? 1 : 0;
#else
	tmw = 0;
#endif
#endif /* THINKOS_ENABLE_THREAD_STAT */

#if THINKOS_ENABLE_TIMESHARE
	sched_val = rt->sched_val[thread_id];
	sched_pri = rt->sched_pri[thread_id]; 
#else
	sched_val = 0;
	sched_pri = 0;
#endif

#if THINKOS_ENABLE_CLOCK
	timeout = (int32_t)(rt->clock[thread_id] - rt->ticks); 
#else
	timeout = -1;
#endif

#if THINKOS_ENABLE_PROFILING
	cyccnt = rt->cyccnt[thread_id];
#else
	cyccnt = 0;
#endif

	type = thinkos_obj_type_get(wq);

	/* Internal thread ids start form 0 whereas user
	   thread numbers start form one ... */
	dmprintf(comm, " - No: %d", thread_id + 1); 
#if THINKOS_ENABLE_THREAD_INFO
	if (rt->th_inf[thread_id])
		dmprintf(comm, ", '%s'", rt->th_inf[thread_id]->tag); 
	else
#endif
		dmprintf(comm, ", '...'"); 

	if (THINKOS_OBJ_READY == type) {
#if THINKOS_IRQ_MAX > 0
		if (thread_id != THINKOS_THREAD_IDLE) {
			int irq;
			for (irq = 0; irq < THINKOS_IRQ_MAX; ++irq) {
				if (thinkos_rt.irq_th[irq] == thread_id) {
					break;
				}
			}
			if (irq < THINKOS_IRQ_MAX) {
				dmprintf(comm, " wait on IRQ[%d]\r\n", irq);
			} else
				dmprintf(comm, " %s.\r\n", thinkos_type_name_lut[type]); 
		} else
#endif
		dmprintf(comm, " %s.\r\n", thinkos_type_name_lut[type]); 
	} else {
		dmprintf(comm, " %swait on %s(%3d)\r\n", 
				 tmw ? "time" : "", thinkos_type_name_lut[type], wq); 
	}

	dmprintf(comm, " - sched: val=%3d pri=%3d - ", 
			 sched_val, sched_pri); 
	dmprintf(comm, " timeout=%8d ms", timeout); 
	dmprintf(comm, " - cycles=%u\r\n", cyccnt); 

	dmon_print_context(comm, rt->ctx[thread_id], (uint32_t)rt->ctx[thread_id]);

	dmprintf(comm, "\r\n");
}


