/* TDMA Scheduler Implementation */

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
#include <string.h>

#include <debug.h>
#include <gsm.h>

#include <layer1/tdma_sched.h>
#include <layer1/sync.h>

#include <calypso/dsp.h>

static uint8_t wrap_bucket(uint8_t offset)
{
	uint16_t bucket;

	bucket = (l1s.tdma_sched.cur_bucket + offset)
					% ARRAY_SIZE(l1s.tdma_sched.bucket);

	return bucket;
}

/* Schedule an item at 'frame_offset' TDMA frames in the future */
int tdma_schedule(uint8_t frame_offset, tdma_sched_cb *cb, uint16_t p1, uint16_t p2)
{
	struct tdma_scheduler *sched = &l1s.tdma_sched;
	uint8_t bucket_nr = wrap_bucket(frame_offset);
	struct tdma_sched_bucket *bucket = &sched->bucket[bucket_nr];
	struct tdma_sched_item *sched_item;

	if (bucket->num_items >= ARRAY_SIZE(bucket->item)) {
		puts("tdma_schedule bucket overflow\n");
		return -1;
	}

	sched_item = &bucket->item[bucket->num_items++];

	sched_item->cb = cb;
	sched_item->p1 = p1;
	sched_item->p2 = p2;

	return 0;
}

/* Schedule a set of items starting from 'frame_offset' TDMA frames in the future */
int tdma_schedule_set(uint8_t frame_offset, const struct tdma_sched_item *item_set,
		      uint8_t num_items)
{
	struct tdma_scheduler *sched = &l1s.tdma_sched;
	uint8_t bucket_nr = wrap_bucket(frame_offset);
	int i;

	for (i = 0; i < num_items; i++) {
		const struct tdma_sched_item *sched_item = &item_set[i];
		struct tdma_sched_bucket *bucket = &sched->bucket[bucket_nr];

		if (sched_item->cb == NULL) {
			/* advance to next bucket (== TDMA frame) */
			bucket_nr = wrap_bucket(++frame_offset);
			continue;
		}
		/* check for bucket overflow */
		if (bucket->num_items >= ARRAY_SIZE(bucket->item)) {
			puts("tdma_schedule bucket overflow\n");
			return -1;
		}
		/* copy the item from the set into the current bucket item position */
		memcpy(&bucket->item[bucket->num_items++], sched_item, sizeof(*sched_item));
	}

	return num_items;
}

/* Execute pre-scheduled events for current frame */
int tdma_sched_execute(void)
{
	struct tdma_scheduler *sched = &l1s.tdma_sched;
	struct tdma_sched_bucket *bucket;
	uint8_t next_bucket;
	int i, num_events = 0;

	/* determine current bucket */
	bucket = &sched->bucket[sched->cur_bucket];

	/* iterate over items in this bucket and call callback function */
	for (i = 0; i < bucket->num_items; i++) {
		struct tdma_sched_item *item = &bucket->item[i];
		int rc;

		num_events++;

		rc = item->cb(item->p1, item->p2);
		if (rc < 0) {
			printf("Error %d during processing of item %u of bucket %u\n",
				rc, i, sched->cur_bucket);
			return rc;
		}
		/* if the cb() we just called has scheduled more items for the
		 * current TDMA, bucket->num_items will have increased and we
		 * will simply continue to execute them as intended */
	}

	/* clear/reset the bucket */
	bucket->num_items = 0;

	/* advance to the next bucket */
	next_bucket = wrap_bucket(1);
	sched->cur_bucket = next_bucket;

	/* return number of items that we called */
	return num_events;
}

void tdma_sched_reset(void)
{
	struct tdma_scheduler *sched = &l1s.tdma_sched;
	unsigned int bucket_nr;

	for (bucket_nr = 0; bucket_nr < ARRAY_SIZE(sched->bucket); bucket_nr++) {
		struct tdma_sched_bucket *bucket = &sched->bucket[bucket_nr];
		/* current bucket will be reset by iteration code above! */
		if (bucket_nr != sched->cur_bucket)
			bucket->num_items = 0;
	}

	/* Don't reset cur_bucket, as it would upset the bucket iteration code
	 * in tdma_sched_execute() */
}

void tdma_sched_dump(void)
{
	unsigned int i;

	printf("\n(%2u)", l1s.tdma_sched.cur_bucket);
	for (i = 0; i < ARRAY_SIZE(l1s.tdma_sched.bucket); i++) {
		int bucket_nr = wrap_bucket(i);
		struct tdma_sched_bucket *bucket = &l1s.tdma_sched.bucket[bucket_nr];
		printf("%u:", bucket->num_items);
	}
	putchar('\n');
}