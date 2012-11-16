/*
 * FILE:    ui_send_audio.c
 * PROGRAM: RAT
 * AUTHOR:  Colin Perkins
 *
 * Routines which send audio related mbus commands to the user interface.
 *
 * Copyright (c) 2000-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: ui_send_audio.c 4084 2007-07-27 16:16:37Z piers $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "mbus.h"
#include "mbus_parser.h"
#include "session.h"
#include "audio_types.h"
#include "audio.h"
#include "transmit.h"
#include "codec.h"
#include "channel.h"
#include "parameters.h"
#include "repair.h"
#include "render_3D.h"
#include "pdb.h"
#include "rtp.h"
#include "util.h"
#include "ui_send_audio.h"

#define SECS_BETWEEN_1900_1970 2208988800u

void
ui_send_audio_input_port(session_t *sp, char *addr)
{
        const audio_port_details_t 	*apd = NULL;
        audio_port_t 			 port;
        char        			*mbes;
        int          			 i, n, found;

	if (!sp->ui_on) return;
        port = audio_get_iport(sp->audio_device);

        found = FALSE;
        n = audio_get_iport_count(sp->audio_device);
        for(i = 0; i < n; i++) {
                apd = audio_get_iport_details(sp->audio_device, i);
                if (apd->port == port) {
                        found = TRUE;
                        break;
                }
        }

        if (found == FALSE) {
                debug_msg("Port %d not found!\n", port);
                apd = audio_get_iport_details(sp->audio_device, 0);
        }

        mbes = mbus_encode_str(apd->name);
        mbus_qmsg(sp->mbus_engine, addr, "audio.input.port", mbes, TRUE);
        xfree(mbes);
}

void
ui_send_audio_input_port_list(session_t *sp, char *addr)
{
        const audio_port_details_t *apd;
        char *mbes;
        int i, n;

	if (!sp->ui_on) return;
        mbus_qmsg(sp->mbus_engine, addr, "audio.input.ports.flush", "", TRUE);

        n = audio_get_iport_count(sp->audio_device);
        assert(n >= 1);

        for(i = 0; i < n; i++) {
                apd = audio_get_iport_details(sp->audio_device, i);
                mbes = mbus_encode_str(apd->name);
                mbus_qmsg(sp->mbus_engine, addr, "audio.input.ports.add", mbes, TRUE);
                xfree(mbes);
        }
}

void
ui_send_audio_input_mute(session_t *sp, char *addr)
{
	if (!sp->ui_on) return;
	if (tx_is_sending(sp->tb)) {
		mbus_qmsg(sp->mbus_engine, addr, "audio.input.mute", "0", TRUE);
	} else {
		mbus_qmsg(sp->mbus_engine, addr, "audio.input.mute", "1", TRUE);
	}
}

void
ui_send_audio_input_gain(session_t *sp, char *addr)
{
	if (!sp->ui_on) return;
        mbus_qmsgf(sp->mbus_engine, addr, TRUE, "audio.input.gain", "%3d", audio_get_igain(sp->audio_device));
}

void
ui_send_audio_input_powermeter(session_t *sp, char *addr, int level)
{
	static int	ol;

	if (!sp->ui_on) return;
        assert(level>=0 && level <=100);

	if (ol == level) {
		return;
	}

	mbus_qmsgf(sp->mbus_engine, addr, FALSE, "audio.input.powermeter", "%3d", level);
	ol = level;
}

void
ui_send_audio_output_port(session_t *sp, char *addr)
{
        const audio_port_details_t 	*apd = NULL;
        audio_port_t 			 port;
        char        			*mbes;
        int          			 i, n, found;

	if (!sp->ui_on) return;
        port = audio_get_oport(sp->audio_device);

        found = FALSE;
        n = audio_get_oport_count(sp->audio_device);
        for(i = 0; i < n; i++) {
                apd = audio_get_oport_details(sp->audio_device, i);
                if (apd->port == port) {
                        found = TRUE;
                        break;
                }
        }

        if (found == FALSE) {
                debug_msg("Port %d not found!\n", port);
                apd = audio_get_oport_details(sp->audio_device, 0);
        }

        mbes = mbus_encode_str(apd->name);

        mbus_qmsg(sp->mbus_engine, addr, "audio.output.port", mbes, TRUE);
        xfree(mbes);
}

void
ui_send_audio_output_port_list(session_t *sp, char *addr)
{
        const audio_port_details_t *apd;
        char *mbes;
        int i, n;

	if (!sp->ui_on) return;
        mbus_qmsg(sp->mbus_engine, addr, "audio.output.ports.flush", "", TRUE);

        n = audio_get_oport_count(sp->audio_device);
        assert(n >= 1);

        for(i = 0; i < n; i++) {
                apd = audio_get_oport_details(sp->audio_device, i);
                mbes = mbus_encode_str(apd->name);
                mbus_qmsg(sp->mbus_engine, addr, "audio.output.ports.add", mbes, TRUE);
                xfree(mbes);
        }
}

void
ui_send_audio_output_mute(session_t *sp, char *addr)
{
	if (!sp->ui_on) return;
	if (sp->playing_audio) {
		mbus_qmsg(sp->mbus_engine, addr, "audio.output.mute", "0", TRUE);
	} else {
		mbus_qmsg(sp->mbus_engine, addr, "audio.output.mute", "1", TRUE);
	}
}

void
ui_send_audio_output_gain(session_t *sp, char *addr)
{
	if (!sp->ui_on) return;
        mbus_qmsgf(sp->mbus_engine, addr, TRUE, "audio.output.gain", "%3d", audio_get_ogain(sp->audio_device));
}


void
ui_send_audio_output_powermeter(session_t *sp, char *addr, int level)
{
	static int	ol;
        assert(level>=0 && level <=100);

	if (!sp->ui_on) return;
	if (ol == level) {
                return;
	}

	mbus_qmsgf(sp->mbus_engine, addr, FALSE, "audio.output.powermeter", "%3d", level);
	ol = level;
}

void
ui_send_audio_device_list(session_t *sp, char *addr)
{
        const audio_device_details_t *add;
        char *mbes, dev_name[AUDIO_DEVICE_NAME_LENGTH];
        int i,nDev;

	if (!sp->ui_on) return;
        mbus_qmsg(sp->mbus_engine, addr, "audio.devices.flush", "", TRUE);
        nDev = audio_get_device_count();

        for(i = 0; i < nDev; i++) {
                add  = audio_get_device_details(i);
                strncpy(dev_name, add->name, AUDIO_DEVICE_NAME_LENGTH);
                purge_chars(dev_name, "[]()");
                mbes = mbus_encode_str(dev_name);
                mbus_qmsg(sp->mbus_engine, addr, "audio.devices.add", mbes, TRUE);
                xfree(mbes);
        }
}

void
ui_send_audio_device(session_t *sp, char *addr)
{
        const audio_device_details_t *add = NULL;
        char                         *mbes;
        uint32_t                       i, n;

	if (!sp->ui_on) return;
        n = audio_get_device_count();
        for(i = 0; i < n; i++) {
                add = audio_get_device_details(i);
                if (sp->audio_device == add->descriptor) {
                        break;
                }
        }

        if (i != n) {
                char dev_name[AUDIO_DEVICE_NAME_LENGTH];
                strncpy(dev_name, add->name, AUDIO_DEVICE_NAME_LENGTH);
                purge_chars(dev_name, "()[]");
                mbes = mbus_encode_str(dev_name);
                mbus_qmsg(sp->mbus_engine, addr, "audio.device", mbes, TRUE);
                xfree(mbes);
        }
}

void
ui_send_audio_suppress_silence(session_t *sp, char *addr)
{
        const char *name;
        char thresh[6];
	if (sp->ui_on == FALSE) {
                return;
        }

        /* This is just for other applications to hear */
        if (sp->silence_detection != SILENCE_DETECTION_OFF) {
                mbus_qmsg(sp->mbus_engine, addr, "audio.suppress.silence", "1", TRUE);
        } else {
                mbus_qmsg(sp->mbus_engine, addr, "audio.suppress.silence", "0", TRUE);
        }

        /* This is for the ui */
        name = sd_name(sp->silence_detection);
        mbus_qmsg(sp->mbus_engine, addr, "tool.rat.silence", mbus_encode_str(name), TRUE);
        sprintf(thresh, "%d", sp->manual_sd_thresh);
        mbus_qmsg(sp->mbus_engine, addr, "tool.rat.silence.threshold", thresh, TRUE);
	if (sp->logger != NULL) {
		struct timeval	t;
		gettimeofday(&t, NULL);
#ifdef WIN32
		fprintf(sp->logger, "silence    %I64u.%06lu 0x%08lx %s\n", (__int64)t.tv_sec + SECS_BETWEEN_1900_1970, t.tv_usec, //SV-XXX: Win32
		        (unsigned long) rtp_my_ssrc(sp->rtp_session[0]), name);
#else
		fprintf(sp->logger, "silence    %ju.%06lu 0x%08lx %s\n", (intmax_t)t.tv_sec + SECS_BETWEEN_1900_1970, t.tv_usec, //SV-XXX: FreeBSD
		        (unsigned long) rtp_my_ssrc(sp->rtp_session[0]), name);
#endif
	}
}

