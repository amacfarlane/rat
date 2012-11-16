/*
 * FILE:    ui_send_prefs.c
 * PROGRAM: RAT
 * AUTHOR:  Colin Perkins
 *
 * Routines which send our preferences and other misc state to the user interface.
 *
 * Copyright (c) 2000-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: ui_send_prefs.c 3828 2006-09-08 19:42:07Z piers $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "mbus.h"
#include "mbus_parser.h"
#include "session.h"
#include "converter.h"
#include "repair.h"
#include "codec.h"
#include "codec_types.h"
#include "channel.h"
#include "auddev.h"
#include "audio_fmt.h"
#include "ui_send_prefs.h"

void
ui_send_converter_list(session_t *sp, char *addr)
{
        const converter_details_t *details;
        char *mbes;
        int i, cnt;

	if (!sp->ui_on) return;
        cnt = converter_get_count();

        mbus_qmsg(sp->mbus_engine, addr, "tool.rat.converters.flush", "", TRUE);

        for (i = 0; i < cnt; i++) {
                details = converter_get_details(i);
                mbes = mbus_encode_str(details->name);
                mbus_qmsg(sp->mbus_engine, addr, "tool.rat.converters.add", mbes, TRUE);
                xfree(mbes);
        }
}

void
ui_send_converter(session_t *sp, char *addr)
{
        const converter_details_t *details;
        char *mbes;
        int i, cnt;

	if (!sp->ui_on) return;
        cnt = converter_get_count();

        for(i = 0; i < cnt; i++) {
                details = converter_get_details(i);
                if (sp->converter == details->id) {
                        mbes = mbus_encode_str(details->name);
                        mbus_qmsg(sp->mbus_engine, addr, "tool.rat.converter", mbes, TRUE);
                        xfree(mbes);
                        return;
                }
        }
        debug_msg("Converter not found: %d\n", sp->converter);
}

void
ui_send_repair_scheme_list(session_t *sp, char *addr)
{
        const repair_details_t *r;
        char *mbes;
        uint16_t i, n;
	if (!sp->ui_on) return;

        n = repair_get_count();
        mbus_qmsg(sp->mbus_engine, addr, "tool.rat.repairs.flush", "", TRUE);

        for(i = 0; i < n; i++) {
                r = repair_get_details(i);
                mbes = mbus_encode_str(r->name);
                mbus_qmsg(sp->mbus_engine, addr, "tool.rat.repairs.add", mbes, TRUE);
                xfree(mbes);
        }
}

void
ui_send_codec_details(session_t *sp, char *addr, codec_id_t cid)
{
        char 			*caps, *long_name_e, *short_name_e, *pay_e, *descr_e;
        int 			 can_enc, can_dec, layers;
        char 			 pay[4];
        u_char 			 pt;
        const codec_format_t	*cf;

	if (!sp->ui_on) return;
        cf  = codec_get_format(cid);
        assert(cf != NULL);

        can_enc = codec_can_encode(cid);
        can_dec = codec_can_decode(cid);

        caps = NULL;
        if (can_enc && can_dec) {
                caps = mbus_encode_str("Encode and decode");
        } else if (can_enc) {
                caps = mbus_encode_str("Encode only");
        } else if (can_dec) {
                caps = mbus_encode_str("Decode only");
        } else {
                caps = mbus_encode_str("Not available");
        }

        pt = codec_get_payload(cid);
        if (payload_is_valid(pt)) {
                sprintf(pay, "%d", pt);
        } else {
                sprintf(pay, "-");
        }
	pay_e        = mbus_encode_str(pay);
	long_name_e  = mbus_encode_str(cf->long_name);
	short_name_e = mbus_encode_str(cf->short_name);
	descr_e      = mbus_encode_str(cf->description);
        layers       = codec_can_layer(cid);

        mbus_qmsgf(sp->mbus_engine, addr, TRUE,
                   "tool.rat.codecs.add",
                   "%s %s %s %d %d %d %d %d %s %s %d",
                   pay_e,
                   long_name_e,
                   short_name_e,
                   cf->format.channels,
                   cf->format.sample_rate,
                   cf->format.bytes_per_block,
                   cf->mean_per_packet_state_size,
                   cf->mean_coded_frame_size,
                   descr_e,
                   caps,
                   layers);
	xfree(caps);
	xfree(pay_e);
	xfree(long_name_e);
	xfree(short_name_e);
	xfree(descr_e);
}

void
ui_send_codec_list(session_t *sp, char *addr)
{
        uint32_t nCodecs, iCodec;
        codec_id_t cid;
	if (!sp->ui_on) return;

        mbus_qmsg(sp->mbus_engine, addr, "tool.rat.codecs.flush", "", TRUE);
        nCodecs = codec_get_number_of_codecs();
        for(iCodec = 0; iCodec < nCodecs; iCodec++) {
                cid = codec_get_codec_number(iCodec);
                if (cid) ui_send_codec_details(sp, addr, cid);
        }
}

static uint32_t sample_rates[] = {
	8000, 11025, 16000, 22050, 32000, 44100, 48000
};
#define NUM_RATES (sizeof(sample_rates) / sizeof(sample_rates[0]))

void
ui_send_sampling_mode_list(session_t *sp, char *addr)
{
	char	*mbes;
        char    modes[255]="";
        char    tmp[22];
        uint16_t channels, support, zap, i;

	if (!sp->ui_on) return;

	for(i =  0; i < NUM_RATES; i++) {
                support = 0;
                for(channels = 1; channels <= 2; channels++) {
                        if (audio_device_supports(sp->audio_device, sample_rates[i], channels)) {
				support += channels;
			}
                }
                switch(support) {
                case 3: sprintf(tmp, "%d-kHz,Mono,Stereo ", sample_rates[i]/1000); break;
                case 2: sprintf(tmp, "%d-kHz,Stereo ", sample_rates[i]/1000);      break;
                case 1: sprintf(tmp, "%d-kHz,Mono ", sample_rates[i]/1000);        break;
                case 0: continue;
                }
                strcat(modes, tmp);
        }

        /* Remove trailing space */
        zap = strlen(modes);
        if (zap) {
                zap -= 1;
                modes[zap] = '\0';
        }

	mbes = mbus_encode_str(modes);
	mbus_qmsg(sp->mbus_engine, addr, "tool.rat.sampling.supported", mbes, TRUE);
	xfree(mbes);
}

