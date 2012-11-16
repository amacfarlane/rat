/*
 * FILE:    ui_send_stats.c
 * PROGRAM: RAT
 * AUTHOR:  Colin Perkins
 *
 * Routines which send stats updates to the user interface.
 *
 * Copyright (c) 2000-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: ui_send_stats.c 4084 2007-07-27 16:16:37Z piers $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "session.h"
#include "pdb.h"
#include "source.h"
#include "mix.h"
#include "transmit.h"
#include "mbus.h"
#include "mbus_parser.h"
#include "ui_send_rtp.h"
#include "ui_send_stats.h"
#include "ui_send_audio.h"

#include "parameters.h"

void
ui_send_stats(session_t *sp, char *addr, uint32_t ssrc)
{
	/* Send RAT specific statistics to the user interface... */
        const rtcp_rr           *rr;
        uint32_t                  fract_lost, my_ssrc, total_lost;
        double                   skew_rate;
	char			*args, *mbes;
        struct s_source      	*src;
        uint32_t               	 buffered, delay;
        pdb_entry_t             *pdbe;

	session_validate(sp);
	if (!sp->ui_on) return;
        if (pdb_item_get(sp->pdb, ssrc, &pdbe) == FALSE) {
                debug_msg("pdb entry does not exist (0x%08x)\n", ssrc);
                return;
        }
        pdbe->last_ui_update = sp->cur_ts;

        if (pdbe->enc_fmt) {
		mbes = mbus_encode_str(pdbe->enc_fmt);
                args = (char *) xmalloc(strlen(mbes) + 12);
                sprintf(args, "\"%08x\" %s", pdbe->ssrc, mbes);
                xfree(mbes);
        } else {
                args = (char *) xmalloc(19);
                sprintf(args, "\"%08x\" unknown", pdbe->ssrc);
        }

        mbus_qmsg(sp->mbus_engine, addr, "rtp.source.codec", args, FALSE);
        xfree(args);

        src = source_get_by_ssrc(sp->active_sources, pdbe->ssrc);
        if (src) {
                buffered = timestamp_to_ms(source_get_audio_buffered(src));
                delay    = timestamp_to_ms(source_get_playout_delay(src));
                skew_rate = source_get_skew_rate(src);
        } else {
                buffered  = 0;
                delay     = 0;
                skew_rate = 1.0;
        }

        mbus_qmsgf(sp->mbus_engine, addr, FALSE, "tool.rat.audio.buffered", "\"%08lx\" %ld", (unsigned long)pdbe->ssrc, buffered);
        mbus_qmsgf(sp->mbus_engine, addr, FALSE, "tool.rat.audio.delay", "\"%08lx\" %ld", (unsigned long)pdbe->ssrc, delay);
        mbus_qmsgf(sp->mbus_engine, addr, FALSE, "tool.rat.audio.skew", "\"%08lx\" %.5f", (unsigned long)pdbe->ssrc, skew_rate);
        mbus_qmsgf(sp->mbus_engine, addr, FALSE, "tool.rat.spike.events", "\"%08lx\" %ld", (unsigned long)pdbe->ssrc, pdbe->spike_events);
        mbus_qmsgf(sp->mbus_engine, addr, FALSE, "tool.rat.spike.toged", "\"%08lx\" %ld",  (unsigned long)pdbe->ssrc, pdbe->spike_toged);
        my_ssrc = rtp_my_ssrc(sp->rtp_session[0]);
        rr = rtp_get_rr(sp->rtp_session[0], my_ssrc, pdbe->ssrc);
        if (rr != NULL) {
                fract_lost = (rr->fract_lost * 100) >> 8;
                total_lost = rr->total_lost;
        } else {
                debug_msg("No rr\n");
                fract_lost = 0;
                total_lost = 0;
        }

        ui_send_rtp_packet_loss(sp, addr, my_ssrc, pdbe->ssrc, fract_lost);
	mbus_qmsgf(sp->mbus_engine, addr, FALSE, "rtp.source.reception", "\"%08lx\" %6ld %6ld %6ld %6ld %6ld %6d",
		  (unsigned long)pdbe->ssrc, pdbe->received, total_lost, pdbe->misordered, pdbe->duplicates, timestamp_to_ms(pdbe->jitter), pdbe->jit_toged);
	mbus_qmsgf(sp->mbus_engine, addr, FALSE, "rtp.source.packet.duration", "\"%08lx\" %3d",
	           (unsigned long)pdbe->ssrc, pdbe->inter_pkt_gap * 1000 / pdbe->sample_rate);
}

void
ui_send_periodic_updates(session_t *sp, char *addr, int elapsed_time)
{
        static uint32_t power_time = 0;
        static uint32_t bps_time   = 0;

	if (!sp->ui_on) return;
	session_validate(sp);

        bps_time   += elapsed_time;
        if (bps_time > 10 * sp->meter_period) {
		double inbps = 0.0, outbps = 0.0;
		uint32_t scnt, sidx;
		struct s_source *s;

		if (!sp->ui_on) return;
		scnt = source_list_source_count(sp->active_sources);
		for(sidx = 0; sidx < scnt; sidx++) {
			s = source_list_get_source_no(sp->active_sources, sidx);
			inbps += source_get_bps(s);
		}
		mbus_qmsgf(sp->mbus_engine, addr, FALSE, "tool.rat.bps.in", "%.0f", inbps);

		outbps = tx_get_bps(sp->tb);
		mbus_qmsgf(sp->mbus_engine, addr, FALSE, "tool.rat.bps.out", "%.0f", outbps);
                bps_time = 0;
        }

        power_time += elapsed_time;
        if (power_time > sp->meter_period) {
		if (sp->meter && (sp->ms != NULL)) {
                        uint16_t me = 0;
                        if (sp->playing_audio) {
                                me = mix_get_energy(sp->ms, 160);
                        }
                        ui_send_audio_output_powermeter(sp, sp->mbus_ui_addr, lin2vu(me, 100, VU_OUTPUT));
		}
		if (tx_is_sending(sp->tb)) {
			tx_update_ui(sp->tb);
		}
                power_time = 0;
        }
}

