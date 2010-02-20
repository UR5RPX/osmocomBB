/* OsmocomBB Layer1 initialization */

/* (C) 2010 by Harald Welte <laforge@gnumonks.org>
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

#include <rffe.h>
#include <rf/trf6151.h>
#include <abb/twl3025.h>
#include <calypso/tpu.h>
#include <calypso/tsp.h>
#include <calypso/dsp.h>
#include <calypso/irq.h>

#include <layer1/sync.h>
#include <layer1/l23_api.h>

void layer1_init(void)
{
	struct msgb *msg;

	/* initialize TDMA Frame IRQ driven synchronous L1 */
	l1s_init();
	/* power up the DSP */
	dsp_power_on();

	/* Initialize TPU, TSP and TRF drivers */
	tpu_init();
	tsp_init();
	trf6151_init();

	rffe_init();

#if 0 /* only if RX TPU window is disabled! */
	/* Put TWL3025 in downlink mode (includes calibration) */
	twl3025_downlink(1, 1000);
#endif

	/* issue the TRF and TWL initialization sequence */
	tpu_enq_sleep();
	tpu_enable(1);
	tpu_wait_idle();

	/* Disable RTC interrupt as it causes lost TDMA frames */
	irq_disable(IRQ_RTC_TIMER);

	/* inform l2 and upwards that we are ready for orders */
	msg = l1_create_l2_msg(LAYER1_RESET, 0, 0);
	l1_queue_for_l2(msg);
}