static void
ui_update_boolean(session_t *sp, char *addr, const char *field, int boolval)
{
        if (boolval) {
                mbus_qmsg(sp->mbus_engine, addr, field, "1", TRUE);
        } else {
                mbus_qmsg(sp->mbus_engine, addr, field, "0", TRUE);
        }
}

void
ui_send_powermeter(session_t *sp, char *addr)
{
	if (!sp->ui_on) return;
        ui_update_boolean(sp, addr, "tool.rat.powermeter", sp->meter);
}

void
ui_send_playout_bounds(session_t *sp, char *addr)
{
        char tmp[6];
	if (!sp->ui_on) return;
        ui_update_boolean(sp, addr, "tool.rat.playout.limit", sp->limit_playout);
        sprintf(tmp, "%4d", (int)sp->min_playout);
        mbus_qmsg(sp->mbus_engine, addr, "tool.rat.playout.min", tmp, TRUE);
        sprintf(tmp, "%4d", (int)sp->max_playout);
        mbus_qmsg(sp->mbus_engine, addr, "tool.rat.playout.max", tmp, TRUE);
}

void
ui_send_agc(session_t *sp, char *addr)
{
	if (!sp->ui_on) return;
        ui_update_boolean(sp, addr, "tool.rat.agc", sp->agc_on);
}

void
ui_send_loopback_gain(session_t *sp, char *addr)
{
	if (!sp->ui_on) return;
        ui_update_boolean(sp, addr, "tool.rat.loopback.gain", sp->loopback_gain);
}

void
ui_send_echo_suppression(session_t *sp, char *addr)
{
	if (!sp->ui_on) return;
        ui_update_boolean(sp, addr, "tool.rat.echo.suppress", sp->echo_suppress);
}

void
ui_send_lecture_mode(session_t *sp, char *addr)
{
	/* Update the UI to reflect the lecture mode setting...*/
	if (!sp->ui_on) return;
	mbus_qmsgf(sp->mbus_engine, addr, TRUE, "tool.rat.lecture.mode", "%1d", sp->lecture);
}

void
ui_send_encryption_key(session_t *sp, char *addr)
{
	char	*key_e;

	if (!sp->ui_on) return;

	if (sp->encrkey == NULL) {
	  key_e = mbus_encode_str("");
	} else {
	  key_e = mbus_encode_str(sp->encrkey);
	}
	mbus_qmsgf(sp->mbus_engine, addr, TRUE, "security.encryption.key", key_e);
	xfree(key_e);
}

void
ui_send_device_config(session_t *sp, char *addr)
{
        char          		 fmt_buf[64], *mbes;
        const audio_format 	*af;

	if (!sp->ui_on) return;
        af = audio_get_ifmt(sp->audio_device);
        if (af && audio_format_name(af, fmt_buf, 64)) {
                mbes = mbus_encode_str(fmt_buf);
                mbus_qmsg(sp->mbus_engine, addr, "tool.rat.format.in", mbes, TRUE);
                xfree(mbes);
        } else {
                debug_msg("Could not get ifmt\n");
        }
}

void
ui_send_rate(session_t *sp, char *addr)
{
	if (!sp->ui_on) return;
	mbus_qmsgf(sp->mbus_engine, addr, TRUE, "tool.rat.rate", "%3d", channel_encoder_get_units_per_packet(sp->channel_coder));
}

