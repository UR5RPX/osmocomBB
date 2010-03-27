/*
 * (C) 2010 by Andreas Eversberg <jolly@eversberg.eu>
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

todo: cell selection and reselection criteria
todo: path loss
todo: downlink signal failure

/* allocate a 05.08 event message */
struct msgb *gsm58_msgb_alloc(int msg_type)
{
	struct msgb *msg;
	struct gsm58_msg *gm;

	msg = msgb_alloc_headroom(GSM58_ALLOC_SIZE, GSM58_ALLOC_HEADROOM,
		"GSM 05.08");
	if (!msg)
		return NULL;

	gm = (struct gsm58_msg *)msgb_put(msg, sizeof(*gm));
	gm->msg_type = msg_type;
	return msg;
}

/* select first/next frequency */
static int gsm58_select_bcch(struct osmocom_ms *ms)
{
	struct gsm_support *s = &ms->support;
	struct gsm58_selproc *sp = &ms->selproc;
	int i, j = 0;

next:
	for (i = 0, i < 1024, i++) {
		if ((sp->ba[i >> 3] & (1 << (i & 7)))) {
			if (j == sp->cur_freq)
				break;
			j++;
		}
	}
	if (i == 1024) {
		struct msgb *nmsg;

		DEBUGP(DRR, "Cycled through all %d frequencies in list.\n", j);
		nmsg = gsm322_msgb_alloc(GSM322_EVENT_NO_CELL_F);
		if (!nmsg)
			return -ENOMEM;
		gsm322_sendmsg(ms, nmsg);
	}

	/* if frequency not supported */
	if (!(s->freq_map[i >> 3] & (1 << (i & 7)))) {
		DEBUGP(DRR, "Frequency %d in list, but not supported.\n", i);
		sp->cur_freq++;
		goto next;
	}

	/* set current BCCH frequency */
	sp->arfcn = i;
	DEBUGP(DRR, "Frequency %d selected, wait for sync.\n", sp->arfcn);
	tx_ph_bcch_req(ms, sp->arfcn);

	/* start timer for synchronizanation */
	gsm58_timer_start(sp, 0, 500000);

	sp->mode = GSM58_MODE_SYNC;

	return 0;
}


/* start normal cell selection: search any channel for given PLMN */
static int gsm58_start_normal_sel(struct osmocom_ms *ms, struct msgb *msg)
{
	struct gsm58_selproc *sp = &ms->selproc;
	struct gsm_support *s = &ms->support;
	struct gsm58_msg *gm = msgb->data;

	/* reset process */
	memset(sp, 0, sizeof(*sp));

	/* use all frequencies from supported frequency map */
	memcpy(sp->ba, s->freq_map, sizeof(sp->ba));

	/* limit process to given PLMN */
	sp->mcc = gm->mcc;
	sp->mnc = gm->mnc;

	/* select first channel */
	gsm58_select_bcch(ms);
}

/* start stored cell selection: search given channels for given PLMN */
static int gsm58_start_stored_sel(struct osmocom_ms *ms, struct msgb *msg)
{
	struct gsm58_selproc *sp = &ms->selproc;
	struct gsm_support *s = &ms->support;
	struct gsm58_msg *gm = msgb->data;

	/* reset process */
	memset(sp, 0, sizeof(*sp));

	/* use all frequencies from given frequency map */
	memcpy(sp->ba, sp->ba, sizeof(sp->ba));

	/* limit process to given PLMN */
	sp->mcc = gm->mcc;
	sp->mnc = gm->mnc;

	/* select first channel */
	gsm58_select_bcch(ms);
}

/* start any cell selection: search any channel for any PLMN */
static int gsm58_start_any_sel(struct osmocom_ms *ms, struct msgb *msg)
{
	struct gsm58_selproc *sp = &ms->selproc;
	struct gsm_support *s = &ms->support;
	struct gsm58_msg *gm = msgb->data;

	/* reset process */
	memset(sp, 0, sizeof(*sp));

	/* allow any cell not barred */
	sp->any = 1;

	/* use all frequencies from supported frequency map */
	memcpy(sp->ba, s->freq_map, sizeof(sp->ba));

	/* select first channel */
	gsm58_select_bcch(ms);
}

/* timeout while selecting BCCH */
static int gsm58_sel_timeout(struct osmocom_ms *ms, struct msgb *msg)
{
	struct gsm58_selproc *sp = &ms->selproc;

	switch(sp->mode) {
	case GSM58_MODE_SYNC:
		/* if no sync is received from radio withing sync time */
		DEBUGP(DRR, "Syncronization failed, selecting next frq.\n");
		break;
	case GSM58_MODE_READ:
		/* timeout while reading BCCH */
		DEBUGP(DRR, "BCC reading failed, selecting next frq.\n");
		break;
	default:
		DEBUGP(DRR, "Timeout in wrong mode, please fix!\n");
	}

	sp->cur_freq++;
	gsm58_select_bcch(ms);

	return 0;
}

