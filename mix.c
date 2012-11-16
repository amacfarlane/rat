/*
 * FILE:        mix.c
 * PROGRAM:     RAT
 * AUTHOR:      Isidor Kouvelas
 * MODIFIED BY: Orion Hodson + Colin Perkins
 *
 * Copyright (c) 1995-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
        "$Id: mix.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "memory.h"
#include "util.h"
#include "session.h"
#include "codec_types.h"
#include "codec.h"
#include "audio_util.h"
#include "audio_fmt.h"
#include "channel_types.h"
#include "pdb.h"
#include "mix.h"
#include "playout.h"
#include "debug.h"
#include "parameters.h"
#include "ui_send_audio.h"

#define MIX_MAGIC 0x81654620

struct s_mixer {
        int          buf_len;              /* Length of circular buffer                */
        int          head, tail;           /* Index to head and tail samples in buffer */
        timestamp_t         head_time, tail_time; /* Time rep of head and tail                */
        int          dist;                 /* Distance between head and tail. (for debug)  */
        sample      *mix_buffer;           /* The buffer containing mixed audio data. */
        mixer_info_t info;
        uint32_t     magic;                /* Debug check value                       */
};

typedef void (*mix_f)(sample *buf, sample *incoming, int len);
static mix_f audio_mix_fn;

static void
mix_verify(const mixer_t *ms)
{
#ifdef DEBUG
        timestamp_t delta;
        int  dist;

        assert((ms->head + ms->buf_len - ms->tail) % ms->buf_len == ms->dist);

        assert(!ts_gt(ms->tail_time, ms->head_time));

        assert(ms->dist <= ms->buf_len);

        delta = ts_sub(ms->head_time, ms->tail_time);

        dist = delta.ticks * ms->info.channels * ms->info.sample_rate / ts_get_freq(delta);
        assert(abs((int)ms->dist - (int)dist) <= 1);

        if (ts_eq(ms->head_time, ms->tail_time)) {
                assert(ms->head == ms->tail);
        }
#endif
        assert(ms->magic == MIX_MAGIC);
}

/*
 * Initialise the circular buffer that is used in mixing.
 * The buffer length should be as big as the largest possible
 * device cushion used (and maybe some more).
 * We allocate space three times the requested one so that we
 * dont have to copy everything when we hit the boundaries..
 */
int
mix_create(mixer_t            **ppms,
           const mixer_info_t  *pmi,
	   timestamp_t                 now)
{
        mixer_t *pms;

        pms = (mixer_t *) xmalloc(sizeof(mixer_t));
        if (pms) {
                memset(pms, 0 , sizeof(mixer_t));
                pms->magic       = MIX_MAGIC;
                memcpy(&pms->info, pmi, sizeof(mixer_info_t));
                pms->buf_len     = pms->info.buffer_length * pms->info.channels;
                pms->mix_buffer  = (sample *)xmalloc(3 * pms->buf_len * BYTES_PER_SAMPLE);
                audio_zero(pms->mix_buffer, 3 * pms->info.buffer_length , DEV_S16);
                pms->mix_buffer += pms->buf_len;
                pms->head_time = pms->tail_time = ts_convert(pms->info.sample_rate, now);
                *ppms = pms;

                audio_mix_fn = audio_mix;
#ifdef WIN32
                if (mmx_present()) {
                        audio_mix_fn = audio_mix_mmx;
                }
#endif /* WIN32 */
                mix_verify(pms);
		debug_msg("Mixer created.  Aligned to %d %dkHz\n", now.ticks, ts_get_freq(now));
                return TRUE;
        }
        return FALSE;
}

void
mix_destroy(mixer_t **ppms)
{
        mixer_t *pms;

        assert(ppms);
        pms = *ppms;
        assert(pms);
        mix_verify(pms);
	debug_msg("Mixer destroyed.  Head %d %dkHz Tail %d %dkHz\n",
		  pms->head_time.ticks, ts_get_freq(pms->head_time),
		  pms->tail_time.ticks, ts_get_freq(pms->tail_time));
        xfree(pms->mix_buffer - pms->buf_len); /* yuk! ouch! splat! */
        xfree(pms);
        *ppms = NULL;
}

