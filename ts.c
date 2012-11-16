/*
 * FILE:    ts.c
 * AUTHORS: Orion Hodson
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: ts.c 4557 2010-01-08 15:41:42Z piers $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "crypt_random.h"
#include "ts.h"

typedef struct {
        uint32_t freq;
        uint32_t wrap;
} ticker;

/* Each timebase has a range corresponding to 0..N seconds.  Depending
 * on the frequency this represents a differing number of ticks.  So
 * an 8 kHz clock has ticks ranging from 0..M, a 16kHz clock has ticks
 * ranging from 0..2M.  We can compare timestamps simply by scaling up
 * from lower frequency clocks to higher frequency clocks.
 *
 * As defined in ts.h we use 26 bits as full range of ticks.  In
 * reality, the highest frequency clock coded (96k) uses under
 * the full 26 bit range ,0..floor (2^26-1 / 90000). All other clocks use
 * less than this.  The range corresponds to 372 seconds which is ample for
 * media playout concerns.
 *
 * NB. The tickers must be frequency ordered - comparison code depends
 * on it!
 */

ticker tickers[] = {
        {   8000, 0x002d6900 },
        {  11025, 0x003e94b4 },
        {  16000, 0x005ad200 },
        {  22050, 0x007d2968 },
        {  24000, 0x00883b00 },
        {  32000, 0x00b5a400 },
        {  40000, 0x00e30d00 },
        {  44100, 0x00fa52d0 },
        {  48000, 0x01107600 },
        {  90000, 0x01fedd40 },
        {  96000, 2 * 0x01107600 },
};

#define TS_NUM_TICKERS (sizeof(tickers)/sizeof(ticker))

#define TS_CHECK_BITS 0x03
#define TS_CHECK_BITS_Z 0x00

timestamp_t
ts_map32(uint32_t freq, uint32_t ticks32)
{
        uint32_t i;
        timestamp_t out = {0, 0, 0}; //SV-XXX was uninitialised

        /* Make invalid timestamp */
        out.check = TS_CHECK_BITS_Z; 

        for(i = 0; i < TS_NUM_TICKERS; i++) {
                if (tickers[i].freq == freq) {
                        out.ticks = ticks32 % tickers[i].wrap;
                        out.check = TS_CHECK_BITS;
                        out.idx   = i;
                        break;
                }
        }
        assert(ts_valid(out));
        return out;
}

static timestamp_t
ts_rebase(uint32_t new_idx, timestamp_t t)
{
        /* Use 64 bit quantity as temporary since
         * we are multiplying a 25 bit quantity by a
         * 16 bit one.  Only have to do this as
         * frequencies are not all multiples of each
         * other.
         */

        int64_t new_ticks;

        assert(new_idx < TS_NUM_TICKERS);

        /* new_ticks = old_ticks * new_freq / old_freq */
        new_ticks  = (int64_t)t.ticks * tickers[new_idx].freq;
        new_ticks /= tickers[t.idx].freq;

        /* Bound tick range */
        new_ticks %= (uint32_t)tickers[new_idx].wrap;

        /* Update ts fields */
        t.ticks   = (uint32_t)new_ticks;
        t.idx     = new_idx;

        return t;
}

int
ts_gt(timestamp_t t1, timestamp_t t2)
{
        uint32_t half_range, x1, x2;

        assert(ts_valid(t1));
        assert(ts_valid(t2));

        /* Make sure both timestamps have same (higher) timebase */
        if (t1.idx > t2.idx) {
                t2 = ts_rebase((unsigned)t1.idx, t2);
        } else if (t1.idx < t2.idx) {
                t1 = ts_rebase((unsigned)t2.idx, t1);
        }

        half_range = tickers[t1.idx].wrap >> 1;

        x1 = t1.ticks;
        x2 = t2.ticks;

        if (x1 > x2) {
                return (x1 - x2) < half_range;
        } else {
                return (x2 - x1) > half_range;
        }
}

int
ts_eq(timestamp_t t1, timestamp_t t2)
{
        assert(ts_valid(t1));
        assert(ts_valid(t2));

        /* Make sure both timestamps have same (higher) timebase */
        if (t1.idx > t2.idx) {
                t2 = ts_rebase((unsigned)t1.idx, t2);
        } else if (t1.idx < t2.idx) {
                t1 = ts_rebase((unsigned)t2.idx, t1);
        }

        return (t2.ticks == t1.ticks);
}

timestamp_t
ts_add(timestamp_t t1, timestamp_t t2)
{
        uint32_t ticks;
        assert(ts_valid(t1));
        assert(ts_valid(t2));

        /* Make sure both timestamps have same (higher) timebase */
        if (t1.idx > t2.idx) {
                t2 = ts_rebase(t1.idx, t2);
        } else if (t1.idx < t2.idx) {
                t1 = ts_rebase(t2.idx, t1);
        }
        assert(t1.idx == t2.idx);

        ticks    = (t1.ticks + t2.ticks) % tickers[t1.idx].wrap;
        t1.ticks = ticks;

        return t1;
}

