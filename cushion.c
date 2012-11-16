/*
 * FILE:    cushion.c
 * PROGRAM: RAT
 * AUTHOR:  Isidor Kouvelas
 * MODIFICATIONS: Orion Hodson
 *
 * Copyright (c) 1995-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: cushion.c 3826 2006-09-08 18:55:21Z piers $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "memory.h"
#include "cushion.h"
#include "audio_types.h"
#include "codec_types.h"
#include "audio.h"

/*
 * SAFETY is how safe we want to be with the device going dry. If we want to
 * cover for larger future jumps in workstation delay then SAFETY should be
 * larger. Sensible values are between 0.9 and 1
 */
#define SAFETY		 0.90

#define HISTORY_SIZE	 250
#define MIN_COVER	 ((float)HISTORY_SIZE * SAFETY)
#define CUSHION_MAX_MS	 500
#define CUSHION_MIN_MS	 40
#define CUSHION_STEP_MS  10

/* Initial cushion value is high, but should guarantee no interruptions and
 * will come down during silent periods anyway
 */
#define CUSHION_START_MS (2 * CUSHION_MIN_MS)

/* All cushion measurements are in sampling intervals, not samples ! [oth] */

typedef struct s_cushion_struct {
	uint32_t         cushion_estimate;
	uint32_t         cushion_size;
	uint32_t         cushion_step;
        uint32_t         cushion_min;
        uint32_t         cushion_max;
	uint32_t        *read_history;	/* Circular buffer of read lengths */
	int              last_in;	/* Index of last added value */
	int             *histogram;	/* Histogram of read lengths */
        uint32_t         histbins;      /* Number of bins in histogram */
} cushion_t;

int
cushion_create(cushion_t **c, uint32_t sample_rate)
{
        int i;
        uint32_t cushion_start;
        cushion_t *nc;

        nc = (cushion_t*) xmalloc (sizeof(cushion_t));
        if (nc == NULL) {
                return FALSE;
        }

        /* cushion operates independently of the number of channels */
        nc->cushion_min      = CUSHION_MIN_MS   * sample_rate / 1000;
        nc->cushion_max      = CUSHION_MAX_MS   * sample_rate / 1000;
        cushion_start        = CUSHION_START_MS * sample_rate / 1000;
        nc->cushion_size     = 0;
	nc->cushion_estimate = cushion_start;
	nc->cushion_step     = CUSHION_STEP_MS  * sample_rate / 1000;

	nc->read_history     = (uint32_t *)xmalloc(HISTORY_SIZE * sizeof(uint32_t));
        if (nc->read_history == NULL) {
                xfree(nc);
                return FALSE;
        }

	for (i = 0; i < HISTORY_SIZE; i++) {
                nc->read_history[i] = nc->cushion_estimate / nc->cushion_step;
        }

        nc->histbins  = CUSHION_MAX_MS / CUSHION_STEP_MS + 1;
	nc->histogram = (int *)xmalloc(nc->histbins * sizeof(int));
        if (nc->histogram == NULL) {
                xfree(nc);
                xfree(nc->read_history);
                return FALSE;
        }

	memset(nc->histogram, 0, nc->histbins * sizeof(int));
	nc->histogram[nc->cushion_estimate / nc->cushion_step] = HISTORY_SIZE;
	nc->last_in = 0;

        *c = nc;
        return TRUE;
}

void
cushion_destroy(cushion_t **ppc)
{
        cushion_t *pc;
        assert(ppc);
        pc = *ppc;
        assert(pc);
        xfree(pc->read_history);
        xfree(pc->histogram);
        xfree(pc);
        *ppc = NULL;
}

void
cushion_update(cushion_t *c, uint32_t read_dur, int mode)
{
	uint32_t idx, cnt, cover_idx, cover_cnt;
	uint32_t lower, upper;

	/* remove entry we are about to overwrite from histogram */
	if (c->read_history[c->last_in] < c->histbins) {
		c->histogram[ c->read_history[c->last_in] ]--;
		assert(c->histogram[ c->read_history[c->last_in] ] <= HISTORY_SIZE);
	} else {
		c->histogram[ c->read_history[c->histbins - 1] ]--;
		assert(c->histogram[ c->read_history[c->histbins - 1] ] <= HISTORY_SIZE);
	}

	/* slot in new entry and update histogram */
	c->read_history[c->last_in] = (max(read_dur, c->cushion_min) + c->cushion_step - 1) / c->cushion_step;
	if (c->read_history[c->last_in] < c->histbins) {
		if (c->histogram[ c->read_history[c->last_in] ] < HISTORY_SIZE) {
			c->histogram[ c->read_history[c->last_in] ]++;
		} else
			debug_msg("Limited cushion history: %d, read_dur: %d ", c->histogram[ c->read_history[c->last_in] ], read_dur);
	} else {
		if (c->histogram[ c->read_history[c->histbins - 1] ] < HISTORY_SIZE) {
			c->histogram[ c->read_history[c->histbins - 1] ]++;
		} else 
			debug_msg("NOT Keeping up AND Limiting cushion history: %d", c->histogram[ c->read_history[c->histbins - 1] ]);
		debug_msg("WE ARE NOT KEEPING UP IN REAL-TIME\n");
	}

	c->last_in++;
	if (c->last_in == HISTORY_SIZE) {
		c->last_in = 0;
	}

	/* Find lower and upper bounds for cushion... */
	idx = cnt = cover_idx = cover_cnt = 0;
	while(idx < c->histbins && cnt < HISTORY_SIZE) {
		if (cover_cnt < MIN_COVER) {
			cover_cnt += c->histogram[idx];
			cover_idx  = idx;
		}
		cnt += c->histogram[idx];
		idx++;
	}

	if (mode == CUSHION_MODE_LECTURE) {
		lower = (cover_idx + 10);
		upper = (idx       + 10);
	} else {
		lower = (cover_idx + 2);
		upper = idx;
	}

	/* it's a weird world :D lower can be above upper */
	c->cushion_estimate = min(lower,upper) * c->cushion_step;
	c->cushion_estimate = max(c->cushion_estimate, c->cushion_min);

#ifdef DEBUG_CUSHION
	debug_msg("size % 3d cur % 3d\n", c->cushion_size, c->cushion_estimate);
#endif /* DEBUG_CUSHION */
	assert(c->cushion_estimate != 0);
}

static void
cushion_size_check(cushion_t *c)
{
        c->cushion_size = max(c->cushion_size, c->cushion_min);
        c->cushion_size = min(c->cushion_size, c->cushion_max);
}

uint32_t
cushion_get_size(cushion_t *c)
{
        return c->cushion_size;
}

uint32_t
cushion_step_up(cushion_t *c)
{
        c->cushion_size += c->cushion_step;
        cushion_size_check(c);
        return c->cushion_size;
}

uint32_t
cushion_step_down(cushion_t *c)
{
        c->cushion_size -= c->cushion_step;
        cushion_size_check(c);
        return c->cushion_size;
}

uint32_t
cushion_get_step(cushion_t *c)
{
        return c->cushion_step;
}

uint32_t
cushion_use_estimate(cushion_t *c)
{
        c->cushion_size = c->cushion_estimate + c->cushion_step
                - (c->cushion_estimate % c->cushion_step);
        cushion_size_check(c);
#ifdef DEBUG_CUSHION
        debug_msg("cushion using size %ld\n", c->cushion_size);
#endif
        return c->cushion_size;
}

int32_t
cushion_diff_estimate_size(cushion_t *c)
{
        return (c->cushion_estimate - c->cushion_size);
}