static void
mix_zero(mixer_t *ms, int offset, int len)
{
        assert(len <= ms->buf_len);
        if (offset + len > ms->buf_len) {
                audio_zero(ms->mix_buffer + offset, ms->buf_len - offset, DEV_S16);
                audio_zero(ms->mix_buffer, offset + len-ms->buf_len, DEV_S16);
        } else {
                audio_zero(ms->mix_buffer + offset, len, DEV_S16);
        }
        xmemchk();
}

static void
mix_advance_head(mixer_t *ms, timestamp_t new_head_time)
{
	timestamp_t	delta;
	int	zeros;

	mix_verify(ms);
	assert(ts_gt(new_head_time, ms->head_time));

	delta = ts_sub(new_head_time, ms->head_time);
	zeros = delta.ticks * ms->info.channels * ms->info.sample_rate / ts_get_freq(delta);

	mix_zero(ms, ms->head, zeros);
	ms->dist	+= zeros;
	ms->head	+= zeros;
	ms->head	%= ms->buf_len;
	ms->head_time	 = new_head_time;

	mix_verify(ms);
}

/* mix_put_audio mixes a single audio frame into mix buffer.  It returns
 * TRUE if incoming audio frame is compatible with mix, FALSE
 * otherwise.  */

int
mix_put_audio(mixer_t     *ms,
              pdb_entry_t *pdbe,
              coded_unit  *frame,
              timestamp_t  playout)
{
        sample          *samples;
        int32_t          pos;
        uint32_t         nticks, nsamples;
        uint16_t         channels;
	uint32_t         rate;
        timestamp_t	 frame_period, playout_end, delta;

        mix_verify(ms);
        if (!codec_get_native_info(frame->id, &rate, &channels)) {
		debug_msg("Cannot mix non-native media\n");
		abort();
	}

        if (rate != ms->info.sample_rate || channels != ms->info.channels) {
                /* This should only occur if source changes sample rate
                 * mid-stream and before buffering runs dry in end host.
                 * This should be a very rare event.
                 */
                debug_msg("Unit (%d, %d) not compitible with mix (%d, %d).\n",
                          rate,
                          channels,
                          ms->info.sample_rate,
                          ms->info.channels);
                return FALSE;
        }

        assert(rate     == (uint32_t)ms->info.sample_rate);
        assert(channels == (uint32_t)ms->info.channels);

        playout         = ts_convert(ms->info.sample_rate, playout);
        nticks          = frame->data_len / (sizeof(sample) * channels);
        frame_period    = ts_map32(rate, nticks);

	/* Map frame period to mixer time base, otherwise we can get
         * truncation errors in verification of mixer when sample rate
         * conversion is active.  */
	frame_period    = ts_convert(ms->info.sample_rate, frame_period);

        if (pdbe->first_mix) {
                debug_msg("New mix\n");
                pdbe->next_mix = playout;
                pdbe->first_mix  = 0;
        }

        mix_verify(ms);

	if (ts_gt(ms->tail_time, playout)) {
		debug_msg("playout before tail (%d %dkHz < %d %dkHz)\n",
			  playout.ticks, ts_get_freq(playout),
			  ms->tail_time.ticks, ts_get_freq(ms->tail_time));
	}

        samples  = (sample*)frame->data;
        nsamples = frame->data_len / sizeof(sample);

	/* Advance head if necessary */
        playout_end = ts_add(playout, ts_map32(ms->info.sample_rate, nsamples / ms->info.channels));
	if (ts_gt(playout_end, ms->head_time)) {
		uint32_t playout_delta = timestamp_to_ms(ts_sub(playout_end, ms->head_time));
		if (playout_delta > 1000) {
		 	debug_msg("WARNING: Large playout buffer advancement (%dms)\n", playout_delta);
		}
		mix_advance_head(ms, playout_end);
	}

        /* Check for overlap in decoded frames */
        if (!ts_eq(pdbe->next_mix, playout)) {
                if (ts_gt(pdbe->next_mix, playout)) {
                        delta = ts_sub(pdbe->next_mix, playout);
                        if (ts_gt(frame_period, delta)) {
                                uint32_t  trim;
				/* Unit overlaps with earlier data written to buffer.
				 * Jump past overlapping samples, decrease number of
				 * samples that need to be written and correct playout
				 * so they are written to the correct place.
				 */
				delta     = ts_convert(ms->info.sample_rate, delta);
				trim      = delta.ticks * ms->info.channels;
				debug_msg("Mixer trimmed %d samples (Expected playout %d got %d) ssrc (0x%08x)\n",
					  trim, pdbe->next_mix.ticks, playout.ticks, pdbe->ssrc);
                                samples  += trim;
				nsamples -= trim;
				playout   = ts_add(playout, delta);
                        } else {
                                debug_msg("Skipped unit\n");
				return TRUE; /* Nothing to do but no fmt change */
			}
                } else {
			debug_msg("Gap between units %d %d ssrc 0x%08x\n",
				pdbe->next_mix.ticks,
				playout.ticks,
				pdbe->ssrc);
                }
        }

        /* Work out where to write the data (head_time > playout) */
        delta = ts_sub(ms->head_time, playout);
        pos = ms->head - delta.ticks * ms->info.channels;
        pos = (pos + ms->buf_len) % ms->buf_len; /* Handle wraps */

        if (pos + nsamples > (uint32_t)ms->buf_len) {
                audio_mix_fn(ms->mix_buffer + pos,
                             samples,
                             ms->buf_len - pos);
                audio_mix_fn(ms->mix_buffer,
                             samples + (ms->buf_len - pos),
                             pos + nsamples - ms->buf_len);
        } else {
                audio_mix_fn(ms->mix_buffer + pos,
                             samples,
                             nsamples);
        }
        xmemchk();
        pdbe->next_mix = playout_end;

        return TRUE;
}