timestamp_t
ts_sub(timestamp_t t1, timestamp_t t2)
{
        timestamp_t out;
        uint32_t ticks;

        assert(ts_valid(t1));
        assert(ts_valid(t2));

        /* Make sure both timestamps have same (higher) timebase */
        if (t1.idx > t2.idx) {
                t2 = ts_rebase(t1.idx, t2);
        } else if (t1.idx < t2.idx) {
                t1 = ts_rebase(t2.idx, t1);
        }

        assert(t1.idx == t2.idx);

        if (t1.ticks < t2.ticks) {
                /* Handle wrap */
                ticks = t1.ticks + tickers[t1.idx].wrap - t2.ticks;
        } else {
                ticks = t1.ticks - t2.ticks;
        }
        out.idx   = t1.idx;
        out.check = TS_CHECK_BITS;
        assert(ticks < tickers[t1.idx].wrap);
        assert((ticks & 0xfe000000) == 0);
        out.ticks = ticks;
        assert((unsigned)out.ticks == ticks);
        assert(ts_valid(out));
        return out;
}

timestamp_t
ts_abs_diff(timestamp_t t1, timestamp_t t2)
{
        if (ts_gt(t1, t2)) {
                return ts_sub(t1, t2);
        } else {
                return ts_sub(t2, t1);
        }
}

timestamp_t
ts_mul(timestamp_t t, uint32_t x)
{
        assert(ts_valid(t));
        t.ticks = t.ticks * x;
        return t;
}

timestamp_t
ts_div(timestamp_t t, uint32_t x)
{
        assert(ts_valid(t));
        t.ticks = t.ticks / x;
        return t;
}

timestamp_t
ts_convert(uint32_t new_freq, timestamp_t ts)
{
        uint32_t i;
        timestamp_t out = {0, 0, 0}; //SV-XXX was uninitialised

        out.check = 0;

        for(i = 0; i < TS_NUM_TICKERS; i++) {
                if (tickers[i].freq == new_freq) {
                        out = ts_rebase(i, ts);
                        break;
                }
        }

        assert(ts_valid(out));

        return out;
}

uint32_t
timestamp_to_ms(timestamp_t t1)
{
        double r;
        uint32_t f;
        assert(ts_valid(t1));
        f = ts_get_freq(t1);
        r = t1.ticks * 1000.0/(double)f;
        return (uint32_t)r;
}

uint32_t
timestamp_to_us(timestamp_t t1)
{
        double  r;
        uint32_t f;
        assert(ts_valid(t1));
        f = ts_get_freq(t1);
        r = t1.ticks * 1000000.0/(double)f;
        return (uint32_t)r;
}

int
ts_valid(timestamp_t t1)
{
        return ((unsigned)t1.idx < TS_NUM_TICKERS &&
                (t1.check == TS_CHECK_BITS) &&
                (unsigned)t1.ticks < tickers[t1.idx].wrap);
}

uint32_t
ts_get_freq(timestamp_t t1)
{
        assert(ts_valid(t1));
        return tickers[t1.idx].freq;
}

/* ts_map32_in and ts_map32_out are used to map between 32bit clock
 * and timestamp type which is modulo M.  Because the boundaries of
 * the timestamping wraps do not coincide, we cache last translated
 * value and add relative difference to other timestamp.  The application
 * does not then have to deal with discontinuities in timestamps.
 */

#define TS_WRAP_32 0x7fffffff

static
int ts32_gt(uint32_t a, uint32_t b)
{
        uint32_t diff;
        diff = a - b;
        return (diff < TS_WRAP_32 && diff != 0);
}

timestamp_t
ts_seq32_in(ts_sequencer *s, uint32_t freq, uint32_t curr_32)
{
        uint32_t delta_32;
        timestamp_t    delta_ts;

        /* Inited or freq changed check */
        if (s->freq != freq || !ts_valid(s->last_ts)) {
                s->last_ts = ts_map32(freq, lrand48());
                s->last_32 = curr_32;
                s->freq    = freq;
                return s->last_ts;
        }

        /* Find difference in 32 bit timestamps, scale to timestamp_t size
         * and add to last returned timestamp.
         */

        if (ts32_gt(curr_32, s->last_32)) {
                delta_32   = curr_32 - s->last_32;
                delta_ts   = ts_map32(freq, delta_32);
                s->last_ts = ts_add(s->last_ts, delta_ts);
        } else {
                delta_32   = s->last_32 - curr_32;
                delta_ts   = ts_map32(freq, delta_32);
                s->last_ts = ts_sub(s->last_ts, delta_ts);
        }

        s->last_32 = curr_32;
        return s->last_ts;
}

uint32_t
ts_seq32_out(ts_sequencer *s, uint32_t freq, timestamp_t curr_ts)
{
        uint32_t delta_32;
        timestamp_t    delta_ts;

        /* Inited or freq change check */
        if (s->freq != freq || !ts_valid(s->last_ts)) {
                s->last_ts = curr_ts;
                s->last_32 = lrand48();
                s->freq    = freq;
                return s->last_32;
        }

        if (ts_gt(curr_ts, s->last_ts)) {
                delta_ts   = ts_sub(curr_ts, s->last_ts);
                delta_32   = delta_ts.ticks * ts_get_freq(delta_ts) / freq;
                s->last_32 = s->last_32 + delta_32;
        } else {
                delta_ts   = ts_sub(s->last_ts, curr_ts);
                delta_32   = delta_ts.ticks * ts_get_freq(delta_ts) / freq;
                s->last_32 = s->last_32 - delta_32;
        }

        s->last_ts = curr_ts;
        return s->last_32;
}

