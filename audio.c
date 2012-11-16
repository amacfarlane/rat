/*
 * FILE:     audio.c
 * PROGRAM:  RAT
 * AUTHOR:   Orion Hodson
 *
 * Based on necessity and earlier code by Isidor Kouvelas, Colin
 * Perkins, and Orion Hodson.  The existence of this code is pretty
 * offensive, but it's here for historical reasons.
 *
 * Copyright (c) 1995-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: audio.c 4393 2009-02-18 13:43:03Z piers $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "memory.h"
#include "debug.h"
#include "audio_types.h"
#include "codec_types.h"
#include "codec.h"
#include "channel_types.h"
#include "channel.h"
#include "audio.h"
#include "audio_fmt.h"
#include "audio_util.h"
#include "session.h"
#include "transcoder.h"
#include "pdb.h"
#include "transmit.h"
#include "mix.h"
#include "cushion.h"
#include "source.h"
#include "sndfile.h"
#include "tonegen.h"
#include "voxlet.h"

/* Zero buf used for writing zero chunks during cushion adaption */
static sample* zero_buf;

/*****************************************************************************/

/* audio_device_release releases open the device.
 * If it succeeds it returns TRUE, otherwise returns FALSE.
 */

int
audio_device_release(session_t *sp, audio_desc_t the_dev)
{
        if (sp->audio_device == 0) {
                debug_msg("Audio device already released from session\n");
                return FALSE;
        }

        if (sp->audio_device != the_dev) {
                debug_msg("Releasing wrong device!\n");
                return FALSE;
        }

	/* Mix is going to be destroyed - tone_generator and
	 * voxlet have pointers to mixer in their state that
	 * is about to expire.  Could pass mixer as argument
	 * to their process functions...
	 */
	if (sp->tone_generator) {
		tonegen_destroy(&sp->tone_generator);
	}

	if (sp->local_file_player) {
		voxlet_destroy(&sp->local_file_player);
	}

        cushion_destroy(&sp->cushion);
        mix_destroy(&sp->ms);

        tx_stop(sp->tb);
        tx_destroy(&sp->tb);

        source_list_clear(sp->active_sources);

        audio_close(sp->audio_device);
        sp->audio_device = 0;

        xfree(zero_buf);
        zero_buf = NULL;

        return FALSE;
}

/* Reconfiguration code ******************************************************/

typedef struct s_audio_config {
        audio_desc_t device;
        codec_id_t   primary;
        int          render_3d;
} audio_config;

static int
ac_create(audio_config **ppac)
{
        audio_config *pac = (audio_config*)xmalloc(sizeof(audio_config));
        if (pac) {
                /* assign invalid values so easy to see what's change
                 * in reconfigure. */
                pac->device    = 0;
                pac->primary   = 0 ;
                pac->render_3d = -1;
                *ppac = pac;
                return TRUE;
        }
        return FALSE;
}

static void
ac_destroy(audio_config **ac)
{
        assert(ac);
        assert(*ac);
        xfree(*ac);
        *ac = NULL;
}

void
audio_device_register_change_device(struct s_session *sp,
                                    audio_desc_t        new_dev)
{
        if (sp->new_config == NULL) {
                ac_create(&sp->new_config);
        }
        assert(sp->new_config);
        sp->new_config->device = new_dev;
}

void
audio_device_register_change_primary(struct s_session *sp,
                                     codec_id_t          primary)
{
        if (sp->new_config == NULL) {
                ac_create(&sp->new_config);
        }
        assert(sp->new_config);
        sp->new_config->primary = primary;
}

void
audio_device_register_change_render_3d(struct s_session *sp,
                                      int enabled)
{
        if (sp->new_config == NULL) {
                ac_create(&sp->new_config);
        }
        assert(sp->new_config);
        sp->new_config->render_3d = enabled;
}

static int
audio_device_attempt_config(session_t *sp, audio_config *config)
{
        audio_format *inf, *ouf;
        const codec_format_t *incf;
        int success;

        incf = codec_get_format(config->primary);
        assert(incf);

        inf = audio_format_dup(&incf->format);
        ouf = audio_format_dup(&incf->format);

        if (inf->channels != 2 && config->render_3d) {
                /* If 3d rendering is enabled we need stereo output
                 * format.
                 */
                ouf->channels = 2;
        }

        success = audio_open(config->device, inf, ouf);
        if (success) {
                mixer_info_t  mi;
                uint16_t unit_len;
                assert(sp->ms           == NULL);
                assert(sp->tb           == NULL);
                assert(sp->cushion      == NULL);

                audio_non_block(config->device);

                /* Initialize read and write components */
                sp->meter_period = inf->sample_rate / 15;
                unit_len         = inf->bytes_per_block * 8 / (inf->bits_per_sample*inf->channels);
                tx_create(&sp->tb, sp, (uint32_t)inf->sample_rate, (uint16_t)inf->channels, (uint16_t)unit_len);
                cushion_create(&sp->cushion, (uint32_t)inf->sample_rate);
		sp->cur_ts = ts_convert(inf->sample_rate, sp->cur_ts);
                mi.sample_rate   = ouf->sample_rate;
                mi.channels      = ouf->channels;
                mi.buffer_length = 32640;
                mix_create(&sp->ms, &mi, sp->cur_ts);

                if (zero_buf == NULL) {
                        zero_buf = (sample*)xmalloc(unit_len * sizeof(sample));
                        audio_zero(zero_buf, unit_len, DEV_S16);
                }
        }

        audio_format_free(&inf);
        audio_format_free(&ouf);

        return success;
}