void
ui_send_audio_channel_repair(session_t *sp, char *addr)
{
        const repair_details_t *r;
        uint16_t i, n;
        char *mbes;

	if (!sp->ui_on) return;
        n = repair_get_count();
        for (i = 0; i < n; i++) {
                r = repair_get_details(i);
                if (sp->repair == r->id) {
                        mbes = mbus_encode_str(r->name);
                        mbus_qmsg(sp->mbus_engine, addr, "audio.channel.repair", mbes, TRUE);
                        xfree(mbes);
                        return;
                }
        }
        debug_msg("Repair not found: %d\n", sp->repair);
}

/*
static void
ui_update_interleaving(session_t *sp)
{

        int pt, isep, iu;
        char buf[128], *sep=NULL, *units = NULL, *dummy;

	if (!sp->ui_on) return;
        pt = get_cc_pt(sp,"INTERLEAVER");
        if (pt != -1) {
                query_channel_coder(sp, pt, buf, 128);
                dummy  = strtok(buf,"/");
                units  = strtok(NULL,"/");
                sep    = strtok(NULL,"/");
        } else {
                debug_msg("Could not find interleaving channel coder!\n");
        }

        if (units != NULL && sep != NULL) {
                iu   = atoi(units);
                isep = atoi(sep);
        } else {
                iu   = 4;
                isep = 4;
        }

        mbus_qmsgf(sp->mbus_engine, sp->mbus_ui_addr, TRUE, "audio.channel.coding", "\"interleaved\" %d %d",iu, isep);

        UNUSED(sp);
}
*/

