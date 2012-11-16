/*
 * FILE:     transcoder.c
 * PROGRAM:  Rat
 * AUTHOR:   Colin Perkins
 *
 * Based on auddev_mux.c, revision 1.11
 *
 * Copyright (C) 1996-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: transcoder.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "memory.h"
#include "audio_types.h"
#include "transcoder.h"

#define CHANNEL_SIZE  	8192
#define NUM_CHANNELS 	2

static int             num_open_channels = 0;
static sample         *channel[NUM_CHANNELS+1];
static int             head[NUM_CHANNELS+1];
static int             tail[NUM_CHANNELS+1];
static struct timeval  last_time[NUM_CHANNELS+1];
static struct timeval  curr_time[NUM_CHANNELS+1];
static int             first_time[NUM_CHANNELS+1];

int
transcoder_open(void)
{
  /* Open a fake audio channel. The value we return is used to identify the */
  /* channel for the other routines in this module. The #ifdefs in net.c    */
  /* prevent it being used in a select().                                   */
  /* Note: We must open EXACTLY two channels, before the other routines in  */
  /*       this module function correctly.                                  */
  int id, i;

  assert((num_open_channels >= 0) && (num_open_channels < NUM_CHANNELS));
  id = ++num_open_channels;
  head[id]       = 0;
  tail[id]       = 0;
  first_time[id] = 0;
  channel[id] = (sample *) xmalloc(CHANNEL_SIZE * sizeof(sample));
  for (i=0; i<CHANNEL_SIZE; i++) {
    channel[id][i] = L16_AUDIO_ZERO;
  }
  return id;
}

void
transcoder_close(int id)
{
  assert(num_open_channels > 0);
  assert(id > 0 && id <= num_open_channels);
  xfree(channel[id]);
  num_open_channels--;
}

int
transcoder_read(int id, sample *buf, int buf_size)
{
  int i, read_size, copy_size;

  assert(buf != 0);
  assert(buf_size > 0);
  assert(id > 0 && id <= num_open_channels);
  assert(head[id] <= CHANNEL_SIZE);
  assert(tail[id] <= CHANNEL_SIZE);
  assert(head[id] <= tail[id]);

  if (first_time[id] == 0) {
    gettimeofday(&last_time[id], NULL);
    first_time[id] = 1;
  }
  gettimeofday(&curr_time[id], NULL);
  read_size = (((curr_time[id].tv_sec - last_time[id].tv_sec) * 1000000) + (curr_time[id].tv_usec - last_time[id].tv_usec)) / 125;
  if (read_size > buf_size) read_size = buf_size;
  for (i=0; i<read_size; i++) {
    buf[i] = L16_AUDIO_ZERO;
  }
  last_time[id] = curr_time[id];

  copy_size = tail[id] - head[id];	/* The amount of data available in this module... */
  if (copy_size >= read_size) {
    copy_size = read_size;
  } else {
#ifdef DEBUG_TRANSCODER
    printf("transcoder_read: underflow, silence substituted -- want %d got %d channel %d\n", read_size, copy_size, id);
#endif
  }
  assert((head[id] + copy_size) <= tail[id]);
  for (i=0; i<copy_size; i++) {
    buf[i] = channel[id][head[id] + i];
  }
  head[id] += copy_size;

  assert(head[id] <= CHANNEL_SIZE);
  assert(tail[id] <= CHANNEL_SIZE);
  assert(head[id] <= tail[id]);
  return read_size;
}

int
transcoder_write(int id, sample *buf, int buf_size)
{
  int i;

  assert(buf != 0);
  assert(buf_size > 0);
  assert(id > 0 && id <= num_open_channels);
  assert(head[id] <= CHANNEL_SIZE);
  assert(tail[id] <= CHANNEL_SIZE);
  assert(head[id] <= tail[id]);

  if ((tail[id] + buf_size) > CHANNEL_SIZE) {
    for (i=0; i < (CHANNEL_SIZE - head[id]); i++) {
      channel[id][i] = channel[id][i + head[id]];
    }
    tail[id] -= head[id];
    head[id]  = 0;
  }
  assert((tail[id] + buf_size) <= CHANNEL_SIZE);

  for (i=0; i<buf_size; i++) {
    channel[id][tail[id] + i] = buf[i];
  }
  tail[id] += buf_size;
  assert(head[id] <= CHANNEL_SIZE);
  assert(tail[id] <= CHANNEL_SIZE);
  assert(head[id] <= tail[id]);
  return buf_size;
}