/* audio_device_reconfigure returns TRUE if device reconfigured,
 * FALSE otherwise
 */

int
audio_device_reconfigure(session_t *sp)
{
        audio_config prev_config, *curr_config, *new_config;
        audio_port_t iport = 0, oport = 0;
        int change_req;

        assert(sp->new_config != NULL);

        new_config = sp->new_config;

        change_req = FALSE;
        if (new_config->device == 0) {
                /* No request to change audio device */
                new_config->device = sp->audio_device;
        } else if (new_config->device != sp->audio_device) {
                /* Request to change device */
                change_req = TRUE;
                debug_msg("Change device requested.\n");
        }

        if (sp->audio_device) {
                iport = audio_get_iport(sp->audio_device);
                oport = audio_get_oport(sp->audio_device);
        }

        if (new_config->primary == 0) {
                /* No request to change primary encoding */
                new_config->primary = codec_get_by_payload(sp->encodings[0]);
        } else if (codec_audio_formats_compatible(new_config->primary, codec_get_by_payload(sp->encodings[0])) == FALSE) {
                /* Request to change primary and format incompatible so
                 * device needs rejigging */
                change_req = TRUE;
                debug_msg("Change primary requested.\n");
        } else {
                /* Request to change primary to compatible coding. */
                sp->encodings[0] = codec_get_payload(new_config->primary);
        }

        if (new_config->render_3d == -1) {
                /* No request to change 3d rendering */
                new_config->render_3d = sp->render_3d;
        } else if (new_config->render_3d != sp->render_3d) {
                change_req = TRUE;
                debug_msg("Change 3d rendering enabled requested.\n");
        }

        if (change_req) {
                /* Store current config in case it reconfig attempt fails */
                prev_config.device    = sp->audio_device;
                prev_config.primary   = codec_get_by_payload(sp->encodings[0]);
                prev_config.render_3d = sp->render_3d;

                if (sp->audio_device) {
                        audio_device_release(sp, sp->audio_device);
			if (strcasestr(audio_get_dev_name(prev_config.device),"PulseAudio")){
			    debug_msg("Sleeping for PulseAudio release.\n");
			    usleep(1100000);
			}
                }

                if (audio_device_attempt_config(sp, new_config)) {
                        /* New config acceptable */
                        curr_config = new_config;
                } else if (audio_device_attempt_config(sp, &prev_config)) {
                        /* else attempt to fallback to previous config */
                        curr_config = &prev_config;
                        debug_msg("Fellback to old dev config\n");
                } else {
                        /* Fallback to guaranteed config - something
                         * went badly wrong */
                        ac_destroy(&new_config);
                        audio_device_get_safe_config(&new_config);
                        assert(new_config);

                        if (audio_device_attempt_config(sp, new_config) == FALSE) {
                                /* should never get here */
                                abort();
                        }
                        curr_config = new_config;
                        debug_msg("Fell back to safe config\n");
                }

                debug_msg("0x%08x 0x%08x\n", prev_config.device, curr_config->device);

                if (prev_config.device == curr_config->device) {
                        debug_msg("Restoring ports\n");
                        audio_set_iport(curr_config->device, iport);
                        audio_set_oport(curr_config->device, oport);
                } else {
                        const audio_port_details_t *papd;
                        /* Ports will be squiffy */
                        papd = audio_get_iport_details(curr_config->device, 0);
                        audio_set_iport(curr_config->device, papd->port);
                        papd = audio_get_oport_details(curr_config->device, 0);
                        audio_set_oport(curr_config->device, papd->port);
                }

                audio_loopback(curr_config->device, sp->loopback_gain);

                sp->audio_device  = curr_config->device;
                sp->encodings[0]  = codec_get_payload(curr_config->primary);

                /* If using redundancy check it is still relevent */
                if (sp->num_encodings > 1 &&
                    !codec_audio_formats_compatible(curr_config->primary, codec_get_by_payload(sp->encodings[1]))) {
                        const cc_details_t *ccd;
                        channel_encoder_destroy(&sp->channel_coder);
                        ccd = channel_get_null_coder();
                        channel_encoder_create(ccd->descriptor, &sp->channel_coder);
                        sp->num_encodings = 1;
                }
                sp->render_3d = curr_config->render_3d;
        } else {
                debug_msg("audio device reconfigure - nothing to do.\n");
                if (tx_is_sending(sp->tb)) {
                        tx_stop(sp->tb);
                        tx_start(sp->tb);
                }
        }

        tx_igain_update(sp->tb);

        ac_destroy(&sp->new_config);
        return change_req;
}