static void
ui_update_redundancy(session_t *sp, char *addr)
{
        const codec_format_t *scf;
        codec_id_t            scid;
        char *cmd, *out, *sec_enc, *sec_off, *mbes;

        int clen;

        clen = 2 * (CODEC_LONG_NAME_LEN + 4) + 1;
        cmd  = (char*)xmalloc(clen);

        channel_encoder_get_parameters(sp->channel_coder, cmd, clen);

        sec_enc = (char *) strtok(cmd, "/");  /* ignore primary encoding   */
        sec_enc = (char *) strtok(NULL, "/"); /* ignore primary offset     */
        sec_enc = (char *) strtok(NULL, "/"); /* get secondary encoding    */
        sec_off = (char *) strtok(NULL, "/"); /* get secondary offset      */

        if (sec_enc == NULL || sec_off == NULL) {
                goto redundancy_update_end;
        }

        scid    = codec_get_by_name(sec_enc);
        if (!codec_id_is_valid(scid)) {
                   goto redundancy_update_end;
        }

        scf = codec_get_format(scid);

	if (sp->logger != NULL) {
		struct timeval	t;
		gettimeofday(&t, NULL);
#ifdef WIN32
		fprintf(sp->logger, "channel    %I64u.%06lu 0x%08lx redundancy %s\n",
		        (__int64)t.tv_sec + SECS_BETWEEN_1900_1970, t.tv_usec, //SV-XXX: Win32
		        (unsigned long) rtp_my_ssrc(sp->rtp_session[0]), scf->long_name);
#else
		fprintf(sp->logger, "channel    %ju.%06lu 0x%08lx redundancy %s\n",
		        (intmax_t)t.tv_sec + SECS_BETWEEN_1900_1970, t.tv_usec, //SV-XXX: FreeBSD
		        (unsigned long) rtp_my_ssrc(sp->rtp_session[0]), scf->long_name);
#endif
	}

        out = (char*)xmalloc(clen);
        mbes = mbus_encode_str("redundancy");
        sprintf(out, "%s ", mbes);
        xfree(mbes);

        mbes = mbus_encode_str(scf->short_name);
        strcat(out, mbes);
        xfree(mbes);

        strcat(out, " ");
        strcat(out, sec_off);
        mbus_qmsg(sp->mbus_engine, addr, "audio.channel.coding", out, TRUE);
        xfree(out);

redundancy_update_end:
        xfree(cmd);
}

