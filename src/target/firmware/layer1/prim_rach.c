/* Layer 1 Random Access Channel Burst */

/* (C) 2010 by Dieter Spaar <spaar@mirider.augusta.de>
 * (C) 2010 by Harald Welte <laforge@gnumonks.org>
 *
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <defines.h>
#include <debug.h>
#include <memory.h>
#include <byteorder.h>
#include <osmocore/gsm_utils.h>
#include <osmocore/msgb.h>
#include <calypso/dsp_api.h>
#include <calypso/irq.h>
#include <calypso/tpu.h>
#include <calypso/tsp.h>
#include <calypso/dsp.h>
#include <calypso/timer.h>
#include <comm/sercomm.h>
#include <asm/system.h>

#include <layer1/sync.h>
#include <layer1/async.h>
#include <layer1/tdma_sched.h>
#include <layer1/tpu_window.h>
#include <layer1/l23_api.h>

#include <l1ctl_proto.h>

struct {
	uint32_t fn;
	uint16_t band_arfcn;
} last_rach;

/* p1: type of operation (0: one NB, 1: one RACH burst, 2: four NB */
static int l1s_tx_rach_cmd(__unused uint8_t p1, __unused uint8_t p2, __unused uint16_t p3)
{
	int i;
	uint16_t  *info_ptr;
	uint8_t data[2];

	putchart('T');

	l1s_tx_apc_helper();

	data[0] = l1s.serving_cell.bsic << 2;
	data[1] = l1s.rach.ra;

	info_ptr = &dsp_api.ndb->d_rach;
	info_ptr[0] = ((uint16_t)(data[0])) | ((uint16_t)(data[1])<<8);

	dsp_api.db_w->d_task_ra = RACH_DSP_TASK;

	l1s_tx_win_ctrl(l1s.serving_cell.arfcn, L1_TXWIN_AB, 0, 3);

	return 0;
}

/* p1: type of operation (0: one NB, 1: one RACH burst, 2: four NB */
static int l1s_tx_rach_resp(__unused uint8_t p1, __unused uint8_t burst_id,
			    __unused uint16_t p3)
{
	putchart('t');

	dsp_api.r_page_used = 1;

	/* schedule a confirmation back indicating the GSM time at which
	 * the RACH burst was transmitted to the BTS */
	last_rach.fn = l1s.current_time.fn - 1;
	last_rach.band_arfcn = l1s.serving_cell.arfcn;
	l1s_compl_sched(L1_COMPL_RACH);

	return 0;
}

/* sched sets for uplink */
const struct tdma_sched_item rach_sched_set_ul[] = {
	SCHED_ITEM_DT(l1s_tx_rach_cmd, 3, 1, 0),	SCHED_END_FRAME(),
							SCHED_END_FRAME(),
	SCHED_ITEM(l1s_tx_rach_resp, -4, 1, 0),		SCHED_END_FRAME(),
	SCHED_END_SET()
};

/* Asynchronous completion handler for FB detection */
static void l1a_rach_compl(__unused enum l1_compl c)
{
	struct msgb *msg;

	msg = l1_create_l2_msg(L1CTL_RACH_CONF, last_rach.fn, 0,
				last_rach.band_arfcn);
	l1_queue_for_l2(msg);
}

/* request a RACH request at the next multiframe T3 = fn51 */
void l1a_rach_req(uint8_t fn51, uint8_t mf_off, uint8_t ra)
{
	uint32_t fn_sched;
	unsigned long flags;

	local_firq_save(flags);
	l1s.rach.ra = ra;
	/* TODO: can we wrap here? I don't think so */
	fn_sched = l1s.current_time.fn - l1s.current_time.t3;
	fn_sched += mf_off * 51;
	fn_sched += fn51;
	sched_gsmtime(rach_sched_set_ul, fn_sched, 0);
	local_irq_restore(flags);

	memset(&last_rach, 0, sizeof(last_rach));
}

static __attribute__ ((constructor)) void prim_rach_init(void)
{
	l1s.completion[L1_COMPL_RACH] = &l1a_rach_compl;
}