int
audio_device_get_safe_config(audio_config **ppac)
{
        if (ac_create(ppac)) {
                audio_config *pac = *ppac;
                pac->device  = audio_get_null_device();
                pac->primary = codec_get_by_name("PCMU-8K-Mono");
                pac->render_3d = FALSE;
                assert(pac->primary);
                return TRUE;
        }
        return FALSE;
}

/*****************************************************************************/

static int
audio_device_write(session_t *sp, sample *buf, int dur)
{
        const audio_format *ofmt = audio_get_ofmt(sp->audio_device);
        int len;

        assert(dur >= 0);

        if (sp->out_file) {
                snd_write_audio(&sp->out_file, buf, (uint16_t)(dur * ofmt->channels));
        }

        len =  audio_write(sp->audio_device, buf, dur * ofmt->channels);

        xmemchk();

        return len;
}

/* This function needs to be modified to return some indication of how well
 * or not we are doing.
 */
int
audio_rw_process(session_t *spi, session_t *spo,  struct s_mixer *ms)
{
        uint32_t cushion_size, read_dur;
        struct s_cushion_struct *c;
	int	trailing_silence, new_cushion, cushion_step, diff;
        const audio_format* ofmt;
	sample	*bufp;

	session_validate(spi);
	session_validate(spo);

        c = spi->cushion;

	if ((read_dur = tx_read_audio(spi->tb)) <= 0) {
		return 0;
	} else {
                if (!spi->audio_device) {
                        /* no device means no cushion */
                        return read_dur;
                }
	}

        xmemchk();

	/* read_dur now reflects the amount of real time it took us to get
	 * through the last cycle of processing.
         */

	if (spo->lecture == TRUE && spo->auto_lecture == 0) {
                cushion_update(c, read_dur, CUSHION_MODE_LECTURE);
	} else {
                cushion_update(c, read_dur, CUSHION_MODE_CONFERENCE);
	}

	/* Following code will try to achieve new cushion size without
	 * messing up the audio...
	 * First case is when we are in trouble and the output has gone dry.
	 * In this case we write out a complete new cushion with the desired
	 * size. We do not care how much of it is going to be real audio and
	 * how much silence so long as the silence is at the head of the new
	 * cushion. If the silence was at the end we would be creating
	 * another silence gap...
	 */
        cushion_size = cushion_get_size(c);
        ofmt         = audio_get_ofmt(spi->audio_device);

	if (cushion_size < read_dur) {
                debug_msg("catch up! read_dur(%d) > cushion_size(%d)\n",
                        read_dur,
                        cushion_size);
		/* Use a step for the cushion to keep things nicely rounded  */
                /* in the mixing. Round it up.                               */
                new_cushion = cushion_use_estimate(c);
                assert(new_cushion >= 0 && new_cushion < 100000);

                /* The mix routine also needs to know for how long the       */
                /* output went dry so that it can adjust the time.           */
                mix_new_cushion(ms,
                                cushion_size,
                                new_cushion,
                                (read_dur - cushion_size),
                                &bufp);
                audio_device_write(spo, bufp, new_cushion);
                /* We've blocked for this long for whatever reason           */
                cushion_size    = new_cushion;
        } else {
                trailing_silence = mix_get_audio(ms, read_dur * ofmt->channels, &bufp);
                cushion_step = cushion_get_step(c);
                diff  = 0;
                if (trailing_silence > cushion_step) {
                        /* Check whether we need to adjust the cushion */
                        diff = cushion_diff_estimate_size(c);
                        if (abs(diff) < cushion_step) {
                                diff = 0;
                        }
                }

                /* If diff is less than zero then we must decrease the */
                /* cushion so loose some of the trailing silence.      */
                if (diff < 0 &&
                    mix_active(ms) == FALSE &&
                    source_list_source_count(spi->active_sources) == 0) {
                        /* Only decrease cushion if not playing anything out */
                        uint32_t old_cushion;
                        old_cushion = cushion_get_size(c);
                        if (read_dur > (unsigned)cushion_step) {
                                cushion_step_down(c);
                                if (cushion_get_size(c) != old_cushion) {
                                        debug_msg("Decreasing cushion\n");
                                        read_dur -= cushion_step;
                                }
                        }
                }
                assert(read_dur < 0x7fffffff);
                audio_device_write(spo, bufp, read_dur);
                /*
                 * If diff is greater than zero then we must increase the
                 * cushion so increase the amount of trailing silence.
                 */
                if (diff > 0) {
                        assert(cushion_step > 0);
                        audio_device_write(spo, zero_buf, cushion_step);
                        cushion_step_up(c);
                        debug_msg("Increasing cushion.\n");
                }
        }
        return (read_dur);
}
