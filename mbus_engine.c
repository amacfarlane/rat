/*
* FILE:    mbus_engine.c
* AUTHORS: Colin Perkins
* MODIFICATIONS: Orion Hodson
*
* Copyright (c) 1998-2001 University College London
* All rights reserved.
*/

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
"$Id: mbus_engine.c 4549 2010-01-07 12:51:14Z piers $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "mbus_engine.h"
#include "mbus.h"
#include "mbus_parser.h"
#include "net_udp.h"
#include "session.h"
#include "net.h"
#include "transmit.h"
#include "codec_types.h"
#include "codec.h"
#include "audio.h"
#include "session.h"
#include "channel.h"
#include "converter.h"
#include "repair.h"
#include "render_3D.h"
#include "settings.h"
#include "pdb.h"
#include "source.h"
#include "sndfile.h"
#include "voxlet.h"
#include "tonegen.h"
#include "util.h"
#include "codec_types.h"
#include "channel_types.h"
#include "parameters.h"
#include "rtp.h"
#include "rtp_callback.h"
#include "ui_send_rtp.h"
#include "ui_send_audio.h"
#include "ui_send_prefs.h"
#include "version.h"

#define SECS_BETWEEN_1900_1970 2208988800u

extern int 	 should_exit;
extern FILE 	*stats_file;

extern void rtp_callback_open_logfile(char *file);

/* Mbus command reception function type */
typedef void (*mbus_rx_proc)(char *srce, char *args, session_t *sp);

/* Tuple to associate string received with it's parsing fn */
typedef struct {
	const char   *rxname;
	mbus_rx_proc  rxproc;
} mbus_cmd_tuple;

