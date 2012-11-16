/*
 * FILE:    session.c
 * PROGRAM: RAT
 * AUTHORS: Vicky Hardman + Isidor Kouvelas + Colin Perkins + Orion Hodson
 *
 * Copyright (c) 1995-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: session.c 3902 2006-10-19 15:23:11Z csp $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "memory.h"
#include "version.h"
#include "session.h"
#include "repair.h"
#include "codec_types.h"
#include "codec.h"
#include "channel_types.h"
#include "channel.h"
#include "converter.h"
#include "parameters.h"
#include "audio.h"
#include "pdb.h"
#include "rtp.h"
#include "source.h"
#include "channel_types.h"
#include "channel.h"
#include "sndfile.h"
#include "tonegen.h"
#include "voxlet.h"

#define PCKT_QUEUE_RTP_LEN  24
#define PCKT_QUEUE_RTCP_LEN 12

/* sanity_check_payloads checks for overlapping payload maps between
 * channel coders and codecs.  Necessary because I don't trust myself
 * to not overlap payloads, and other people should not have to worry
 * about it either. [oth]
 */

static int
sanity_check_payloads(void)
{
        uint32_t i, j, n_codecs, n_channels;
        codec_id_t cid;
        const codec_format_t *cf  = NULL;
        const cc_details_t   *ccd = NULL;
        cc_id_t    ccid;

        u_char pt;

        n_codecs = codec_get_number_of_codecs();
        n_channels = channel_get_coder_count();
        for(i = 0; i < n_codecs; i++) {
                cid = codec_get_codec_number(i);
                cf  = codec_get_format(cid);
                pt  = codec_get_payload(cid);
                if (pt != CODEC_PAYLOAD_DYNAMIC) {
                        ccid = channel_coder_get_by_payload(pt);
                        for(j = 0; j < n_channels; j++) {
                                ccd = channel_get_coder_details(j);
                                if (ccd == channel_get_null_coder()) {
                                        continue;
                                }
                                if (ccd->descriptor == ccid) {
                                        debug_msg("clash with %s %s payload (%d)\n", cf->long_name, ccd->name, pt);
                                        return FALSE;
                                }
                        }
                } else {
                        /* codec is not mapped into codec space so ignore */
                }
        }
        return TRUE;
}

void
session_init(session_t *sp, int index, int mode)
{
	codec_id_t                 cid;
        const codec_format_t      *cf   = NULL;
        const converter_details_t *conv = NULL;
        const cc_details_t        *ccd  = NULL;
        uint8_t                    i;

	memset(sp, 0, sizeof(session_t));

	codec_init();
        sanity_check_payloads();
        vu_table_init();

	cid = codec_get_by_name("DVI-8K-Mono");
        assert(cid);
        cf  = codec_get_format(cid);
	sp->cur_ts                      = ts_map32(8000,0);
        sp->encodings[0]		= codec_get_payload(cid);           	/* user chosen encoding for primary */
	sp->num_encodings		= 1;                                	/* Number of encodings applied */

        ccd = channel_get_null_coder();
        channel_encoder_create(ccd->descriptor, &sp->channel_coder);

        conv                            = converter_get_details(0);
        sp->converter                   = conv->id;
	sp->other_session		= NULL;				/* Completed in main_engine.c if we're a transoder */
	sp->id				= index;
	sp->mode         		= mode;
        sp->rtp_session_count           = 0;
	for (i = 0; i < MAX_LAYERS; i++) {
		sp->rx_rtp_port[i] = sp->tx_rtp_port[i] = sp->rx_rtcp_port[i] = sp->tx_rtcp_port[i] = PORT_UNINIT;
                sp->rtp_session[i] = NULL;
	}
	sp->rx_rtp_port[0] 		= 5004; /* Default ports per:             */
	sp->tx_rtp_port[0] 		= 5004; /* draft-ietf-avt-profile-new-00  */
        sp->rx_rtcp_port[0]   		= 5005;
        sp->tx_rtcp_port[0]   		= 5005;
	sp->ttl				= 127;
        sp->filter_loopback             = TRUE;
	sp->playing_audio		= TRUE;
	sp->lecture			= FALSE;
	sp->auto_lecture		= 0;
 	sp->receive_audit_required	= FALSE;
	sp->silence_detection		= SILENCE_DETECTION_OFF;
	sp->sync_on			= FALSE;
	sp->agc_on			= FALSE;
        sp->ui_on                       = FALSE;
	sp->meter			= TRUE;					/* Powermeter operation */
	sp->in_file 			= NULL;
	sp->out_file  			= NULL;
	sp->local_file_player		= NULL;
	sp->mbus_engine_addr		= NULL;
	sp->mbus_engine			= NULL;
	sp->mbus_ui_addr		= NULL;
	sp->mbus_video_addr		= xstrdup("(media:video module:engine)");
	sp->min_playout			= 0;
	sp->max_playout			= 1000;
        sp->last_depart_ts              = 0;
	sp->loopback_gain		= 0;
	sp->layers                      = 1;
	sp->ui_activated		= FALSE;
	sp->encrkey			= NULL;
	sp->logger                      = NULL;
	sp->mbus_waiting		= FALSE;
	sp->mbus_waiting_token		= NULL;
	sp->mbus_go 			= FALSE;
	sp->mbus_go_token		= NULL;
	sp->magic			= 0xcafebabe;				/* Magic number for debugging */

        source_list_create(&sp->active_sources);

	sp->title = "Untitled session";
	strncpy(sp->asc_address[0], "127.0.0.3", MAXHOSTNAMELEN);	/* Yeuch! This value should never be used! */
}

void
session_exit(session_t *sp)
{
        codec_exit();
        if (sp->local_file_player) {
                voxlet_destroy(&sp->local_file_player);
        }
	if (sp->tone_generator) {
		tonegen_destroy(&sp->tone_generator);
	}
        if (sp->in_file_converter) {
                converter_destroy(&sp->in_file_converter);
        }
	if (sp->in_file  != NULL) {
                snd_read_close (&sp->in_file);
        }
	if (sp->out_file != NULL) {
                snd_write_close(&sp->out_file);
        }
	if (sp->pdb != NULL) {
		pdb_destroy(&sp->pdb);
	}
        channel_encoder_destroy(&sp->channel_coder);
        source_list_destroy(&sp->active_sources);
	xfree(sp->mbus_engine_addr);
	xfree(sp->mbus_video_addr);
	xfree(sp->mbus_ui_addr);
	xfree(sp);
}

void
session_validate(session_t *sp)
{
	/* Sanity check the session... the more checks we can add here the better, */
	/* they're done once round the main loop and we can add calls to this at   */
	/* any point we think our data structures are being corrupted. For speed,  */
	/* we only check the magic number if we've not been built with debugging.  */
	assert(sp != NULL);
	assert(sp->magic == 0xcafebabe);
#ifdef DEBUG
	assert((sp->ttl >= 0) && (sp->ttl <= 255));
	assert((sp->tx_rtp_port[0] % 2) == 0);
	assert((sp->rx_rtp_port[0] % 2) == 0);
	assert((sp->tx_rtcp_port[0] % 2) == 1);
	assert((sp->rx_rtcp_port[0] % 2) == 1);
#endif
}