static void
ui_update_layering(session_t *sp, char *addr)
{
        const codec_format_t *lcf;
        codec_id_t            lcid;
        char *cmd, *out, *sec_enc, *layerenc, *mbes;

        int clen;

        clen = 2 * (CODEC_LONG_NAME_LEN + 4) + 1;
        cmd  = (char*)xmalloc(clen);

        channel_encoder_get_parameters(sp->channel_coder, cmd, clen);

        sec_enc = (char *) strtok(cmd, "/");
        layerenc = (char *) strtok(NULL, "/");

        if (sec_enc == NULL || layerenc == NULL) {
                goto layering_update_end;
        }

        lcid    = codec_get_by_name(sec_enc);
        if (!codec_id_is_valid(lcid)) {
                   goto layering_update_end;
        }

        lcf = codec_get_format(lcid);
        out = (char*)xmalloc(clen);

        mbes = mbus_encode_str("layering");
        sprintf(out, "%s ", mbes);
        xfree(mbes);

        mbes = mbus_encode_str(lcf->short_name);
        strcat(out, mbes);
        xfree(mbes);

        strcat(out, " ");
        strcat(out, layerenc);
        mbus_qmsg(sp->mbus_engine, addr, "audio.channel.coding", out, TRUE);
        xfree(out);

layering_update_end:
        xfree(cmd);
}

void
ui_send_audio_channel_coding(session_t *sp, char *addr)
{
        const cc_details_t *ccd;

	if (!sp->ui_on) return;
        ccd = channel_get_coder_identity(sp->channel_coder);
        switch(tolower((int)ccd->name[0])) { //SV-XXX: NetBSD
        case 'n':
                mbus_qmsg(sp->mbus_engine, addr, "audio.channel.coding", "\"none\"", TRUE);
		if (sp->logger != NULL) {
			struct timeval	t;
			gettimeofday(&t, NULL);
#ifdef WIN32
			fprintf(sp->logger, "channel    %I64u.%06lu 0x%08lx none\n",
				(__int64)t.tv_sec+SECS_BETWEEN_1900_1970, t.tv_usec, (unsigned long) rtp_my_ssrc(sp->rtp_session[0])); //SV-XXX: Win32
#else
			fprintf(sp->logger, "channel    %ju.%06lu 0x%08lx none\n",
				(intmax_t)t.tv_sec+SECS_BETWEEN_1900_1970, t.tv_usec, (unsigned long) rtp_my_ssrc(sp->rtp_session[0])); //SV-XXX: FreeBSD
#endif
		}
                break;
        case 'r':
                ui_update_redundancy(sp, addr);
                break;
        case 'l':
                ui_update_layering(sp, addr);
                break;
        }
        return;
}

void
ui_send_audio_codec(session_t *sp, char *addr)
{
	codec_id_t            pri_id;
        const codec_format_t *pri_cf;
        char *mbes;

	if (!sp->ui_on) return;
	pri_id = codec_get_by_payload(sp->encodings[0]);
        pri_cf = codec_get_format(pri_id);

	mbes = mbus_encode_str(pri_cf->short_name);
        mbus_qmsg(sp->mbus_engine, addr, "tool.rat.codec", mbes, FALSE);
        xfree(mbes);

	if (sp->logger != NULL) {
		struct timeval	t;
		gettimeofday(&t, NULL);
#ifdef WIN32
		fprintf(sp->logger, "codec      %I64u.%06lu 0x%08lx %s\n",
			(__int64)t.tv_sec+SECS_BETWEEN_1900_1970, t.tv_usec, (unsigned long) rtp_my_ssrc(sp->rtp_session[0]), //SV-XXX: Win32
			pri_cf->long_name);
#else
		fprintf(sp->logger, "codec      %ju.%06lu 0x%08lx %s\n",
			(intmax_t)t.tv_sec+SECS_BETWEEN_1900_1970, t.tv_usec, (unsigned long) rtp_my_ssrc(sp->rtp_session[0]), //SV-XXX: FreeBSD
			pri_cf->long_name);
#endif
	}
}

void
ui_send_audio_file_play_ready(session_t *sp, char *addr, char *name)
{
        char *mbes;
	if (!sp->ui_on) return;
        mbes = mbus_encode_str(name);
        mbus_qmsg(sp->mbus_engine, addr, "audio.file.play.ready", mbes, TRUE);
        xfree(mbes);
}