/* we are synchronized to selecting BCCH */
static int gsm58_sel_sync(struct osmocom_ms *ms, struct msgb *msg)
{
	struct gsm58_selproc *sp = &ms->selproc;
	struct gsm58_msg *gm = (struct gsm58_msg *)msgb->data;

	/* if we got a sync while already selecting a new frequency */
	if (gm->arfcn != sp->arfcn) {
		DEBUGP(DRR, "Requested frq %d, but synced to %d, ignoring.\n"
			sp->arfcn, gm->arfcn);
		return 0;
	}

	DEBUGP(DRR, "Synced to %d, waiting for relevant data.\n", sp->arfcn);

	/* set timer for reading BCCH */
	gsm58_timer_start(sp, 4, 0); // TODO: timer depends on BCCH configur.

	/* reset sysinfo and wait for relevant data */
	gsm_sysinfo_init(ms);
	sp->mode = GSM58_MODE_READ;

	return 0;
}

/* we are getting sysinfo from BCCH */
static int gsm58_sel_sysinfo(struct osmocom_ms *ms, struct msgb *msg)
{
	struct gsm58_selproc *sp = &ms->selproc;
	struct gsm58_msg *gm = (struct gsm58_msg *)msgb->data;
	struct msgb *nmsg;
	int barred = 0, i;

	/* ignore system info, if not synced */
	if (sp->mode != GSM58_MODE_DATA && sp->mode != GSM58_MODE_CAMP)
		return 0;

	/* check if cell is barred for us */
	if (!subscr->barred_access && s->cell_barred)
		barred = 1;
	if (sp->any) {
		if (s->class_barr[10])
			barred = 1;
	} else {
		for (i = 0, i <= 15, i++)
			if (subscr->class_access[i] && !s->class_barr[i])
				break;
		if (i > 15)
			barred = 1;
	}


	/* cell has become barred */
	if (sp->mode == GSM58_MODE_CAMP) {
		if (barred) {
			DEBUGP(DRR, "Cell has become barred, starting "
				"reselection.\n");

			sp->mode = GSM58_MODE_IDLE;

			nmsg = gsm322_msgb_alloc(GSM322_EVENT_CELL_RESEL);
			if (!nmsg)
				return -ENOMEM;
			gsm322_sendmsg(ms, nmsg);

			return 0;
		}

		return 0;
	}

	/* can't use barred cell */
	if (barred) {
		DEBUGP(DRR, "Selected cell is barred, select next.\n");
		gsm58_timer_stop(sp);
		sp->cur_freq++;
		gsm58_select_bcch(ms);

		return 0;
	}

	/* if PLMN not yet indicated */
	if (!s->mcc && !s->mnc)
		return 0;

	// todo: what else do we need until we can camp?

	/* all relevant informations received */
	gsm58_timer_stop(sp);
	sp->mode = GSM58_MODE_CAMP;

	DEBUGP(DRR, "Cell with freq %d, mcc = %d, mnc = %d selected.\n",
		sp->arfcn, s->mcc, s->mnc);

	nmsg = gsm322_msgb_alloc(GSM322_EVENT_CELL_FOUND);
	if (!nmsg)
		return -ENOMEM;
	gsm322_sendmsg(ms, nmsg);

	return 0;
}

/* receive events for GSM 05.08 processes */
static int gsm58_event(struct osmocom_ms *ms, struct msgb *msg)
{
	struct gsm58_msg *gm = (struct gsm58_msg *)msgb->data;
	int msg_type = gm->msg_type;

	DEBUGP(DRR, "(ms %s) Message '%s' for link control "
		"%s\n", ms->name, gsm58_event_names[msg_type],
		plmn_a_state_names[plmn->state]);

	switch(msg_type) {
	case GSM58_EVENT_START_NORMAL:
		gsm58_start_normal_sel(ms, msg);
		break;
	case GSM58_EVENT_START_STORED:
		gsm58_start_stored_sel(ms, msg);
		break;
	case GSM58_EVENT_START_ANY:
		gsm58_start_any_sel(ms, msg);
		break;
	case GSM58_EVENT_TIMEOUT:
		gsm58_sel_timeout(ms, msg);
		break;
	case GSM58_EVENT_SYNC:
		gsm58_sel_sync(ms, msg);
		break;
	case GSM58_EVENT_SYSINFO:
		gsm58_sel_sysinfo(ms, msg);
		break;
	default:
		DEBUGP(DRR, "Message unhandled.\n");
	}

	free_msgb(msg);

	return 0;
}


static void gsm58_timer_timeout(void *arg)
{
	struct gsm322_58_selproc *sp = arg;
	struct msgb *nmsg;

	/* indicate BCCH selection T timeout */
	nmsg = gsm58_msgb_alloc(GSM58_EVENT_TIMEOUT);
	if (!nmsg)
		return -ENOMEM;
	gsm58_sendmsg(ms, nmsg);
}

static void gsm58_timer_start(struct gsm58_selproc *sp, int secs, int micro)
{
	DEBUGP(DRR, "starting FREQUENCY search timer\n");
	sp->timer.cb = gsm58_timer_timeout;
	sp->timer.data = sp;
	bsc_schedule_timer(&sp->timer, secs, micro);
}

static void gsm58_timer_stop(struct gsm58_selproc *plmn)
{
	if (timer_pending(&sp->timer)) {
		DEBUGP(DRR, "stopping pending timer\n");
		bsc_del_timer(&sp->timer);
	}
}