static void rx_session_title(char *srce, char *args, session_t *sp)
{
	char			*title;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &title)) {
		sp->title = xstrdup(mbus_decode_str(title));
	} else {
		debug_msg("mbus: usage \"session.title <title>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_powermeter(char *srce, char *args, session_t *sp)
{
	int 			 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
		sp->meter = i;
		ui_send_audio_input_powermeter(sp, sp->mbus_ui_addr, 0);
		ui_send_audio_output_powermeter(sp, sp->mbus_ui_addr, 0);
	} else {
		debug_msg("mbus: usage \"tool.rat.powermeter <boolean>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_silence(char *srce, char *args, session_t *sp)
{
	char *detector;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &detector)) {
		mbus_decode_str(detector);
		sp->silence_detection = sd_name_to_type(detector);
		debug_msg("detector:%s index %d (%s)\n", detector, sp->silence_detection, srce);
	} else {
		debug_msg("mbus: usage \"tool.rat.silence <Auto|Manual|Off>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_silence_thresh(char *srce, char *args, session_t *sp)
{
	int 			 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
		sp->manual_sd_thresh = i;
		manual_sd_set_thresh(sp->manual_sd, (uint16_t)i);
		debug_msg("Setting threshold: %d\n", i);
	} else {
		debug_msg("mbus: usage \"tool.rat.silence.threshold <int>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_audio_3d_enable(char *srce, char *args, session_t *sp)
{
	int 			 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
		audio_device_register_change_render_3d(sp, i);
	} else {
		debug_msg("mbus: usage \"audio.3d.enabled <boolean>\"\n");
	}
	mbus_parse_done(mp);
}

static void
rx_audio_3d_user_settings(char *srce, char *args, session_t *sp)
{
	pdb_entry_t             *p;
	char 			*filter_name;
	int 			 filter_type, filter_length, azimuth, freq;
	char			*ss;
	uint32_t                 ssrc;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &ss) &&
		mbus_parse_str(mp, &filter_name) &&
		mbus_parse_int(mp, &filter_length) &&
		mbus_parse_int(mp, &azimuth)) {

			mbus_decode_str(filter_name);
			ss = mbus_decode_str(ss);
			ssrc = strtoul(ss, 0, 16);

			if (pdb_item_get(sp->pdb, ssrc, &p)) {
				filter_type = render_3D_filter_get_by_name(filter_name);
				freq        = ts_get_freq(sp->cur_ts);
				if (p->render_3D_data == NULL) {
					p->render_3D_data = render_3D_init(freq);
				}
				render_3D_set_parameters(p->render_3D_data,
					freq,
					azimuth,
					filter_type,
					filter_length);
			} else {
				debug_msg("Unknown source 0x%08lx\n", ssrc);
			}
		} else {
			debug_msg("mbus: usage \"audio.3d.user.settings <cname> <filter name> <filter len> <azimuth>\"\n");
		}
		mbus_parse_done(mp);
}

static void
rx_audio_3d_user_settings_req(char *srce, char *args, session_t *sp)
{
	char			*ss;
	uint32_t         	 ssrc;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &ss)) {
		ss   = mbus_decode_str(ss);
		ssrc = strtoul(ss, 0, 16);
		ui_send_audio_3d_settings(sp, sp->mbus_ui_addr, ssrc);
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_lecture_mode(char *srce, char *args, session_t *sp)
{
	int 		  	 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
		sp->lecture = i;
	} else {
		debug_msg("mbus: usage \"tool.rat.lecture.mode <boolean>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_agc(char *srce, char *args, session_t *sp)
{
	int 		  	 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
		sp->agc_on = i;
	} else {
		debug_msg("mbus: usage \"tool.rat.agc <boolean>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_filter_loopback(char *srce, char *args, session_t *sp)
{
	int 		  	 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
		sp->filter_loopback = i;
	} else {
		debug_msg("mbus: usage \"tool.rat.filter.loopback <boolean>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_loopback_gain(char *srce, char *args, session_t *sp)
{
	int 		  	 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
		if (i) {
			audio_loopback(sp->audio_device, 100);
			sp->loopback_gain = 100;
		} else {
			audio_loopback(sp->audio_device, 0);
			sp->loopback_gain = 0;
		}
	} else {
		debug_msg("mbus: usage \"tool.rat.loopback.gain <boolean>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_echo_suppress(char *srce, char *args, session_t *sp)
{
	int 		  	 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
		sp->echo_suppress = i;
		if (sp->echo_suppress     == FALSE &&
			sp->echo_tx_active    == TRUE  &&
			tx_is_sending(sp->tb) == FALSE) {
				/* Suppressor has just been disabled,  transmitter  */
				/* is in suppressed state and would otherwise be    */
				/* active.  Therefore start it up now.              */
				tx_start(sp->tb);
			}
	} else {
		debug_msg("mbus: usage \"tool.rat.echo.suppress <boolean>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_rate(char *srce, char *args, session_t *sp)
{
	int 		  	 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
		assert(sp->channel_coder != NULL);
		channel_encoder_set_units_per_packet(sp->channel_coder, (uint16_t)i);
		ui_send_rate(sp, sp->mbus_ui_addr);
	} else {
		debug_msg("mbus: usage \"tool.rat.rate <integer>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_audio_input_mute(char *srce, char *args, session_t *sp)
{
	int 		  	 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
		if (i) {
			if (tx_is_sending(sp->tb)) {
				tx_stop(sp->tb);
			}
		} else {
			if (tx_is_sending(sp->tb) == FALSE) {
				tx_start(sp->tb);
			}
		}
		/* Keep echo suppressor informed of change */
		sp->echo_tx_active = !i;
		ui_send_audio_input_port(sp, sp->mbus_ui_addr);
		ui_send_audio_input_mute(sp, sp->mbus_ui_addr);
		ui_send_audio_input_gain(sp, sp->mbus_ui_addr);
	} else {
		debug_msg("mbus: usage \"audio.input.mute <boolean>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_audio_input_gain(char *srce, char *args, session_t *sp)
{
	int 		  	 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i) &&
		(i >= 0 && i <= 100)) {
			audio_set_igain(sp->audio_device, i);
			tx_igain_update(sp->tb);
		} else {
			debug_msg("mbus: usage \"audio.input.gain <integer>\"\n");
		}
		mbus_parse_done(mp);
}

static void rx_audio_input_port(char *srce, char *args, session_t *sp)
{
	const audio_port_details_t *apd = NULL;
	char			*s;
	int      		 i, n, found;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &s)) {
		s = mbus_decode_str(s);
		n = audio_get_iport_count(sp->audio_device);
		found = FALSE;
		for(i = 0; i < n; i++) {
			apd = audio_get_iport_details(sp->audio_device, i);
			if (!strcasecmp(s, apd->name)) {
				found = TRUE;
				break;
			}
		}
		if (found == FALSE) {
			debug_msg("%s does not match any port names\n", s);
			apd = audio_get_iport_details(sp->audio_device, 0);
		}
		audio_set_iport(sp->audio_device, apd->port);
	} else {
		debug_msg("mbus: usage \"audio.input.port <port>\"\n");
	}
	mbus_parse_done(mp);
	ui_send_audio_input_port(sp, sp->mbus_ui_addr);
	ui_send_audio_input_mute(sp, sp->mbus_ui_addr);
	ui_send_audio_input_gain(sp, sp->mbus_ui_addr);
}

static void rx_audio_output_mute(char *srce, char *args, session_t *sp)
{
	struct mbus_parser	*mp;
	struct s_source 	*s;
	int 			 i, n;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
		sp->playing_audio = !i;
		ui_send_audio_output_port(sp, sp->mbus_ui_addr);
		n = (int)source_list_source_count(sp->active_sources);
		for (i = 0; i < n; i++) {
			s = source_list_get_source_no(sp->active_sources, i);
			ui_send_rtp_inactive(sp, sp->mbus_ui_addr, source_get_ssrc(s));
			source_remove(sp->active_sources, s);
			/* revise source no's since we removed a source */
			i--;
			n--;
		}
	} else {
		debug_msg("mbus: usage \"audio.output.mute <boolean>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_audio_output_gain(char *srce, char *args, session_t *sp)
{
	struct mbus_parser	*mp;
	int			 i;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i) &&
		(i >= 0 && i <= 100)) {
			audio_set_ogain(sp->audio_device, i);
		} else {
			debug_msg("mbus: usage \"audio.output.gain <integer>\"\n");
		}
		mbus_parse_done(mp);
}

static void rx_audio_output_port(char *srce, char *args, session_t *sp)
{
	const audio_port_details_t *apd = NULL;
	char *s;
	int   i, n, found;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &s)) {
		s = mbus_decode_str(s);
		n     = audio_get_oport_count(sp->audio_device);
		found = FALSE;

		for(i = 0; i < n; i++) {
			apd = audio_get_oport_details(sp->audio_device, i);
			if (!strcasecmp(s, apd->name)) {
				found = TRUE;
				break;
			}
		}
		if (found == FALSE) {
			debug_msg("%s does not match any port names\n", s);
			apd = audio_get_oport_details(sp->audio_device, 0);
		}
		audio_set_oport(sp->audio_device, apd->port);
	} else {
		debug_msg("mbus: usage \"audio.output.port <port>\"\n");
	}
	mbus_parse_done(mp);
	ui_send_audio_output_port(sp, sp->mbus_ui_addr);
	ui_send_audio_output_mute(sp, sp->mbus_ui_addr); //SV-XXX: NetBSD
	ui_send_audio_output_gain(sp, sp->mbus_ui_addr); //SV-XXX: NetBSD
}

static void rx_audio_channel_repair(char *srce, char *args, session_t *sp)
{
	const repair_details_t *r;
	uint16_t i, n;
	char	*s;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &s)) {
		s = mbus_decode_str(s);
		if (strcasecmp(s, "first") == 0) {
			r = repair_get_details(0);
			sp->repair = r->id;
		} else {
			n = repair_get_count();
			for(i = 0; i < n; i++) {
				r = repair_get_details(i);
				if (strcasecmp(r->name, s) == 0 || strcasecmp("first", s) == 0) {
					sp->repair = r->id;
					break;
				}
			}
		}
	} else {
		debug_msg("mbus: usage \"audio.channel.repair <repair>\"\n");
	}
	mbus_parse_done(mp);
	ui_send_audio_channel_repair(sp, sp->mbus_ui_addr);
}

static void rx_security_encryption_key(char *srce, char *args, session_t *sp)
{
	int      i;
	char	*key;
	struct mbus_parser	*mp;

	UNUSED(sp);
	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &key)) {
		debug_msg("rx_security_encryption_key: key: %s\n",key);
		key = mbus_decode_str(key);
		for(i = 0; i < sp->rtp_session_count; i++) {
			if (strlen(key) == 0) {
				rtp_set_encryption_key(sp->rtp_session[i], NULL);
				sp->encrkey = NULL;
				ui_send_encryption_key(sp, sp->mbus_ui_addr);
			} else {
				rtp_set_encryption_key(sp->rtp_session[i], key);
				sp->encrkey = xstrdup(key);
				ui_send_encryption_key(sp, sp->mbus_ui_addr);
			}
		}
	} else {
		debug_msg("mbus: usage \"security.encryption.key <key>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_audio_file_play_stop(char *srce, char *args, session_t *sp)
{
	UNUSED(srce);
	UNUSED(args);

	if (sp->in_file != NULL) {
		snd_read_close(&sp->in_file);
	}
}

static void rx_tool_rat_voxlet_play(char *srce, char *args, session_t *sp)
{
	char *file;
	struct mbus_parser *mp;

	UNUSED(srce);
	UNUSED(sp);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &file)) {
		mbus_decode_str(file);
		if (sp->local_file_player) {
			voxlet_destroy(&sp->local_file_player);
		}
		voxlet_create(&sp->local_file_player, sp->ms, sp->pdb, file);
	} else {
		debug_msg("mbus: usage \"tool.rat.voxlet.play <filename>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_logstats(char *srce, char *args, session_t *sp)
{
	int 		  	 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
		if (i) {
			struct timeval   t;
			char             fname[100];
			char		 hname[64];
			char		*uname;
			const char	*cname;
#ifndef WIN32
			struct passwd  *pwent;

			pwent = getpwuid(getuid());
			uname = pwent->pw_name;

#else
			char	user[100];
			int	size = 100;

			if (!GetUserName(user, &size)) {
				uname = "UNKNOWN";
			} else {
				uname = (char *) user;
			}
#endif
			gettimeofday(&t, NULL);
			gethostname(hname, 64);
#ifdef WIN32
			sprintf(fname, "rat-%p-%I64d.%06ld-%s-%s.log", sp, (__int64)t.tv_sec, (long)t.tv_usec, hname, uname); //SV-XXX: Win32
#else
			sprintf(fname, "rat-%p-%jd.%06ld-%s-%s.log", sp, (intmax_t)t.tv_sec, (long)t.tv_usec, hname, uname); //SV-XXX: FreeBSD
#endif
			cname = rtp_get_sdes(sp->rtp_session[0], rtp_my_ssrc(sp->rtp_session[0]), RTCP_SDES_CNAME);

			sp->logger = fopen(fname, "w");
#ifdef WIN32
			fprintf(sp->logger, "tool_start %I64u.%06lu ", (__int64)t.tv_sec + SECS_BETWEEN_1900_1970, (long)t.tv_usec); //SV-XXX: Win32
#else
			fprintf(sp->logger, "tool_start %ju.%06lu ", (intmax_t)t.tv_sec + SECS_BETWEEN_1900_1970, (long)t.tv_usec); //SV-XXX: FreeBSD
#endif
			fprintf(sp->logger, "0x%08lx\n", (unsigned long) rtp_my_ssrc(sp->rtp_session[0]));
		} else {
			fclose(sp->logger);
		}
	} else {
		debug_msg("mbus: usage \"tool.rat.logstats <boolean>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_logdebug(char *srce, char *args, session_t *sp)
{
    int enabled;
    char *file;
	struct mbus_parser	*mp;

	UNUSED(srce);
	UNUSED(sp);
	
	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &enabled) &&
	    mbus_parse_str(mp, &file))
	    {
		mbus_decode_str(file);
		debug_msg("Logstats enabled=%d file=%s\n", enabled, file);
		if (enabled) {
		    rtp_callback_open_logfile(file);
		}
	} else {
		debug_msg("mbus: usage \"tool.rat.logstats <boolean>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_tone_start(char *srce, char *args, session_t *sp)
{
	int freq, amp;
	struct mbus_parser *mp;

	UNUSED(srce);

	if (sp->tone_generator) {
		tonegen_destroy(&sp->tone_generator);
	}

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &freq) &&
		mbus_parse_int(mp, &amp)) {
			tonegen_create(&sp->tone_generator, sp->ms, sp->pdb, (uint16_t)freq, (uint16_t)amp);
		} else {
			debug_msg("mbus: usage \"tool.rat.tone.start <freq> <amplitude>\"\n");
		}
		mbus_parse_done(mp);
}

static void rx_tool_rat_tone_stop(char *srce, char *args, session_t *sp)
{
	UNUSED(srce);
	UNUSED(args);
	if (sp->tone_generator) {
		tonegen_destroy(&sp->tone_generator);
	}
}

static void rx_audio_file_play_open(char *srce, char *args, session_t *sp)
{
	char	*file;
	struct mbus_parser	*mp;

	UNUSED(srce);
	UNUSED(sp);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &file)) {
		mbus_decode_str(file);
		if (sp->in_file) snd_read_close(&sp->in_file);
		if (snd_read_open(&sp->in_file, file, NULL)) {
			debug_msg("Hooray opened %s\n",file);
		}
	} else {
		debug_msg("mbus: usage \"audio.file.play.open <filename>\"\n");
	}
	mbus_parse_done(mp);

	if (sp->in_file) {
		ui_send_audio_file_play_ready(sp, sp->mbus_ui_addr, file);
	}
}

static void rx_audio_file_play_pause(char *srce, char *args, session_t *sp)
{
	int pause;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);

	if (mbus_parse_int(mp, &pause)) {
		if (sp->in_file) {
			if (pause) {
				snd_pause(sp->in_file);
			} else {
				snd_resume(sp->in_file);
			}
		}
	} else {
		debug_msg("mbus: usage \"audio.file.play.pause <bool>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_audio_file_play_live(char *srce, char *args, session_t *sp)
{
	/* This is a request to see if file we are playing is still valid */
	UNUSED(args);
	UNUSED(srce);
	ui_send_audio_file_alive(sp, sp->mbus_ui_addr, "play", (sp->in_file) ? 1 : 0);
}

static void rx_audio_file_rec_stop(char *srce, char *args, session_t *sp)
{
	UNUSED(srce);
	UNUSED(args);

	if (sp->out_file != NULL) {
		snd_write_close(&sp->out_file);
	}
}

static void rx_audio_file_rec_open(char *srce, char *args, session_t *sp)
{
	char	*file;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &file)) {
		sndfile_fmt_t sf_fmt;
		const audio_format *ofmt;
		ofmt = audio_get_ofmt(sp->audio_device);
		mbus_decode_str(file);
		if (sp->out_file) snd_write_close(&sp->out_file);

		sf_fmt.encoding    = SNDFILE_ENCODING_L16;
		sf_fmt.sample_rate = (uint32_t)ofmt->sample_rate;
		sf_fmt.channels    = (uint16_t)ofmt->channels;
#ifdef WIN32
		if (snd_write_open(&sp->out_file, file, "wav", &sf_fmt)) {
			debug_msg("Hooray opened %s\n",file);
		}
#else
		if (snd_write_open(&sp->out_file, file, "au", &sf_fmt)) {
			debug_msg("Hooray opened %s\n",file);
		}
#endif /* WIN32 */
	} else {
		debug_msg("mbus: usage \"audio.file.record.open <filename>\"\n");
	}
	mbus_parse_done(mp);

	if (sp->out_file) ui_send_audio_file_record_ready(sp, sp->mbus_ui_addr, file);
}

static void rx_audio_file_rec_pause(char *srce, char *args, session_t *sp)
{
	int pause;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);

	if (mbus_parse_int(mp, &pause)) {
		if (sp->out_file) {
			if (pause) {
				snd_pause(sp->out_file);
			} else {
				snd_resume(sp->out_file);
			}
		}
	} else {
		debug_msg("mbus: usage \"audio.file.record.pause <bool>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_audio_file_rec_live(char *srce, char *args, session_t *sp)
{
	/* This is a request to see if file we are recording is still valid */
	UNUSED(args);
	UNUSED(srce);
	ui_send_audio_file_alive(sp, sp->mbus_ui_addr, "record", (sp->out_file) ? 1 : 0);
}

static void
rx_audio_device(char *srce, char *args, session_t *sp)
{
	char	*s, dev_name[64], first_dev_name[64];
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &s)) {
		s = mbus_decode_str(s);
		purge_chars(s, "[]()");
		if (s) {
			const audio_device_details_t *add = NULL;
			audio_desc_t           first_dev_desc = 0;
			uint32_t i, n, stop_at_first_device = FALSE;
			dev_name[0] = 0;
			first_dev_name[0] = 0;
			n = audio_get_device_count();

			if (!strncasecmp("first", s, 5)) {
				/* The ui may send first if the saved device is the null audio
				* device so it starts up trying to play something.
				*/
				stop_at_first_device = TRUE;
			}

			for(i = 0; i < n; i++) {
				/* Brackets are a problem so purge them */
				add = audio_get_device_details(i);
				strncpy(dev_name, add->name, AUDIO_DEVICE_NAME_LENGTH);
				purge_chars(dev_name, "[]()");
				if (first_dev_name[0] == 0) {
					strncpy(first_dev_name, dev_name, AUDIO_DEVICE_NAME_LENGTH);
					first_dev_desc = add->descriptor;
				}

				if (!strcmp(s, dev_name) | stop_at_first_device) {
					break;
				}
			}
			if (i < n) {
				/* Found device looking for */
				audio_device_register_change_device(sp, add->descriptor);
			} else if (first_dev_name[0]) {
				/* Have a fall back */
				audio_device_register_change_device(sp, first_dev_desc);
			}
		}
	} else {
		debug_msg("mbus: usage \"audio.device <string>\"\n");
	}
	mbus_parse_done(mp);
}

static void
rx_audio_query(char *srce, char *args, session_t *sp)
{
	/* The audio.query() command solicits information about the audio device. */
	/* We respond by dumping all our audio related state to the querier.     */
	UNUSED(args);
	ui_send_audio_update(sp, srce);
}

static void rx_rtp_source_sdes(char *srce, char *args, session_t *sp, uint8_t type)
{
	char	           *arg, *ss;
	uint32_t           ssrc;
	struct mbus_parser *mp;
	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &ss) &&
		mbus_parse_str(mp, &arg)) {
			uint32_t my_ssrc = rtp_my_ssrc(sp->rtp_session[0]);
			ss = mbus_decode_str(ss);
			if (isalpha((int)ss[0])) {
				/*
				* Allow alpha so people can do my_src, me,
				* local_user, whatever.  Let the mbus police
				* pick something sane.
				*/
				ssrc = my_ssrc;
			} else {
				ssrc = strtoul(ss, 0, 16);
			}
			if (ssrc == my_ssrc) {
				char *value;
				int i, vlen;
				value = mbus_decode_str(arg);
				vlen  = strlen(value);
				for (i = 0; i < sp->rtp_session_count; i++) {
					rtp_set_sdes(sp->rtp_session[i], ssrc, type, value, vlen);
				}
			} else {
				debug_msg("mbus: ssrc %s (%08lx) != %08lx\n", ss, strtoul(ss, 0, 16), rtp_my_ssrc(sp->rtp_session[0]));
			}
		} else {
			debug_msg("mbus: usage \"rtp_source_<sdes_item> <ssrc> <name>\"\n");
		}
		mbus_parse_done(mp);
}

static void rx_rtp_query(char *srce, char *args, session_t *sp)
{
	/* The rtp.query() command solicits information about the RTP session. */
	/* We respond by dumping all our RTP related state to the querier.     */
	uint32_t	 ssrc, my_ssrc;
	struct s_source	*s;
    pdb_entry_t *e;

	UNUSED(args);
	debug_msg("mbus: rx_rtp_query\n");
	ui_send_rtp_ssrc(sp, srce);
	pdb_get_first_id(sp->pdb, &ssrc);
	my_ssrc = rtp_my_ssrc(sp->rtp_session[0]);

	do {
		ui_send_rtp_cname(sp, srce, ssrc);
		ui_send_rtp_name(sp, srce, ssrc);
		ui_send_rtp_email(sp, srce, ssrc);
		ui_send_rtp_phone(sp, srce, ssrc);
		ui_send_rtp_loc(sp, srce, ssrc);
		ui_send_rtp_tool(sp, srce, ssrc);
		ui_send_rtp_note(sp, srce, ssrc);
		ui_send_rtp_mute(sp, srce, ssrc);

		if (pdb_item_get(sp->pdb, ssrc, &e) == TRUE) {
		  if (e->siteid != NULL) {
		    ui_send_rtp_app_site(sp, srce, ssrc, e->siteid);
		  }
		}

		if (ssrc != my_ssrc) {
			if ((s = source_get_by_ssrc(sp->active_sources, ssrc)) != NULL) {
				ui_send_rtp_active(sp, srce, ssrc);
			} else {
				ui_send_rtp_inactive(sp, srce, ssrc);
			}
		} else {
			tx_update_ui(sp->tb);
		}
	} while (pdb_get_next_id(sp->pdb, ssrc, &ssrc));
	ui_send_rtp_addr(sp, srce);
	ui_send_rtp_title(sp, srce);
}

static void rx_rtp_addr_query(char *srce, char *args, session_t *sp)
{
	UNUSED(args);
	ui_send_rtp_addr(sp, srce);
}

static void rx_rtp_source_mute(char *srce, char *args, session_t *sp)
{
	/* Sources are active whilst packets are arriving and maintaining      */
	/* statistics on sender.  This is good, but we need to remove source   */
	/* when changing mute state, if packets are still arriving source will */
	/* be recreated when next packet arrives.  When muting we want to      */
	/* remove source to stop audio already buffered from playing.  When    */
	/* unmuting want to remove source to initialize state, particularly    */
	/* timestamps of last repair etc.                                      */

	pdb_entry_t 		*pdbe;
	char        		*ssrc;
	int         		 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &ssrc) && mbus_parse_int(mp, &i)) {
		ssrc = mbus_decode_str(ssrc);
		if (strcmp(ssrc, "ALL") == 0) {
			uint32_t	id;
			pdb_get_first_id(sp->pdb, &id);
			do {
				if (pdb_item_get(sp->pdb, id, &pdbe)) {
					struct s_source *s = source_get_by_ssrc(sp->active_sources, pdbe->ssrc);
					if (s != NULL) {
						source_remove(sp->active_sources, s);
					}
					pdbe->mute = i;
					ui_send_rtp_mute(sp, sp->mbus_ui_addr, pdbe->ssrc);
					debug_msg("mute ssrc 0x%08x (%d)\n", pdbe->ssrc, i);
				} else {
					debug_msg("Unknown source 0x%08lx\n", ssrc);
				}
			} while (pdb_get_next_id(sp->pdb, id, &id));
		} else {
			if (pdb_item_get(sp->pdb, strtoul(ssrc, 0, 16), &pdbe)) {
				struct s_source *s = source_get_by_ssrc(sp->active_sources, pdbe->ssrc);
				if (s != NULL) {
					source_remove(sp->active_sources, s);
				}
				pdbe->mute = i;
				ui_send_rtp_mute(sp, sp->mbus_ui_addr, pdbe->ssrc);
				debug_msg("mute ssrc 0x%08x (%d)\n", pdbe->ssrc, i);
			} else {
				debug_msg("Unknown source 0x%08lx\n", ssrc);
			}
		}
	} else {
		debug_msg("mbus: usage \"rtp_source_mute <ssrc> <bool>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_rtp_addr(char *srce, char *args, session_t *sp)
{
	/* rtp.addr ("224.1.2.3" 1234 1234 16) */
	char	*addr;
	int	 rx_port, tx_port, ttl;
	struct mbus_parser	*mp;
	uint32_t           ssrc;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	mbus_parse_str(mp, &addr); addr = mbus_decode_str(addr);
	mbus_parse_int(mp, &rx_port);
	mbus_parse_int(mp, &tx_port);
	mbus_parse_int(mp, &ttl);
	mbus_parse_done(mp);
	debug_msg("rx_rtp_addr: New Addr:%s,rx_port:%d,tx_port:%d,ttl:%d\n",addr, rx_port, tx_port, ttl);

	if (sp->rtp_session_count) {
		/* Existing session present - delete and recreate with new params */
		sp->rtp_session_count--;
		rx_audio_input_mute(srce, "1", sp);
		rtp_set_encryption_key(sp->rtp_session[0], NULL);
		sp->encrkey = NULL;
		ui_send_encryption_key(sp, sp->mbus_ui_addr);
		settings_save(sp);
		rtp_send_bye(sp->rtp_session[sp->rtp_session_count]);
		rtp_callback_exit(sp->rtp_session[sp->rtp_session_count]);
		/* Remove existing sources from DB and UI , then destory DB and RTP sess */
		pdb_get_first_id(sp->pdb, &ssrc);
		do {
			struct s_source *s = source_get_by_ssrc(sp->active_sources, ssrc);
			if (s != NULL) {
				source_remove(sp->active_sources, s);
			}
			ui_send_rtp_remove(sp, sp->mbus_ui_addr, ssrc);
		} while (pdb_get_next_id(sp->pdb, ssrc, &ssrc));
		pdb_destroy(&sp->pdb);
		rtp_done(sp->rtp_session[sp->rtp_session_count]);

		/* Perform rtp session re-creation */
		if (!(sp->rtp_session[sp->rtp_session_count] = 
			rtp_init(addr, (uint16_t)rx_port, (uint16_t)tx_port, ttl, 64000, rtp_callback_proc, NULL))) {
				debug_msg("Failed to reinitialise rtp session - exiting");
				assert(sp->rtp_session[sp->rtp_session_count]);
		}
		rtp_set_option(sp->rtp_session[sp->rtp_session_count], RTP_OPT_PROMISC, sp->rtp_promiscuous_mode);
		rtp_set_option(sp->rtp_session[sp->rtp_session_count], RTP_OPT_WAIT_FOR_RTCP, sp->rtp_wait_for_rtcp);
		rtp_callback_init(sp->rtp_session[0], sp);

		if(sp->rtp_session_count < sp->layers && sp->rtp_session_count > 0) {
			rtp_set_my_ssrc(sp->rtp_session[sp->rtp_session_count], rtp_my_ssrc(sp->rtp_session[0]));
		}
		sp->rtp_session_count++;
		/* Load saved settings, and create the participant database... */
		/* FIXME: probably needs updating for the transcoder so we can */
		/*        have different saved settings for each domain.       */
		/* FIXME: this gets the wrong device name for the transcoder.  */
		if (pdb_create(&sp->pdb) == FALSE) {
			debug_msg("Failed to re-create new participant database\n");
			abort();
		}
		pdb_item_create(sp->pdb, (uint16_t)ts_get_freq(sp->cur_ts), rtp_my_ssrc(sp->rtp_session[0]));
		settings_load_late(sp);
		session_validate(sp);

		// Update src in UI
		rx_rtp_query(srce, args, sp);
	} else {
		sp->rtp_session[sp->rtp_session_count] = rtp_init(addr, (uint16_t)rx_port, (uint16_t)tx_port, ttl, 64000, rtp_callback_proc, NULL);

		rtp_set_option(sp->rtp_session[sp->rtp_session_count], RTP_OPT_PROMISC, sp->rtp_promiscuous_mode);
		rtp_set_option(sp->rtp_session[sp->rtp_session_count], RTP_OPT_WAIT_FOR_RTCP, sp->rtp_wait_for_rtcp);
		rtp_callback_init(sp->rtp_session[0], sp);

		if(sp->rtp_session_count < sp->layers && sp->rtp_session_count > 0) {
			rtp_set_my_ssrc(sp->rtp_session[sp->rtp_session_count], rtp_my_ssrc(sp->rtp_session[0]));
		}
		sp->rtp_session_count++;
	}
}


static void rx_rtp_source_name(char *srce, char *args, session_t *sp)
{
	rx_rtp_source_sdes(srce, args, sp, RTCP_SDES_NAME);
}

static void rx_rtp_source_email(char *srce, char *args, session_t *sp)
{
	rx_rtp_source_sdes(srce, args, sp, RTCP_SDES_EMAIL);
}

static void rx_rtp_source_phone(char *srce, char *args, session_t *sp)
{
	rx_rtp_source_sdes(srce, args, sp, RTCP_SDES_PHONE);
}

static void rx_rtp_source_loc(char *srce, char *args, session_t *sp)
{
	rx_rtp_source_sdes(srce, args, sp, RTCP_SDES_LOC);
}

static void rx_rtp_source_note(char *srce, char *args, session_t *sp)
{
	rx_rtp_source_sdes(srce, args, sp, RTCP_SDES_NOTE);
}

static void rx_rtp_source_app_site(char *srce, char *args, session_t *sp)
{
        char               *arg, *ss;
        uint32_t           ssrc;
        struct mbus_parser *mp;
        UNUSED(srce);

        mp = mbus_parse_init(args);
        if (mbus_parse_str(mp, &ss) &&
            mbus_parse_str(mp, &arg)) {
                uint32_t my_ssrc = rtp_my_ssrc(sp->rtp_session[0]);
                ss = mbus_decode_str(ss);
                if (isalpha((int)ss[0])) {
                        /*
                         * Allow alpha so people can do my_src, me,
                         * local_user, whatever.  Let the mbus police
                         * pick something sane.
                         */
                        ssrc = my_ssrc;
                } else {
                        ssrc = strtoul(ss, 0, 16);
                }
                if (ssrc == my_ssrc) {
                        char *value;

                        value = mbus_decode_str(arg);

                        if (sp->rtp_session_app_site != NULL) {
                                xfree(sp->rtp_session_app_site);
                        }
                        sp->rtp_session_app_site = xstrdup(value);
                } else {
                        debug_msg("mbus: rtp_source_app_site ssrc %s (%08lx) != %08lx\n", ss, strtoul(ss, 0, 16), rtp_my_ssrc(sp->rtp_session[0]));
                }
        } else {
                debug_msg("mbus: usage \"rtp_source_app_site <ssrc> <siteid>\"\n");
        }
        mbus_parse_done(mp);
}

static void rx_rtp_source_gain(char *srce, char *args, session_t *sp)
{
	pdb_entry_t	*pdbe;
	char		*ssrc;
	double           g;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &ssrc) && mbus_parse_flt(mp, &g)) {
		ssrc = mbus_decode_str(ssrc);
		if (pdb_item_get(sp->pdb, strtoul(ssrc, 0, 16), &pdbe)) {
			pdbe->gain = g;
		} else {
			debug_msg("Unknown source 0x%08lx\n", ssrc);
		}
	} else {
		debug_msg("mbus: usage \"rtp_source_gain <ssrc> <bool>\"\n");
	}
	mbus_parse_done(mp);
}

static int
string_to_freq(const char *str) {
	int freq = atoi(str) * 1000;
	if ((freq % 11000) == 0) {
		freq /= 11000;
		freq *= 11025;
	}
	return freq;
}

static void
rx_tool_rat_codec(char *srce, char *args, session_t *sp)
{
	char	*short_name, *sfreq, *schan;
	int      freq, channels;
	codec_id_t cid, next_cid;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &short_name) &&
		mbus_parse_str(mp, &schan) &&
		mbus_parse_str(mp, &sfreq)) {
			mbus_decode_str(short_name);
			mbus_decode_str(schan);
			mbus_decode_str(sfreq);
			mbus_parse_done(mp);
		} else {
			debug_msg("mbus: usage \"tool.rat.codec <codec> <channels> <freq>\"\n");
			mbus_parse_done(mp);
			return;
		}

		if (strcasecmp(schan, "mono") == 0) {
			channels = 1;
		} else if (strcasecmp(schan, "stereo") == 0) {
			channels = 2;
		} else {
			channels = 0;
		}

		freq = string_to_freq(sfreq);
		assert(channels != 0);
		assert(freq     != 0);
		next_cid = codec_get_matching(short_name, (uint16_t)freq, (uint16_t)channels);

		if (next_cid && codec_get_payload(next_cid) != 255) {
			cid     = codec_get_by_payload ((u_char)sp->encodings[0]);
			if (codec_audio_formats_compatible(next_cid, cid)) {
				sp->encodings[0] = codec_get_payload(next_cid);
				ui_send_audio_codec(sp, sp->mbus_ui_addr);
			} else {
				/* just register we want to make a change */
				audio_device_register_change_primary(sp, next_cid);
			}
		}
}

static void
rx_tool_rat_codecs_request(char *srce, char *args, session_t *sp)
{
	UNUSED(args);
	ui_send_codec_list(sp, srce);
}

static void rx_tool_rat_playout_limit(char *srce, char *args, session_t *sp)
{
	int i;
	struct mbus_parser	*mp;

	UNUSED(srce);
	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i) && (1 == i || 0 == i)) {
		sp->limit_playout = i;
	} else {
		debug_msg("mbus: usage \"tool.rat.playout.limit <bool>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_playout_min(char *srce, char *args, session_t *sp)
{
	int	 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
		sp->min_playout = i;
	} else {
		debug_msg("mbus: usage \"tool.rat.playout.min <integer>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_playout_max(char *srce, char *args, session_t *sp)
{
	int	 i;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
		sp->max_playout = i;
	} else {
		debug_msg("mbus: usage \"tool.rat.playout.max <integer>\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_payload_set(char *srce, char *args, session_t *sp)
{
	codec_id_t cid, cid_replacing;
	char *codec_long_name;
	int   i, new_pt;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);

	if (mbus_parse_str(mp, &codec_long_name) &&
		mbus_parse_int(mp, &new_pt)) {
			mbus_decode_str(codec_long_name);

			if (payload_is_valid((u_char)new_pt) == FALSE ||
				new_pt < 0 || new_pt > 255) {
					debug_msg("Invalid payload specified\n");
					mbus_parse_done(mp);
					return;
				}

				/* Don't allow payloads to be mapped to channel_coder payloads - it doesn't seem to work */
				if (channel_coder_exist_payload((uint8_t)new_pt)) {
					debug_msg("Channel coder payload specified\n");
					mbus_parse_done(mp);
					return;
				}

				for(i = 0; i < sp->num_encodings; i++) {
					if (new_pt == sp->encodings[i]) {
						debug_msg("Doh! Attempting to remap encoding %d codec.\n", i);
						mbus_parse_done(mp);
						return;
					}
				}

				cid_replacing = codec_get_by_payload((u_char)new_pt);
				if (cid_replacing) {
					const codec_format_t *cf;
					cf = codec_get_format(cid_replacing);
					assert(cf);
					debug_msg("Codec map replacing %s\n", cf->long_name);
					codec_unmap_payload(cid_replacing, (u_char)new_pt);
					ui_send_codec_details(sp, sp->mbus_ui_addr, cid_replacing);
				}

				cid = codec_get_by_name(codec_long_name);
				if (cid && codec_map_payload(cid, (u_char)new_pt)) {
					ui_send_codec_details(sp, sp->mbus_ui_addr, cid);
					debug_msg("map %s %d succeeded.\n", codec_long_name, new_pt);
				} else {
					debug_msg("map %s %d failed.\n", codec_long_name, new_pt);
				}
		}
		mbus_parse_done(mp);
}

static void rx_tool_rat_converters_request(char *srce, char *args, session_t *sp)
{
	UNUSED(args);
	ui_send_converter_list(sp, srce);
}

static void rx_tool_rat_converter(char *srce, char *args, session_t *sp)
{
	const converter_details_t *d = NULL;
	uint32_t             i, n;
	char               *name;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &name)) {
		mbus_decode_str(name);
		n = converter_get_count();
		for(i = 0; i < n; i++) {
			d = converter_get_details(i);
			if (0 == strcasecmp(d->name,name)) {
				break;
			}
		}
		if (i == n) {
			d = converter_get_details(0);
		}
		sp->converter = d->id;
	} else {
		debug_msg("mbus: usage \"tool.rat.converter <name>\"\n");
	}
	mbus_parse_done(mp);
	ui_send_converter(sp, sp->mbus_ui_addr);
}

/* set_red_parameters translates what mbus_receives into command
* redundancy encoder understands.
*/

static void
set_red_parameters(session_t *sp, char *sec_enc, int offset)
{
	const codec_format_t *pcf, *rcf;
	codec_id_t            pri_id, red_id;
	char *cmd;
	int   clen;
	assert(offset>0);

	pri_id = codec_get_by_payload(sp->encodings[0]);
	pcf    = codec_get_format(pri_id);
	red_id = codec_get_matching(sec_enc, (uint32_t)pcf->format.sample_rate, (uint16_t)pcf->format.channels);
	if (!codec_id_is_valid(red_id)) {
		debug_msg("Failed to get redundant codec requested (%s)\n", sec_enc);
		red_id = pri_id;  /* Use same as primary */
	}
	rcf = codec_get_format(red_id);

	clen = 2 * (CODEC_LONG_NAME_LEN + 4);
	cmd  = (char*)xmalloc(clen);
	sprintf(cmd, "%s/%d/%s/%d", pcf->long_name, 0, rcf->long_name, offset);

	xmemchk();
	if (channel_encoder_set_parameters(sp->channel_coder, cmd) == 0) {
		debug_msg("Red command failed: %s\n", cmd);
	}
	xmemchk();
	xfree(cmd);
	/* Now tweak session parameters */
	sp->num_encodings = 2;
	sp->encodings[1]  = codec_get_payload(red_id);
}


static void
set_layered_parameters(session_t *sp, char *sec_enc, char *schan, char *sfreq, int layerenc)
{
	const codec_format_t *pcf, *lcf;
	codec_id_t            pri_id, lay_id;
	char *cmd;
	int      freq, channels;
	int   clen;
	assert(layerenc>0);

	if (strcasecmp(schan, "mono") == 0) {
		channels = 1;
	} else if (strcasecmp(schan, "stereo") == 0) {
		channels = 2;
	} else {
		channels = 0;
	}

	freq = string_to_freq(sfreq);
	pri_id = codec_get_by_payload(sp->encodings[0]);
	pcf    = codec_get_format(pri_id);
	lay_id = codec_get_matching(sec_enc, (uint16_t)freq, (uint16_t)channels);
	if(lay_id == 0) {
		debug_msg("Can't find layered codec (%s) - need to change primary codec\n", sec_enc);
	}
	if (pri_id!=lay_id) {
		/* This can happen if you change codec and select layering    *
		* before pushing apply, so change the primary encoding here. */
		codec_id_t cid;
		if (lay_id && codec_get_payload(lay_id) != 255) {
			cid     = codec_get_by_payload ((u_char)sp->encodings[0]);
			if (codec_audio_formats_compatible(lay_id, cid)) {
				sp->encodings[0] = codec_get_payload(lay_id);
				ui_send_audio_codec(sp, sp->mbus_ui_addr);
			} else {
				/* just register we want to make a change */
				audio_device_register_change_primary(sp, lay_id);
			}
		}
	}
	lcf = codec_get_format(lay_id);

	if(layerenc<=MAX_LAYERS) {
		if(layerenc > sp->rtp_session_count) {
			debug_msg("%d is too many layers - ports not inited - forcing %d layers\n", layerenc, sp->rtp_session_count);
			layerenc = sp->rtp_session_count;
		}
	}

	clen = CODEC_LONG_NAME_LEN + 4;
	cmd  = (char*)xmalloc(clen);
	sprintf(cmd, "%s/%d", lcf->long_name, layerenc);

	xmemchk();
	if (channel_encoder_set_parameters(sp->channel_coder, cmd) == 0) {
		debug_msg("Layered command failed: %s\n", cmd);
	}
	xmemchk();
	xfree(cmd);
	/* Now tweak session parameters */
	sp->layers = layerenc;
	sp->num_encodings = 1;
}

/* This function is a bit nasty because it has to coerce what the
* mbus gives us into something the channel coders understand.  In addition,
* we assume we know what channel coders are which kind of defies point
* 'generic' structure but that's probably because it's not generic enough.
*/
static void rx_audio_channel_coding(char *srce, char *args, session_t *sp)
{
	const cc_details_t *ccd;
	char        *coding, *sec_enc, *schan, *sfreq;
	int          offset, layerenc;
	uint32_t      i, n;
	uint16_t      upp;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &coding)) {
		mbus_decode_str(coding);
		upp = channel_encoder_get_units_per_packet(sp->channel_coder);
		n = channel_get_coder_count();
		for(i = 0; i < n; i++) {
			ccd = channel_get_coder_details(i);
			if (strncasecmp(ccd->name, coding, 3) == 0) {
				debug_msg("rx_audio_channel_coding: 0x%08x, %s\n", ccd->descriptor, &ccd->name);
				switch(tolower((int)ccd->name[0])) { //SV-XXX: NetBSD
								case 'n':   /* No channel coding */
									sp->num_encodings = 1;
									sp->layers = 1;
									channel_encoder_destroy(&sp->channel_coder);
									channel_encoder_create(ccd->descriptor, &sp->channel_coder);
									channel_encoder_set_units_per_packet(sp->channel_coder, upp);
									break;
								case 'r':   /* Redundancy -> extra parameters */
									if (mbus_parse_str(mp, &sec_enc) &&
										mbus_parse_int(mp, &offset)) {
											mbus_decode_str(sec_enc);
											sp->layers = 1;
											channel_encoder_destroy(&sp->channel_coder);
											channel_encoder_create(ccd->descriptor, &sp->channel_coder);
											channel_encoder_set_units_per_packet(sp->channel_coder, upp);
											set_red_parameters(sp, sec_enc, offset);
										}
										break;
								case 'l':       /*Layering */
									if (mbus_parse_str(mp, &sec_enc) &&
										mbus_parse_str(mp, &schan) &&
										mbus_parse_str(mp, &sfreq) &&
										mbus_parse_int(mp, &layerenc)) {
											mbus_decode_str(sec_enc);
											mbus_decode_str(schan);
											mbus_decode_str(sfreq);
											channel_encoder_destroy(&sp->channel_coder);
											channel_encoder_create(ccd->descriptor, &sp->channel_coder);
											channel_encoder_set_units_per_packet(sp->channel_coder, upp);
											set_layered_parameters(sp, sec_enc, schan, sfreq, layerenc);
										}
										break;
				}
				break;
			}
		}
	}
	mbus_parse_done(mp);
#ifdef DEBUG
	ccd = channel_get_coder_identity(sp->channel_coder);
	debug_msg("***** %s\n", ccd->name);
#endif /* DEBUG */
	ui_send_audio_channel_coding(sp, sp->mbus_ui_addr);
}

static void rx_tool_rat_settings(char *srce, char *args, session_t *sp)
{
	/* When we get a tool.rat.settings() message, we dump out state  */
	/* into the query source. We omit rtp and audio related state,   */
	/* since there are other mbus commands which solicit that info.  */
	/* It should be possible to query most of this information via   */
	/* specific commands for each variable, but I haven't got around */
	/* to implementing all that yet - please send in a patch! [csp]  */
	UNUSED(args);
	ui_send_repair_scheme_list(sp, srce);
	ui_send_codec_list        (sp, srce);
	ui_send_converter_list    (sp, srce);
	ui_send_converter         (sp, sp->mbus_ui_addr);
	ui_send_lecture_mode      (sp, sp->mbus_ui_addr);
	ui_send_sampling_mode_list(sp, srce);
	ui_send_powermeter        (sp, srce);
	ui_send_playout_bounds    (sp, srce);
	ui_send_agc               (sp, srce);
	ui_send_loopback_gain     (sp, srce);
	ui_send_echo_suppression  (sp, srce);
	ui_send_encryption_key    (sp, srce);
	ui_send_device_config     (sp, srce);
	ui_send_audio_codec       (sp, srce);
	ui_send_rate              (sp, srce);
}

static void rx_mbus_quit(char *srce, char *args, session_t *sp)
{
	/* mbus.quit() means that we should quit */
	UNUSED(args);
	UNUSED(srce);
	UNUSED(sp);
	should_exit = TRUE;
	debug_msg("Media engine got mbus.quit()\n");
}

static void rx_mbus_bye(char *srce, char *args, session_t *sp)
{
	/* mbus.bye() means that the sender of the message is about to quit */
	UNUSED(args);
	if (strstr(srce, "media:video") != NULL) {
		sp->sync_on = FALSE;
	}
}

static void rx_mbus_waiting(char *srce, char *args, session_t *sp)
{
	char	 		*s, *sd;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &s)) {
		sd = mbus_decode_str(s);
		if (strcmp(sd, sp->mbus_waiting_token) == 0) {
			sp->mbus_waiting = FALSE;
		} else {
			debug_msg("Got mbus.waiting(%s)\n", sd);
		}
	} else {
		debug_msg("mbus: usage \"mbus.waiting(token)\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_mbus_go(char *srce, char *args, session_t *sp)
{
	char	 		*s, *sd;
	struct mbus_parser	*mp;

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &s)) {
		sd = mbus_decode_str(s);
		if (strcmp(sd, "rat-ui-requested") == 0) {
			/* We now have a user interface... :-) */
			sp->ui_on = TRUE;
			sp->mbus_ui_addr = xstrdup(srce);
		} else if (strcmp(sd, sp->mbus_go_token) == 0) {
			sp->mbus_go = FALSE;
			debug_msg("Got required mbus.go(%s)\n", sd);
		} else {
			debug_msg("Got spurious mbus.go(%s)\n", sd);
		}
	} else {
		debug_msg("mbus: usage \"mbus.go(token)\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_tool_rat_ui_detach_request(char *srce, char *args, session_t *sp)
{
	UNUSED(args);
	if (sp->ui_on == FALSE) {
		debug_msg("UI not enabled, cannot detach\n");
		return;
	}
	if (strcmp(sp->mbus_ui_addr, srce) != 0) {
		debug_msg("Cannot detach UI, addresses differ\n");
		return;
	}
	mbus_qmsgf(sp->mbus_engine, srce, TRUE, "tool.rat.ui.detach", "");
	sp->ui_on = FALSE;
}

static void rx_tool_rat_layers(char *srce, char *args, session_t *sp)
{
	int      i;
	struct mbus_parser      *mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_int(mp, &i)) {
		if(i>MAX_LAYERS) {
			debug_msg("too many layers: max = %d\n", MAX_LAYERS);
			sp->layers = MAX_LAYERS;
		} else {
			sp->layers = i;
		}
	} else {
		debug_msg("mbus: usage \"tool.rat.layers <integer>\"\n");
	}
	mbus_parse_done(mp);
}


static void rx_mbus_hello(char *srce, char *args, session_t *sp)
{
	UNUSED(args);
	if (strstr(srce, "media:video") != NULL) {
		sp->sync_on = TRUE;
	}
}

static const mbus_cmd_tuple engine_cmds[] = {
	{ "tool.rat.logstats",                     rx_tool_rat_logstats },
	{ "tool.rat.logdebug",                     rx_tool_rat_logdebug },
	{ "tool.rat.tone.start",                   rx_tool_rat_tone_start },
	{ "tool.rat.tone.stop",                    rx_tool_rat_tone_stop },
	{ "tool.rat.voxlet.play",                  rx_tool_rat_voxlet_play },
	{ "session.title",                         rx_session_title },
	{ "tool.rat.silence",                      rx_tool_rat_silence },
	{ "tool.rat.silence.threshold",            rx_tool_rat_silence_thresh },
	{ "tool.rat.lecture.mode",                 rx_tool_rat_lecture_mode },
	{ "audio.3d.enabled",                      rx_audio_3d_enable },
	{ "audio.3d.user.settings",                rx_audio_3d_user_settings },
	{ "audio.3d.user.settings.request",        rx_audio_3d_user_settings_req },
	{ "tool.rat.agc",                          rx_tool_rat_agc },
	{ "tool.rat.loopback.gain",                rx_tool_rat_loopback_gain },
	{ "tool.rat.echo.suppress",                rx_tool_rat_echo_suppress },
	{ "tool.rat.rate",                         rx_tool_rat_rate },
	{ "tool.rat.powermeter",                   rx_tool_rat_powermeter },
	{ "tool.rat.converters.request",           rx_tool_rat_converters_request },
	{ "tool.rat.converter",                    rx_tool_rat_converter },
	{ "tool.rat.settings",                     rx_tool_rat_settings },
	{ "tool.rat.codec",                        rx_tool_rat_codec },
	{ "tool.rat.codecs.request",               rx_tool_rat_codecs_request },
	{ "tool.rat.playout.limit",                rx_tool_rat_playout_limit },
	{ "tool.rat.playout.min",                  rx_tool_rat_playout_min },
	{ "tool.rat.playout.max",                  rx_tool_rat_playout_max },
	{ "tool.rat.payload.set",                  rx_tool_rat_payload_set },
	{ "tool.rat.ui.detach.request",            rx_tool_rat_ui_detach_request },
	{ "tool.rat.filter.loopback",              rx_tool_rat_filter_loopback },
	{ "tool.rat.layers",                       rx_tool_rat_layers },
	{ "audio.input.mute",                      rx_audio_input_mute },
	{ "audio.input.gain",                      rx_audio_input_gain },
	{ "audio.input.port",                      rx_audio_input_port },
	{ "audio.output.mute",                     rx_audio_output_mute },
	{ "audio.output.gain",                     rx_audio_output_gain },
	{ "audio.output.port",                     rx_audio_output_port },
	{ "audio.channel.coding",                  rx_audio_channel_coding },
	{ "audio.channel.repair",                  rx_audio_channel_repair },
	{ "audio.file.play.open",                  rx_audio_file_play_open },
	{ "audio.file.play.pause",                 rx_audio_file_play_pause },
	{ "audio.file.play.stop",                  rx_audio_file_play_stop },
	{ "audio.file.play.live",                  rx_audio_file_play_live },
	{ "audio.file.record.open",                rx_audio_file_rec_open },
	{ "audio.file.record.pause",               rx_audio_file_rec_pause },
	{ "audio.file.record.stop",                rx_audio_file_rec_stop },
	{ "audio.file.record.live",                rx_audio_file_rec_live },
	{ "audio.device",                          rx_audio_device },
	{ "audio.query",                           rx_audio_query },
	{ "security.encryption.key",               rx_security_encryption_key },
	{ "rtp.query",                             rx_rtp_query },
	{ "rtp.addr.query",                        rx_rtp_addr_query },
	{ "rtp.addr",                              rx_rtp_addr },
	{ "rtp.source.name",                       rx_rtp_source_name },
	{ "rtp.source.email",                      rx_rtp_source_email },
	{ "rtp.source.phone",                      rx_rtp_source_phone },
	{ "rtp.source.loc",                        rx_rtp_source_loc },
	{ "rtp.source.note",                       rx_rtp_source_note },
	{ "rtp.source.mute",                       rx_rtp_source_mute },
	{ "rtp.source.gain",                       rx_rtp_source_gain },
        { "rtp.source.app.site",                   rx_rtp_source_app_site },
	{ "mbus.quit",                             rx_mbus_quit },
	{ "mbus.bye",                              rx_mbus_bye },
	{ "mbus.waiting",                          rx_mbus_waiting },
	{ "mbus.go",                               rx_mbus_go },
	{ "mbus.hello",                            rx_mbus_hello },
};

#define NUM_ENGINE_CMDS sizeof(engine_cmds)/sizeof(engine_cmds[0])

void mbus_engine_rx(char *srce, char *cmnd, char *args, void *data)
{
	uint32_t i;

	for (i = 0; i < NUM_ENGINE_CMDS; i++) {
		if (strcmp(engine_cmds[i].rxname, cmnd) == 0) {
			engine_cmds[i].rxproc(srce, args, (session_t *) data);
			return;
		}
	}
	debug_msg("Unknown mbus command: %s (%s)\n", cmnd, args);
#ifndef NDEBUG
	fprintf(stderr, "Unknown mbus command: %s (%s)\n", cmnd, args);
	abort();
#endif
}

