/*
 * FILE:    ts.h
 * AUTHORS: Orion Hodson
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 *
 * $Id: ts.h 3603 2002-03-16 01:00:49Z ucacoxh $
 */

#ifndef __TS_H__
#define __TS_H__

typedef struct {
        uint32_t ticks:26;
        uint32_t check:2;
        uint32_t idx:4;
} timestamp_t;

/* Maps a 32 bit unsigned integer into a valid timestamp.
 * This be used for mapping offsets, not timestamps (see
 * below) */

timestamp_t     ts_map32(uint32_t freq, uint32_t ts32);

/* Common Operations */
timestamp_t     ts_add      (timestamp_t ts1, timestamp_t ts2);
timestamp_t     ts_sub      (timestamp_t ts1, timestamp_t ts2);
timestamp_t     ts_abs_diff (timestamp_t ts1, timestamp_t ts2);

/* Operations for use on offets, i.e. small ts values */
timestamp_t     ts_mul      (timestamp_t ts,  uint32_t x);
timestamp_t     ts_div      (timestamp_t ts,  uint32_t x);

/* ts_gt = timestamp greater than */
int      ts_gt(timestamp_t t1, timestamp_t t2);
int      ts_eq(timestamp_t t1, timestamp_t t2);

/* ts_convert changes timebase of a timestamp */
timestamp_t     ts_convert(uint32_t new_freq, timestamp_t ts);

/* Conversion to milliseconds */
uint32_t timestamp_to_ms(timestamp_t t1);

/* Conversion to microseconds */
uint32_t timestamp_to_us(timestamp_t t1);

/* Debugging functions */
int      ts_valid(timestamp_t t1);
uint32_t ts_get_freq(timestamp_t t1);

typedef struct {
        timestamp_t     last_ts;
        uint32_t last_32;
        uint32_t freq;
} ts_sequencer;

/* These functions should be used for mapping sequences of
 * 32 bit timestamps to timestamp_t and vice-versa.
 */

timestamp_t     ts_seq32_in  (ts_sequencer *s, uint32_t f, uint32_t curr_32);
uint32_t ts_seq32_out (ts_sequencer *s, uint32_t f, timestamp_t     curr_ts);

#endif