/*
 * The mix_get_audio function returns a pointer to "request" samples of mixed
 * audio data, suitable for playout (ie: you can do audio_device_write() with
 * the returned data).
 *
 * This function was modified so that it returns the amount of
 * silence at the end of the buffer returned so that the cushion
 * adjustment functions can use it to decrease the cushion.
 *
 * Note: "request" is number of samples to get and not sampling intervals!
 */

int
mix_get_audio(mixer_t *ms, int request, sample **bufp)
{
        int  silence, amount;
        timestamp_t delta;

        xmemchk();
        mix_verify(ms);
        amount = request;
        assert(amount < ms->buf_len);
        if (amount > ms->dist) {
		timestamp_t new_head_time;
                /*
		 * If we dont have enough to give one of two things
                 * must have happened.
                 * a) There was silence :-)
                 * b) There wasn't enough time to decode the stuff...
                 * In either case we will have to return silence for
                 * now so zero the rest of the buffer and move the head.
                 */
#ifdef DEBUG_MIX
                if (!ts_eq(ms->head_time, ms->tail_time)) {
                        /* Only print message if not-silent */
                        debug_msg("Insufficient audio: %d < %d\n", ms->dist, amount);
                }
#endif /* DEBUG_MIX */
                silence = amount - ms->dist;
		new_head_time = ts_add(ms->head_time,
				       ts_map32(ms->info.sample_rate, silence/ms->info.channels));
		mix_advance_head(ms, new_head_time);
        } else {
                silence = 0;
        }

        if (ms->tail + amount > ms->buf_len) {
                /*
                 * We have run into the end of the buffer so we will
                 * have to copy stuff before we return it.
                 * The space after the 'end' of the buffer is used
                 * for this purpose as the space before is used to
                 * hold silence that is returned in case the cushion
                 * grows too much.
                 * Of course we could use both here (depending on which
                 * direction involves less copying) and copy actual
                 * voice data in the case a cushion grows into it.
                 * The problem is that in that case we are probably in
                 * trouble and want to avoid doing too much...
                 *
                 * Also if the device is working in similar boundaries
                 * to our chunk sizes and we are a bit careful about the
                 * possible cushion sizes this case can be avoided.
                 */
                xmemchk();
                memcpy(ms->mix_buffer + ms->buf_len, ms->mix_buffer, BYTES_PER_SAMPLE*(ms->tail + amount - ms->buf_len));
                xmemchk();
#ifdef DEBUG_MIX
                debug_msg("Copying start of mix len: %d\n", ms->tail + amount - ms->buf_len);
#endif /* DEBUG_MIX */
        }

        mix_verify(ms);

        *bufp = ms->mix_buffer + ms->tail;
        delta = ts_map32(ms->info.sample_rate, amount/ms->info.channels);
        ms->tail_time = ts_add(ms->tail_time, delta);

        ms->tail      += amount;
        ms->tail      %= ms->buf_len;
        ms->dist      -= amount;
        mix_verify(ms);

        return silence;
}

