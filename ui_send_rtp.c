/*
 * FILE:    ui_send_rtp.c
 * PROGRAM: RAT
 * AUTHOR:  Colin Perkins
 *
 * Routines which send RTP related mbus commands to the user interface.
 *
 * Copyright (c) 2000-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: ui_send_rtp.c 4084 2007-07-27 16:16:37Z piers $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "mbus.h"
#include "mbus_parser.h"
#include "audio_types.h"
#include "rtp.h"
#include "pdb.h"
#include "session.h"
#include "ui_send_rtp.h"

static void ui_info_update_sdes(session_t *sp, char *addr, char *item, const char *val, uint32_t ssrc)
{
	char *arg;

	if (!sp->ui_on) return;
        if (val == NULL) {
                val = "";
        }
        arg = mbus_encode_str(val);
	mbus_qmsgf(sp->mbus_engine, addr, FALSE, item, "\"%08lx\" %s", (unsigned long)ssrc, arg);
	xfree(arg);
}

void ui_send_rtp_cname(session_t *sp, char *addr, uint32_t ssrc)
{
	ui_info_update_sdes(sp, addr, "rtp.source.cname", rtp_get_sdes(sp->rtp_session[0], ssrc, RTCP_SDES_CNAME), (unsigned long)ssrc);
}

void ui_send_rtp_name(session_t *sp, char *addr, uint32_t ssrc)
{
	const char 	*sdes;
	char		 ssrc_c[9];

	sdes = rtp_get_sdes(sp->rtp_session[0], ssrc, RTCP_SDES_NAME);
	if (sdes == NULL) {
		sdes = rtp_get_sdes(sp->rtp_session[0], ssrc, RTCP_SDES_CNAME);
	}
	if (sdes == NULL) {
		sprintf(ssrc_c, "%08lx", (unsigned long) ssrc);
		sdes = ssrc_c;
	}
	ui_info_update_sdes(sp, addr, "rtp.source.name", sdes, ssrc);
}

void ui_send_rtp_email(session_t *sp, char *addr, uint32_t ssrc)
{
	ui_info_update_sdes(sp, addr, "rtp.source.email", rtp_get_sdes(sp->rtp_session[0], ssrc, RTCP_SDES_EMAIL), ssrc);
}

void ui_send_rtp_phone(session_t *sp, char *addr, uint32_t ssrc)
{
	ui_info_update_sdes(sp, addr, "rtp.source.phone", rtp_get_sdes(sp->rtp_session[0], ssrc, RTCP_SDES_PHONE), ssrc);
}

void ui_send_rtp_loc(session_t *sp, char *addr, uint32_t ssrc)
{
	ui_info_update_sdes(sp, addr, "rtp.source.loc", rtp_get_sdes(sp->rtp_session[0], ssrc, RTCP_SDES_LOC), ssrc);
}

void ui_send_rtp_tool(session_t *sp, char *addr, uint32_t ssrc)
{
	ui_info_update_sdes(sp, addr, "rtp.source.tool", rtp_get_sdes(sp->rtp_session[0], ssrc, RTCP_SDES_TOOL), ssrc);
}

void ui_send_rtp_note(session_t *sp, char *addr, uint32_t ssrc)
{
	ui_info_update_sdes(sp, addr, "rtp.source.note", rtp_get_sdes(sp->rtp_session[0], ssrc, RTCP_SDES_NOTE), ssrc);
}

void ui_send_rtp_priv(session_t *sp, char *addr, uint32_t ssrc)
{
	char	priv[255];
	char	l;
	int	i;

	strncpy(priv, rtp_get_sdes(sp->rtp_session[0], ssrc, RTCP_SDES_PRIV), 255);
	l = priv[0];
	for (i=1; i<(int)l+1; i++) {
		priv[i-1] = priv[i];
	}
	priv[(int)l] = '/';

	ui_info_update_sdes(sp, addr, "rtp.source.priv", priv, ssrc);
}

void
ui_send_rtp_gain(session_t *sp, char *addr, uint32_t ssrc)
{
        pdb_entry_t *pdbe;
	if (!sp->ui_on) return;
        if (pdb_item_get(sp->pdb, ssrc, &pdbe)) {
                mbus_qmsgf(sp->mbus_engine, addr, TRUE, "rtp.source.gain", "\"%08lx\" %.2f", (unsigned long)pdbe->ssrc, pdbe->gain);
        }
}

void
ui_send_rtp_mute(session_t *sp, char *addr, uint32_t ssrc)
{
        pdb_entry_t *pdbe;
	if (!sp->ui_on) return;
        if (pdb_item_get(sp->pdb, ssrc, &pdbe)) {
                mbus_qmsgf(sp->mbus_engine, addr, TRUE, "rtp.source.mute", "\"%08lx\" %d", (unsigned long)pdbe->ssrc, pdbe->mute);
        }
}

void
ui_send_rtp_remove(session_t *sp, char *addr, uint32_t ssrc)
{
	if (!sp->ui_on) return;
        mbus_qmsgf(sp->mbus_engine, addr, TRUE, "rtp.source.remove", "\"%08lx\"", (unsigned long)ssrc);
}

void
ui_send_rtp_active(session_t *sp, char *addr, uint32_t ssrc)
{
	if (!sp->ui_on) return;
        mbus_qmsgf(sp->mbus_engine, addr, TRUE, "rtp.source.active", "\"%08lx\"", (unsigned long)ssrc);
}

void
ui_send_rtp_inactive(session_t *sp, char *addr, uint32_t ssrc)
{
	session_validate(sp);
	if (!sp->ui_on) return;
        mbus_qmsgf(sp->mbus_engine, addr, TRUE, "rtp.source.inactive", "\"%08lx\"", (unsigned long)ssrc);
}

void
ui_send_rtp_packet_loss(session_t *sp, char *addr, uint32_t srce, uint32_t dest, int loss)
{
	if (!sp->ui_on) return;
	mbus_qmsgf(sp->mbus_engine, addr, FALSE, "rtp.source.packet.loss", "\"%08lx\" \"%08lx\" %3d", (unsigned long)srce, (unsigned long)dest, loss);
}

void
ui_send_rtp_rtt(session_t *sp, char *addr, uint32_t ssrc, double rtt_sec)
{
	if (!sp->ui_on) return;
        mbus_qmsgf(sp->mbus_engine, addr, FALSE, "rtp.source.rtt", "\"%08lx\" %6ld", (unsigned long)ssrc, (uint32_t) (1000 * rtt_sec));
}

void
ui_send_rtp_ssrc(session_t *sp, char *addr)
{
	if (!sp->ui_on) return;
	mbus_qmsgf(sp->mbus_engine, addr, TRUE, "rtp.ssrc", "\"%08lx\"", (unsigned long)rtp_my_ssrc(sp->rtp_session[0]));
}

void
ui_send_rtp_addr(session_t *sp, char *addr)
{
	char *rtp_addr = mbus_encode_str(rtp_get_addr(sp->rtp_session[0]));

	if (!sp->ui_on) return;
        mbus_qmsgf(sp->mbus_engine, addr, TRUE, "rtp.addr", "%s %5d %5d %3d",
		   rtp_addr,
		   rtp_get_rx_port(sp->rtp_session[0]),
		   rtp_get_tx_port(sp->rtp_session[0]),
		   rtp_get_ttl(sp->rtp_session[0]));
        xfree(rtp_addr);
}

void
ui_send_rtp_title(session_t *sp, char *addr)
{
	char	*title;

	if (!sp->ui_on) return;
        title = mbus_encode_str(sp->title);
        mbus_qmsg(sp->mbus_engine, addr, "session.title", title, TRUE);
        xfree(title);
}

void ui_send_rtp_app_site(session_t *sp, char *addr, uint32_t ssrc, char *siteid)
{
        char *enc_siteid;
	if (!sp->ui_on) return;
	enc_siteid = mbus_encode_str(siteid);
	mbus_qmsgf(sp->mbus_engine, addr, TRUE, "rtp.source.app.site", "\"%08lx\" %s", (unsigned long)ssrc, enc_siteid);
	xfree(enc_siteid);
}
