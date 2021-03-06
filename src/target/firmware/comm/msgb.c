/* (C) 2008-2010 by Harald Welte <laforge@gnumonks.org>
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
#include <sys/types.h>

#include <debug.h>
#include <delay.h>

#include <osmocore/msgb.h>

#include <calypso/backlight.h>

#define NO_TALLOC

void *tall_msgb_ctx;

#ifdef NO_TALLOC
/* This is a poor mans static allocator for msgb objects */
#define MSGB_DATA_SIZE	256+4
#define MSGB_NUM	32
struct supermsg {
	uint8_t allocated;
	struct msgb msg;
	uint8_t buf[MSGB_DATA_SIZE];
};
static struct supermsg msgs[MSGB_NUM];
void *_talloc_zero(void *ctx, unsigned int size, const char *name)
{
	unsigned int i;
	if (size > sizeof(struct msgb) + MSGB_DATA_SIZE)
		goto panic;

	while (1) {
		for (i = 0; i < ARRAY_SIZE(msgs); i++) {
			if (!msgs[i].allocated) {
				msgs[i].allocated = 1;
				memset(&msgs[i].msg, 0, sizeof(&msgs[i].msg));
				memset(&msgs[i].buf, 0, sizeof(&msgs[i].buf));
				return &msgs[i].msg;
			}
		}
		cons_puts("unable to allocate msgb\n");
		bl_level(++i % 50);
		delay_ms(50);
	}
panic:
	return NULL;
}
void talloc_free(void *msg)
{
	struct supermsg *smsg = container_of(msg, struct supermsg, msg);
	smsg->allocated = 0;
}
#endif