/*
 * We need the amount of time we went dry so that we can make a time
 * adjustment to keep in sync with the receive buffer etc...
 */
void
mix_new_cushion(mixer_t *ms,
                int      last_cushion_size,
                int      new_cushion_size,
                int      dry_time,
                sample **bufp)
{
        int diff, elapsed_time;

        debug_msg("Getting new cushion %d old %d\n", new_cushion_size, last_cushion_size);

        mix_verify(ms);
        elapsed_time = (last_cushion_size + dry_time);
        diff = abs(new_cushion_size - elapsed_time) * ms->info.channels;

        if (new_cushion_size > elapsed_time) {
                /*
                 * New cushion is larger so move tail back to get
                 * the right amount and end up at the correct time.
                 * The effect of moving the tail is that some old
                 * audio and/or silence will be replayed. We do not
                 * care to much as we are right after an underflow.
                 */
                ms->tail -= diff;
                if (ms->tail < 0) {
                        ms->tail += ms->buf_len;
                }
                ms->dist += diff;

                ms->tail_time = ts_sub(ms->tail_time,
                                       ts_map32(ms->info.sample_rate, diff/ms->info.channels));
                mix_verify(ms);
        } else if (new_cushion_size < elapsed_time) {
                /*
                 * New cushion is smaller so we have to throw away
                 * some audio.
                 */
                ms->tail += diff;
                ms->tail %= ms->buf_len;
                ms->tail_time = ts_add(ms->tail_time,
                                       ts_map32(ms->info.sample_rate, diff/ms->info.channels));
                if (diff > ms->dist) {
                        ms->head = ms->tail;
                        ms->head_time = ms->tail_time;
                        ms->dist = 0;
                } else {
                        ms->dist -= diff;
                }
                mix_verify(ms);
        }
        mix_verify(ms);
        mix_get_audio(ms, new_cushion_size * ms->info.channels, bufp);
        mix_verify(ms);
}

uint16_t
mix_get_energy(mixer_t *ms, uint16_t samples)
{
        sample        *bp;

        if (ms->tail < samples) {
                bp = ms->mix_buffer + ms->buf_len - samples * ms->info.channels;
        } else {
                bp = ms->mix_buffer + ms->tail - samples;
        }

        return audio_avg_energy(bp, samples, 1);
}

int
mix_active(mixer_t *ms)
{
        mix_verify(ms);
        return !ts_eq(ms->head_time, ms->tail_time);
}

const mixer_info_t *
mix_query(const mixer_t *ms)
{
	mix_verify(ms);
        if (ms == NULL) {
                return FALSE;
        }
        return &ms->info;
}