void
ui_send_audio_file_record_ready(session_t *sp, char *addr, char *name)
{
        char *mbes;
	if (!sp->ui_on) return;
        mbes = mbus_encode_str(name);
        mbus_qmsg(sp->mbus_engine, addr, "audio.file.record.ready", mbes, TRUE);
        xfree(mbes);
}

void
ui_send_audio_file_alive(session_t *sp, char *addr, char *mode, int valid)
{
        char cmd[32], arg[2];

	if (!sp->ui_on) return;
        assert(!strcmp(mode, "play") || !strcmp(mode, "record"));

        sprintf(cmd, "audio.file.%s.alive", mode);
        sprintf(arg, "%1d", valid);
        mbus_qmsg(sp->mbus_engine, addr, cmd, arg, TRUE);
}

void
ui_send_audio_3d_options(session_t *sp, char *addr)
{
        char args[256], tmp[5];
        char *mbes;
        int i, cnt;

	if (!sp->ui_on) return;
        args[0] = '\0';
        cnt = render_3D_filter_get_count();
        for(i = 0; i < cnt; i++) {
                strcat(args, render_3D_filter_get_name(i));
                if (i != cnt - 1) strcat(args, ",");
        }

        mbes = mbus_encode_str(args);
        mbus_qmsg(sp->mbus_engine, addr, "audio.3d.filter.types", mbes, TRUE);
        xfree(mbes);

        args[0] = '\0';
        cnt = render_3D_filter_get_lengths_count();
        for(i = 0; i < cnt; i++) {
                sprintf(tmp, "%d", render_3D_filter_get_length(i));
                strcat(args, tmp);
                if (i != cnt - 1) strcat(args, ",");
        }

        mbes = mbus_encode_str(args);
        mbus_qmsg(sp->mbus_engine, addr, "audio.3d.filter.lengths", mbes, TRUE);
        xfree(mbes);

        mbus_qmsgf(sp->mbus_engine, addr, TRUE, "audio.3d.azimuth.min", "%d", render_3D_filter_get_lower_azimuth());
        mbus_qmsgf(sp->mbus_engine, addr, TRUE, "audio.3d.azimuth.max", "%d", render_3D_filter_get_upper_azimuth());
}

void
ui_send_audio_3d_enabled(session_t *sp, char *addr)
{
	if (!sp->ui_on) return;
        mbus_qmsgf(sp->mbus_engine, addr, TRUE, "audio.3d.enabled", "%d", (sp->render_3d ? 1 : 0));
}

void
ui_send_audio_3d_settings(session_t *sp, char *addr, uint32_t ssrc)
{
        char *filter_name;
        int   azimuth, filter_type, filter_length;
        pdb_entry_t *p;

	if (!sp->ui_on) return;
        if (pdb_item_get(sp->pdb, ssrc, &p) == FALSE) {
                return;
        }

        if (p->render_3D_data == NULL) {
                p->render_3D_data = render_3D_init(ts_get_freq(sp->cur_ts));
        }

        render_3D_get_parameters(p->render_3D_data, &azimuth, &filter_type, &filter_length);
        filter_name = mbus_encode_str(render_3D_filter_get_name(filter_type));
        mbus_qmsgf(sp->mbus_engine, addr, TRUE, "audio.3d.user.settings", "\"%08lx\" %s %d %d", (unsigned long) ssrc, filter_name, filter_length, azimuth);
        xfree(filter_name);
}

void
ui_send_audio_update(session_t *sp, char *addr)
{
        ui_send_audio_device_list     (sp, addr);
        ui_send_audio_device          (sp, addr);
	ui_send_audio_output_port_list(sp, addr);
        ui_send_audio_output_port     (sp, addr);
        ui_send_audio_output_mute     (sp, addr);
        ui_send_audio_output_gain     (sp, addr);
	ui_send_audio_input_port_list (sp, addr);
	ui_send_audio_input_port      (sp, addr);
	ui_send_audio_input_mute      (sp, addr);
	ui_send_audio_input_gain      (sp, addr);
        ui_send_audio_suppress_silence(sp, addr);
        ui_send_audio_channel_coding  (sp, addr);
        ui_send_audio_channel_repair  (sp, addr);
	ui_send_audio_codec           (sp, addr);
        ui_send_audio_3d_options      (sp, addr);
        ui_send_audio_3d_enabled      (sp, addr);
}

