/*
 * FILE:      source.c
 * AUTHOR(S): Orion Hodson
 *
 * Layering support added by Tristan Henderson.
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: source.c 4548 2010-01-07 12:44:36Z piers $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "audio_types.h"
#include "codec_types.h"
#include "ts.h"
#include "playout.h"
#include "channel.h"
#include "channel_types.h"
#include "codec.h"
#include "codec_state.h"
#include "converter.h"
#include "audio_util.h"
#include "render_3D.h"
#include "repair.h"
#include "ts.h"
#include "channel_types.h"
#include "pdb.h"
#include "pktbuf.h"
#include "source.h"
#include "debug.h"
#include "util.h"
#include "net_udp.h"
#include "mix.h"
#include "rtp.h"
#include "playout_calc.h"
#include "session.h"
#include "ui_send_stats.h"
#include "auddev.h"
#include "mbus.h"

#define SKEW_ADAPT_THRESHOLD       5000
#define SOURCE_YOUNG_AGE             20
#define NO_TOGED_CONT_FOR_PLAYOUT_RECALC 3

#define SOURCE_COMPARE_WINDOW_SIZE 8
#define SOURCE_MERGE_LEN_SAMPLES SOURCE_COMPARE_WINDOW_SIZE

/* Match threshold is mean abs diff. lower score gives less noise, but less  */
/* adaption..., might be better if threshold adapted with how much extra     */
/* data we have buffered...                                                  */
#define MATCH_THRESHOLD 1200

/* constants for skew adjustment:
 SOURCE_SKEW_SLOW - denotes source clock appears slower than ours.
 SOURCE_SKEW_FAST - denotes source clock appears faster than ours.
*/
typedef enum { SOURCE_SKEW_SLOW, SOURCE_SKEW_FAST, SOURCE_SKEW_NONE } skew_t;

typedef enum { PLAYOUT_MODE_NORMAL, PLAYOUT_MODE_SPIKE } pmode_t;

typedef struct s_source {
        struct s_source            *next;
        struct s_source            *prev;
        pdb_entry_t                *pdbe;       /* persistent database entry */
        uint32_t                    age;
        timestamp_t                 next_played; /* anticipated next unit    */
        timestamp_t                 talkstart;  /* start of latest talkspurt */
        timestamp_t                 last_repair;
	int			    hold_repair;
        uint32_t                    post_talkstart_units;
        uint16_t                    consec_lost;
        uint32_t                    mean_energy;
        struct s_pktbuf            *pktbuf;
        uint32_t                    packets_done;
        struct s_channel_state     *channel_state;
        struct s_codec_state_store *codec_states;
        struct s_pb                *channel;
        struct s_pb                *media;
        struct s_pb_iterator       *media_pos;
        struct s_converter         *converter;
        pmode_t                     playout_mode; /* SPIKE, NORMAL */
        timestamp_t                 spike_var;
        /* Fine grained playout buffer adjustment variables.  Used in        */
        /* attempts to correct for clock skew between source and local host. */
        skew_t 			    skew;
        timestamp_t   		    skew_adjust;
        int16_t                     skew_cnt;
        /* Skew stats                                                        */
        int32_t                     samples_played;
        int32_t                     samples_added;
        /* b/w estimation variables                                          */
        uint32_t                    byte_count;
        timestamp_t                 byte_count_start;
        double                      bps;
        /* Playout stats (most in pdb_entry_t)                               */
	u_char                      toged_cont;	     /* Toged in a row       */
        uint16_t                    toged_mask;      /* bitmap hist. of tog  */
	uint32_t		    magic;	     /* For debugging        */
} source;

/* A linked list is used for sources and this is fine since we mostly expect */
/* 1 or 2 sources to be simultaneously active and so efficiency is not a     */
/* killer.                                                                   */

typedef struct s_source_list {
        source  sentinel;
        uint16_t nsrcs;
} source_list;

/*****************************************************************************/
/* Source List functions.  Source List is used as a container for sources    */
/*****************************************************************************/

int
source_list_create(source_list **pplist)
{
        source_list *plist = (source_list*)xmalloc(sizeof(source_list));
        if (plist != NULL) {
                *pplist = plist;
                plist->sentinel.next = &plist->sentinel;
                plist->sentinel.prev = &plist->sentinel;
                plist->nsrcs = 0;
                return TRUE;
        }
        return FALSE;
}

void
source_list_clear(source_list *plist)
{
       assert(plist != NULL);

       while(plist->sentinel.next != &plist->sentinel) {
               source_remove(plist, plist->sentinel.next);
       }
}

void
source_list_destroy(source_list **pplist)
{
        source_list *plist = *pplist;
        source_list_clear(plist);
        assert(plist->nsrcs == 0);
        xfree(plist);
        *pplist = NULL;
}

uint32_t
source_list_source_count(source_list *plist)
{
        return plist->nsrcs;
}

source*
source_list_get_source_no(source_list *plist, uint32_t n)
{
        source *curr = NULL;

        assert(plist != NULL);

        if (n < plist->nsrcs) {
                curr = plist->sentinel.next;
                while(n != 0) {
                        curr = curr->next;
                        n--;
                }
                return curr;
        }
        return NULL;
}

source*
source_get_by_ssrc(source_list *plist, uint32_t ssrc)
{
        source *curr = NULL, *stop = NULL;

        curr = plist->sentinel.next;
        stop = &plist->sentinel;
        while(curr != stop) {
                if (curr->pdbe->ssrc == ssrc) {
                        return curr;
                }
                curr = curr->next;
        }

        return NULL;
}

/*****************************************************************************/
/* Timestamp constants and initialization                                    */
/*****************************************************************************/

static timestamp_t zero_ts;        /* No time at all :-)                            */
static timestamp_t keep_source_ts; /* How long source kept after source goes quiet  */
static timestamp_t history_ts;     /* How much old audio hang onto for repair usage */
static timestamp_t bw_avg_period;  /* Average period for bandwidth estimate         */
static timestamp_t skew_thresh;    /* Significant size b4 consider playout adapt    */
static timestamp_t skew_limit;     /* Upper bound, otherwise clock reset.           */
static timestamp_t transit_reset;  /* Period after which new transit time taken     */
        	                   /* if source has been quiet.                     */
static timestamp_t transit_jump;   /* If transit delta is bigger than this reset    */
static timestamp_t spike_jump;     /* Packet spike delay threshold (trigger).       */
static timestamp_t spike_end;      /* Value of var when spike over                  */
static timestamp_t repair_max_gap; /* Maximum stream gap repair is attempted for.   */
static int  time_constants_inited = FALSE;

static void
time_constants_init()
{
        /* We use these time constants *all* the time.   Initialize once     */
        zero_ts        = ts_map32(8000, 0);
        keep_source_ts = ts_map32(8000, 24000);
        history_ts     = ts_map32(8000, 2000);
        bw_avg_period  = ts_map32(8000, 8000);
        skew_thresh    = ts_map32(8000, 320);
        skew_limit     = ts_map32(8000, 4000);
        transit_reset  = ts_map32(8000, 80000);
	transit_jump   = ts_map32(8000, 12000);
        spike_jump     = ts_map32(8000, 3000);
        spike_end      = ts_map32(8000, 64);
        repair_max_gap = ts_map32(8000, 1600); /* 200ms */
        time_constants_inited = TRUE;
}

/*****************************************************************************/
/* Source functions.  A source is an active audio source.                    */
/*****************************************************************************/

static void
source_validate(source *s)
{
	/* More debugging code... check the invarients of the soure. */
	/* This is called from all the routines here... if anything  */
	/* is trashing the source, this is supposed to detect it.    */
	assert(s != NULL);
	assert(s->magic == 0xface0ff);
#ifdef DEBUG
	assert(s->next != NULL);
	assert(s->prev != NULL);
        assert(s->pdbe != NULL);
        assert(s->bps  >= 0);
	assert((s->skew == SOURCE_SKEW_SLOW) || (s->skew == SOURCE_SKEW_FAST) ||( s->skew == SOURCE_SKEW_NONE));
	assert((s->playout_mode == PLAYOUT_MODE_NORMAL) || (s->playout_mode == PLAYOUT_MODE_SPIKE));
	assert(ts_valid(s->pdbe->playout));
#endif
}

source*
source_create(source_list    *plist,
              uint32_t        ssrc,
	      pdb_t	     *pdb)
{
        source *psrc;
        int     success;

        assert(plist != NULL);
        assert(source_get_by_ssrc(plist, ssrc) == NULL);

        /* Time constant initialization. Nothing to do with source creation  */
        /* just has to go somewhere before sources might be active, here it  */
        /* definitely is!                                                    */
        if (time_constants_inited == FALSE) {
                time_constants_init();
        }

        /* On with the show...                                               */
        psrc = (source*)block_alloc(sizeof(source));
        if (psrc == NULL) {
                return NULL;
        }
        memset(psrc, 0, sizeof(source));
	psrc->magic = 0xface0ff;

        if (pdb_item_get(pdb, ssrc, &psrc->pdbe) == FALSE) {
                debug_msg("Persistent database item not found\n");
                abort();
        }

        psrc->pdbe->first_mix  = 1; /* Used to note nothing mixed anything   */
        psrc->toged_cont       = 0; /* Reset continuous thrown on ground cnt */
        psrc->toged_mask       = 0;
        psrc->channel_state    = NULL;
        psrc->skew             = SOURCE_SKEW_NONE;
        psrc->samples_played   = 0;
        psrc->samples_added    = 0;
        psrc->spike_var        = zero_ts;
	psrc->last_repair      = zero_ts;
	psrc->hold_repair      = 0;

        /* Allocate channel and media buffers                                */
        success = pb_create(&psrc->channel,
                            (playoutfreeproc)channel_data_destroy);
        if (!success) {
                debug_msg("Failed to allocate channel buffer\n");
                goto fail_create_channel;
        }

        success = pb_create(&psrc->media, (playoutfreeproc)media_data_destroy);
        if (!success) {
                debug_msg("Failed to allocate media buffer\n");
                goto fail_create_media;
        }

        success = pb_iterator_create(psrc->media, &psrc->media_pos);
        if (!success) {
                debug_msg("Failed to attach iterator to media buffer\n");
                goto fail_create_iterator;
        }

        success = codec_state_store_create(&psrc->codec_states, DECODER);
        if (!success) {
                debug_msg("Failed to allocate codec state storage\n");
                goto fail_create_states;
        }

        success = pktbuf_create(&psrc->pktbuf, 8);
        if (!success) {
                debug_msg("Failed to allocate packet buffer\n");
                goto fail_pktbuf;
        }

        /* List maintenance    */
        psrc->next = plist->sentinel.next;
        psrc->prev = &plist->sentinel;
        psrc->next->prev = psrc;
        psrc->prev->next = psrc;
        plist->nsrcs++;

        debug_msg("Created source decode path\n");

	source_validate(psrc);
        return psrc;

        /* Failure fall throughs */
fail_pktbuf:
        codec_state_store_destroy(&psrc->codec_states);
fail_create_states:
        pb_iterator_destroy(psrc->media, &psrc->media_pos);
fail_create_iterator:
        pb_destroy(&psrc->media);
fail_create_media:
        pb_destroy(&psrc->channel);
fail_create_channel:
        block_free(psrc, sizeof(source));

        return NULL;
}

/* All sources need to be reconfigured when anything changes in
 * audio path.  These include change of device frequency, change of
 * the number of channels, etc..
 */

static void
source_reconfigure(source        *src,
		   cc_id_t	  ccid,
                   uint8_t	  codec_pt,
		   uint16_t	  units_per_packet,
                   converter_id_t conv_id,
		   int            render_3d,
                   uint16_t       out_rate,
                   uint16_t       out_channels)
{
        uint16_t    		 src_rate, src_channels;
        codec_id_t            	 src_cid;
        const codec_format_t 	*src_cf;
	codec_id_t           	 cid;
	uint32_t            	 samples_per_frame;

	source_validate(src);

	cid = codec_get_by_payload(codec_pt);
	src_cf  = codec_get_format(cid);
	/* Fix details... */
	src->pdbe->enc              = codec_pt;
	src->pdbe->units_per_packet = units_per_packet;
	src->pdbe->channel_coder_id = ccid;
	if (src->channel_state != NULL) {
		channel_decoder_destroy(&(src->channel_state));
		pb_flush(src->channel);
	}
	channel_decoder_create(src->pdbe->channel_coder_id, &(src->channel_state));
	samples_per_frame   = codec_get_samples_per_frame(cid);
	debug_msg("Reconfiguring source:\n");
	debug_msg("    samples per frame = %d\n", samples_per_frame);
	debug_msg("    frames per packet = %d\n", units_per_packet);
	debug_msg("    audio sample rate = %d\n", src_cf->format.sample_rate);
	src->pdbe->sample_rate   = src_cf->format.sample_rate;
	src->pdbe->inter_pkt_gap = src->pdbe->units_per_packet * (uint16_t)samples_per_frame;
	src->pdbe->frame_dur     = ts_map32(src_cf->format.sample_rate, samples_per_frame);

        /* Set age to zero and flush existing media
         * so that repair mechanism does not attempt
         * to patch across different block sizes.
         */

        src->age = 0;
        pb_flush(src->media);

        /* Get rate and channels of incoming media so we know
         * what we have to change.
         */
        src_cid = codec_get_by_payload(src->pdbe->enc);
        src_cf  = codec_get_format(src_cid);
        src_rate     = (uint16_t)src_cf->format.sample_rate;
        src_channels = (uint16_t)src_cf->format.channels;

        if (render_3d) {
                assert(out_channels == 2);
                /* Rejig 3d renderer if there, else create */
                if (src->pdbe->render_3D_data) {
                        int azi3d, fil3d, len3d;
                        render_3D_get_parameters(src->pdbe->render_3D_data, &azi3d, &fil3d, &len3d);
                        render_3D_set_parameters(src->pdbe->render_3D_data, (int)src_rate, azi3d, fil3d, len3d);
                } else {
                        src->pdbe->render_3D_data = render_3D_init((int)src_rate);
                }
                assert(src->pdbe->render_3D_data);
                /* Render 3d is before sample rate/channel conversion, and   */
                /* output 2 channels.                                        */
                src_channels = 2;
        } else {
                /* Rendering is switched off so destroy info.                */
                if (src->pdbe->render_3D_data != NULL) {
                        render_3D_free(&src->pdbe->render_3D_data);
                }
        }

        /* Now destroy converter if it is already there.                     */
        if (src->converter) {
                converter_destroy(&src->converter);
        }

        if (src_rate != out_rate || src_channels != out_channels) {
                converter_fmt_t c;
                c.src_freq      = src_rate;
                c.src_channels = src_channels;
                c.dst_freq      = out_rate;
                c.dst_channels   = out_channels;
                converter_create(conv_id, &c, &src->converter);
        }
        src->byte_count = 0;
        src->bps        = 0.0;
	source_validate(src);
}

void
source_remove(source_list *plist, source *psrc)
{
	source_validate(psrc);
        assert(plist);
        assert(psrc);
        assert(source_get_by_ssrc(plist, psrc->pdbe->ssrc) != NULL);

        psrc->next->prev = psrc->prev;
        psrc->prev->next = psrc->next;

        if (psrc->channel_state) {
                channel_decoder_destroy(&psrc->channel_state);
        }

        if (psrc->converter) {
                converter_destroy(&psrc->converter);
        }

        pb_iterator_destroy(psrc->media, &psrc->media_pos);
        pb_destroy(&psrc->channel);
        pb_destroy(&psrc->media);
        codec_state_store_destroy(&psrc->codec_states);
        pktbuf_destroy(&psrc->pktbuf);
        plist->nsrcs--;

        debug_msg("Destroying source decode path\n");

        block_free(psrc, sizeof(source));

        assert(source_get_by_ssrc(plist, psrc->pdbe->ssrc) == NULL);
}

/* Source Processing Routines ************************************************/

/* Returns true if fn takes ownership responsibility for data */
static int
source_process_packet (source  		*src,
                       u_char  		*pckt,
                       uint32_t 	 pckt_len,
                       uint8_t  	 payload,
                       timestamp_t       playout)
{
        channel_data *cd;
        channel_unit *cu;
        cc_id_t       cid;
        uint8_t       clayers;

	source_validate(src);
        assert(src  != NULL);
        assert(pckt != NULL);

        /* Need to check:
         * (i) if layering is enabled
         * (ii) if channel_data exists for this playout point (if pb_iterator_get_at...)
         * Then need to:
         * (i) create cd if doesn't exist
         * (ii) add packet to cd->elem[layer]
         * We work out layer number by deducting the base port
         * no from the port no this packet came from
         * But what if layering on one port?
         */

        /* Or we could:
         * (i) check if cd exists for this playout point
         * (ii) if so, memcmp() to see if this packet already exists (ugh!)
         */

        cid = channel_coder_get_by_payload(payload);
        clayers = channel_coder_get_layers(cid);
        if (clayers > 1) {
                struct s_pb_iterator *pi;
                uint8_t i;
                uint32_t clen;
                int dup;
                timestamp_t lplayout;
                pb_iterator_create(src->channel, &pi);
                while(pb_iterator_advance(pi)) {
                        u_char *cd_get;
                        pb_iterator_get_at(pi, &cd_get, &clen, &lplayout);
                        cd = (channel_data*)cd_get;
                        /* if lplayout==playout there is already
                          channel_data for this playout point */
                        if (!ts_eq(playout, lplayout)) {
                                continue;
                        }
                        pb_iterator_detach_at(pi, &cd_get, &clen, &lplayout);
                        cd = (channel_data*)cd_get;
                        assert(cd->nelem >= 1);

                       /* if this channel_data is full, this new packet must *
                        * be a duplicate, so we don't need to check          */
                        if (cd->nelem >= clayers) {
                                debug_msg("source_process_packet failed - duplicate layer\n");
                                src->pdbe->duplicates++;
                                pb_iterator_destroy(src->channel, &pi);
                                goto done;
                        }

                        cu = (channel_unit*)block_alloc(sizeof(channel_unit));
                        cu->data     = pckt;
                        cu->data_len = pckt_len;
                        cu->pt       = payload;

                        dup = 0;

                       /* compare existing channel_units to this one */
                        for (i=0; i<cd->nelem; i++) {
                                if(cu->data_len!=cd->elem[i]->data_len) break;
                                /* This memcmp arbitrarily only checks
                                 * 20 bytes, otherwise it takes too
                                 * long */
                                if (memcmp(cu->data, cd->elem[i]->data, 20) == 0) {
                                        dup=1;
                                }
                        }

                       /* duplicate, so stick the channel_data back on *
                        * the playout buffer and swiftly depart        */
                        if (dup) {
                                debug_msg("source_process_packet failed - duplicate layer\n");
                                src->pdbe->duplicates++;
                                /* destroy temporary channel_unit */
                                block_free(cu->data, cu->data_len);
                                cu->data_len = 0;
                                block_free(cu, sizeof(channel_unit));
                                pb_iterator_destroy(src->channel, &pi);
                                goto done;
                        }

                       /* add this layer if not a duplicate           *
                        * NB: layers are not added in order, and thus *
                        * have to be reorganised in the layered       *
                        * channel coder                               */
                        cd->elem[cd->nelem] = cu;
                        cd->nelem++;
                        pb_iterator_destroy(src->channel, &pi);
                        goto done;
                }
                pb_iterator_destroy(src->channel, &pi);
        }

        if (channel_data_create(&cd, 1) == 0) {
                return FALSE;
        }

        cu               = cd->elem[0];
        cu->data         = pckt;
        cu->data_len     = pckt_len;
        cu->pt           = payload;

	src->age++;
done:
        if (pb_add(src->channel, (u_char*)cd, sizeof(channel_data), playout) == FALSE) {
                src->pdbe->duplicates++;
                channel_data_destroy(&cd, sizeof(channel_data));
        }

	source_validate(src);
        return TRUE;
}

#ifdef SOURCE_LOG_PLAYOUT

static FILE *psf; /* Playout stats file */
static uint32_t t0;

static void
source_close_log(void)
{
        if (psf) {
                fclose(psf);
                psf = NULL;
        }
}

static void
source_playout_log(source *src, uint32_t ts, timestamp_t now)
{
	source_validate(src);
        if (psf == NULL) {
                psf = fopen("playout.log", "w");
                if (psf == NULL) {
                        fprintf(stderr, "Could not open playout.log\n");
                } else {
                        atexit(source_close_log);
                        fprintf(psf, "# <SSRC> <RTP timestamp> <talkstart> <jitter> <transit> <avg transit> <last transit> <playout del> <spike_var> <arr time>\n");
                }
                t0 = ts - 1000; /* -1000 in case of out of order first packet */
        }

        fprintf(psf, "0x%08x %.6f %5u %5u %5u %5u %5u %5u %5u %5u\n",
		src->pdbe->ssrc,
                (ts - t0)/8000.0,
                timestamp_to_ms(src->talkstart),
                timestamp_to_ms(src->pdbe->jitter),
                timestamp_to_ms(src->pdbe->transit),
                timestamp_to_ms(src->pdbe->avg_transit),
                timestamp_to_ms(src->pdbe->last_transit),
                timestamp_to_ms(src->pdbe->playout),
                timestamp_to_ms(src->spike_var),
                timestamp_to_ms(now)
                );
	source_validate(src);
}

#endif /* SOURCE_LOG_PLAYOUT */

static void
source_update_toged(source *src, int toged)
{
	source_validate(src);
        src->toged_mask <<= 1;
        src->toged_mask |= toged;
        src->toged_cont = 0;
        if (toged == 1) {
                int m;
                m = src->toged_mask & 0xff; /* Last 8 packets */
                while (m) {
                        src->toged_cont += (m & 1);
                        m >>= 1;
                }
        }
	source_validate(src);
}


static void
sanity_check_playout_time(timestamp_t now, timestamp_t playout)
{
	assert(ts_valid(now));
	assert(ts_valid(playout));
	/* Check that the calculated playout time is within 10 seconds */
	/* of the current time. This is an arbitrary check, but if it  */
	/* fails something has almost certainly gone wrong...          */
	assert(timestamp_to_ms(ts_abs_diff(now, playout)) < 10000);
}

static void
source_process_packets(session_t *sp, source *src, timestamp_t now)
{
	/* This function calculates the desired playout point for each packet and */
	/* inserts it into the channel decoder input buffer (src->channel) at the */
	/* correct time interval.                                                 */
        timestamp_t    src_ts, playout, transit;
        pdb_entry_t    *e;
        rtp_packet     *p;
        cc_id_t         ccid = -1;
        uint16_t        units_per_packet = -1;
        uint32_t        delta_ts, delta_seq;
        uint8_t         codec_pt;
        uint8_t         adjust_playout;

	source_validate(src);
        e = src->pdbe;

        /* Timing of startup is such that sometimes we get huge burst of packets */
        /* between source creation and first round of packet processing.  Causes */
        /* too much audio to be buffered and skew adjustment make lots of adjust */
        /* actions unnecessarily.                                                */
        if (src->packets_done == 0) {
                int16_t discarded = 0;
                while(pktbuf_get_count(src->pktbuf) > 1) {
                        pktbuf_dequeue(src->pktbuf, &p);
                        discarded++;
                        xfree(p);
                }
                if (discarded > 0) {
                        debug_msg("Discarded %d surplus packets\n", discarded);
                }
        }

	/* Loop for each new packet we have received...  */
        while(pktbuf_dequeue(src->pktbuf, &p)) {
                adjust_playout = FALSE;

                ccid = channel_coder_get_by_payload((u_char)p->fields.pt);
                if (channel_verify_and_stat(ccid, (u_char)p->fields.pt, (unsigned char *)p->meta.data, p->meta.data_len, &units_per_packet, &codec_pt) == FALSE) { //SV-XXX cast 3rd arg into uchar as per channel.h
                        debug_msg("Packet discarded for ssrc 0x%08lx: packet failed channel verify.\n", e->ssrc);
                        xfree(p);
                        continue;
                }

                if (e->channel_coder_id != ccid ||
                    e->enc              != codec_pt ||
                    e->units_per_packet != units_per_packet ||
                    src->packets_done == 0) {
                        /* Either the channel coder, payload type or number of units */
			/* per packet has changed (or this is the first packet from  */
			/* this source, and so these have not been initialized). We  */
			/* reconfigure the source and update the user interface...   */
			const audio_format   	*dev_fmt = audio_get_ofmt(sp->audio_device);
                        channel_describe_data(ccid, codec_pt, (unsigned char *)p->meta.data, p->meta.data_len, src->pdbe->enc_fmt, src->pdbe->enc_fmt_len); //SV-XXX cast 3rd arg to uchar as per channel.h
                        source_reconfigure(src, ccid, codec_pt, units_per_packet, sp->converter, sp->render_3d,
                                           (uint16_t)dev_fmt->sample_rate,
                                           (uint16_t)dev_fmt->channels);
			if (sp->mbus_engine) {
				ui_send_stats(sp, sp->mbus_ui_addr, src->pdbe->ssrc);
			}
                        adjust_playout = TRUE;
		}

		/* We have a heap of conditions to check before we get to the  */
		/* playout calculation.  These are primarily to detect whether */
		/* we have a new talkspurt as indicated by marker bit, or an   */
		/* implicit new talkspurt indicated by change in relationship  */
		/* between timestamps or sequence numbers, or whether the      */
		/* config has changed at the receiver or sender.               */
		/*                                                             */
		/* We also have to check for "spikes" in packet arrivals as we */
		/* do not want to consider these packets in the playout        */
		/* calculation.                                                */

		/* Marker bit set: explicit indication of new talkspurt */
                if (p->fields.m) {
                        adjust_playout = TRUE;
                        debug_msg("Adjusting playout: marker bit set\n");
                }

		/* Check for change in timestamp-sequence number relationship. */
		/* This is an implicit indication of a new talkspurt (e.g. if  */
		/* the packet containing the marker bit was lost.              */
                delta_seq = p->fields.seq - e->last_seq;
                delta_ts  = p->fields.ts  - e->last_ts;
                if (delta_seq * e->inter_pkt_gap != delta_ts) {
                        debug_msg("Adjusting playout: sequence number/timestamp realignment\n");
                        adjust_playout = TRUE;
                }

		/* transit is difference between our clock and their  */
		/* clock. Note, we have to put through sequencer      */
		/* because our time representation is shorter than    */
		/* RTP's 32bits.  Mapping use first order differences */
		/* to update time representation                      */
                src_ts = ts_seq32_in(&e->seq, e->sample_rate, p->fields.ts);
                transit = ts_sub(now, src_ts);

		if (src->packets_done == 0  || ts_gt(ts_abs_diff(transit, e->transit), transit_jump)) {
			/* Need a fresh transit estimate */
			debug_msg("Transit estimate reset %s\n", (src->packets_done == 0)?"(first packet)":"");
			e->transit = e->last_transit = e->last_last_transit = transit;
			e->avg_transit = transit;
			adjust_playout = TRUE;
		}

		/* Check neither we nor source has changed sampling rate */
		if (ts_get_freq(transit) != ts_get_freq(e->last_transit)) {
			debug_msg("Adjusting playout: sampling rate change (either local or remote)\n");
			adjust_playout = TRUE;
			e->received = 0;
		}

                /* Spike adaptation - Ramjee, Kurose, Towsley, and Schulzerinne.   */
                /* Adaptive Playout Mechanisms for Packetized Audio Applications   */
                /* in Wide-Area Networks, IEEE Infocom 1994, pp 680-688.           */
                if (adjust_playout) {
			/* If we're about to adjust the playout point, we ignore spike events... */
			if (src->playout_mode == PLAYOUT_MODE_SPIKE) {
				debug_msg("Leaving spike mode due to required playout adjustment\n");
			}
                        src->playout_mode = PLAYOUT_MODE_NORMAL;
                } else {
			/* ...otherwise, we track spikes in the transit delay. */
                        timestamp_t delta_transit = ts_abs_diff(transit, e->last_transit);
                        if (ts_gt(delta_transit, spike_jump)) {
				/* Transit delay increased suddenly - this is a "spike" */
                                debug_msg("Entering spike mode (%d, %dHz) > (%d, %dHz))\n",
					  delta_transit.ticks, ts_get_freq(delta_transit),
					  spike_jump.ticks, ts_get_freq(spike_jump));
				debug_msg("transit (%d, %dHz) last_transit (%d, %dHz)\n",
					  transit.ticks, ts_get_freq(transit),
					  e->last_transit.ticks, ts_get_freq(e->last_transit));
                                src->playout_mode = PLAYOUT_MODE_SPIKE;
                                src->spike_var    = zero_ts;
                                e->spike_events++;
                        } else {
				if (src->playout_mode == PLAYOUT_MODE_SPIKE) {
					timestamp_t delta_var;
					src->spike_var = ts_div(src->spike_var, 2);
					delta_var = ts_add(ts_abs_diff(transit, e->last_transit),
							   ts_abs_diff(transit, e->last_last_transit));
					delta_var = ts_div(delta_var, 8);
					src->spike_var = ts_add(src->spike_var, delta_var);
					if (ts_gt(spike_end, src->spike_var)) {
						debug_msg("Leaving spike mode\n");
						src->playout_mode = PLAYOUT_MODE_NORMAL;
					}
				}
                        }
                }

                /* Check for continuous number of packets being discarded.   */
                /* This happens when jitter or transit estimate is no longer */
                /* consistent with the real world.                           */
                if (src->toged_cont >= NO_TOGED_CONT_FOR_PLAYOUT_RECALC) {
                        debug_msg("Adjusting playout: many consecutive discarded packets\n");
                        adjust_playout  = TRUE;
                        src->toged_cont = 0;
                        /* We've been dropping packets so take a new transit */
                        /* estimate and discard all existing transit info.   */
			e->transit = e->last_transit = e->last_last_transit = transit;
			e->avg_transit = transit;
                }

                if (adjust_playout && (ts_gt(ts_sub(now, e->last_arr), transit_reset) || (e->received < 20))) {
                        /* Source has been quiet for a long time.  Discard   */
                        /* old average transit estimate.                     */
			debug_msg("Average transit reset (%d -> %d)\n", timestamp_to_ms(transit), timestamp_to_ms(e->avg_transit));
			e->transit           = transit;
			e->last_transit      = transit;
			e->last_last_transit = transit;
			e->avg_transit       = transit;
                }

                /* Calculate the playout point for this packet.              */
                /* Playout calc updates avg_transit and jitter.              */
                /* Do not call if in spike mode as it distorts both.         */
                if (src->playout_mode == PLAYOUT_MODE_NORMAL) {
                        playout = playout_calc(sp, e->ssrc, transit, adjust_playout);
                } else {
                        playout = e->playout;
			debug_msg("in spike\n");
                }
                playout = ts_add(e->transit, playout);
                playout = ts_add(src_ts, playout);
		//debug_msg("%d %d\n", timestamp_to_ms(playout), timestamp_to_ms(now));
		sanity_check_playout_time(now, playout);

		/* At this point we know the desired playout time for this packet, */
		/* and adjust_playout is set if this has changed from the previous */
		/* packet.                                                         */
                if (adjust_playout) {
                        if (ts_gt(playout, now) == FALSE) {
                                /* This is the first packet in this spurt and */
                                /* it would not have been played out.  Push   */
                                /* back to point where it will...             */
                                /* This usually happens because of VAD check  */
                                /* above...                                   */
                                timestamp_t shortfall = ts_sub(now, playout);
                                /* And then a little more...                  */
                                shortfall  = ts_add(shortfall, e->frame_dur);
                                e->playout = ts_add(e->playout, shortfall);
                                playout    = ts_add(playout, shortfall);
                                debug_msg("Pushed back first packet - would have missed playout time\n");
                                assert(ts_gt(playout, now));
                        }
                        if (ts_valid(src->next_played) && ts_gt(src->next_played, playout)) {
                                /* Talkspurts would have overlapped.  May     */
                                /* cause problems for redundancy decoder.     */
                                /* Don't take any chances.                    */
                                timestamp_t overlap = ts_sub(src->next_played, playout);
                                debug_msg("Overlap %d us (next_played %d (%dhz) playout %d (%dHz))\n",
					  timestamp_to_us(overlap),
					  src->next_played.ticks, ts_get_freq(src->next_played),
					  playout.ticks, ts_get_freq(playout));
                                e->playout   = ts_add(e->playout, overlap);
                                playout      = ts_add(playout, overlap);
                        }
                        src->talkstart = playout; /* Note start of new talkspurt  */
                        src->post_talkstart_units = 0;
                } else {
                        src->post_talkstart_units++;
                }

                if (src->packets_done == 0) {
                        /* This is first packet so expect next played to have its */
                        /* playout.                                               */
                        src->next_played = playout;
                }

		sanity_check_playout_time(now, playout);

                if (ts_gt(now, playout)) {
                        /* Packet being decoded is before start of current   */
                        /* so there is no way its audio will be played       */
                        /* Playout recalculation gets triggered in           */
                        /* rtp_callback if toged_cont hits a critical        */
                        /* threshold.  It signifies current playout delay    */
                        /* is inappropriate.                                 */
                        if (src->playout_mode == PLAYOUT_MODE_NORMAL) {
                                debug_msg("Packet late (compared to now)\n");
                                source_update_toged(src, 1);
                                src->pdbe->jit_toged++;
                        } else {
                                /* Spike mode - don't worry about jit_toged  */
                                src->pdbe->spike_toged++;
                        }
                } else {
			/* This packet arrived in time to be played out. We  */
			/* add it to the channel decoder buffer at the point */
			/* determined by the playout delay.                  */
                        u_char  *u = (u_char*)block_alloc(p->meta.data_len);
                        memcpy(u, p->meta.data, p->meta.data_len);
                        if (source_process_packet(src, u, p->meta.data_len, codec_pt, playout) == FALSE) {
				debug_msg("Unwanted packet?\n");
                                block_free(u, (int)p->meta.data_len);
                        }
                        source_update_toged(src, 0);
                }

		/* Signal the playout delay to the video tool, so it can lip */
		/* sync with us.                                             */
		if (adjust_playout && sp->sync_on) {
			mbus_qmsgf(sp->mbus_engine, sp->mbus_video_addr, FALSE, "rtp.source.playout", "\"%08lx\" %d",
				   src->pdbe->ssrc, timestamp_to_ms(ts_abs_diff(playout, now)));
		}

                /* Update persistent database fields... */
                if (e->last_seq > p->fields.seq) {
                        e->misordered++;
                }
                e->last_seq          = p->fields.seq;
                e->last_ts           = p->fields.ts;
                e->last_arr          = now;
                e->last_last_transit = e->last_transit;
                e->last_transit      = transit;

		/* This would be a good place to log a histogram of loss     */
		/* lengths, right? llhist[p->seq - e->last_seq]++ after a    */
		/* check that this is not the first packet in a talkspurt.   */
		/* We could then feed it back to the sender in our reception */
		/* reports, where it could be used to adapt the redundancy   */
		/* offset, for example. [csp]                                */

#ifdef SOURCE_LOG_PLAYOUT
                source_playout_log(src, p->ts, now);
#endif /* SOURCE_LOG_PLAYOUT */
                src->packets_done++;
                xfree(p);
        }
	source_validate(src);
}

int
source_add_packet (source     *src,
                   rtp_packet *pckt)
{
	source_validate(src);
        src->byte_count += pckt->meta.data_len;
        return pktbuf_enqueue(src->pktbuf, pckt);
}

static void
source_update_bps(source *src, timestamp_t now)
{
        timestamp_t delta;

	source_validate(src);
        if (!ts_valid(src->byte_count_start)) {
                src->byte_count_start = now;
                src->byte_count       = 0;
                src->bps              = 0.0;
		source_validate(src);
                return;
        }

        delta = ts_sub(now, src->byte_count_start);

        if (ts_gt(delta, bw_avg_period)) {
                double this_est;
                this_est = 8.0 * src->byte_count * 1000.0/ timestamp_to_ms(delta);
                if (src->bps == 0.0) {
                        src->bps = this_est;
                } else {
                        src->bps += (this_est - src->bps)/2.0;
                }
                src->byte_count = 0;
                src->byte_count_start = now;
        }
	source_validate(src);
}

double
source_get_bps(source *src)
{
        return src->bps;
}

static int16_t
find_local_match(sample *buffer, uint16_t wstart, uint16_t wlen, uint16_t sstart, uint16_t send, uint16_t channels)
{
        uint16_t i,j, i_min = sstart;
        uint32_t score = 0, score_min = 0xffffffff;

        for (i = sstart; i < send; i += channels) {
                score = 0;
                for(j = 0; j < wlen; j += channels) {
                        score += abs((int32_t)buffer[wstart + j] - (int32_t)buffer[i + j]);
                }
                if (score <= score_min) {
                        score_min = score;
                        i_min     = i;
                }
        }

        if (score_min / wlen < MATCH_THRESHOLD) {
                return i_min / channels;
        }
        return -1;
}

/* recommend_skew_adjust_dur examines a frame to determine how much audio    */
/* to insert or drop.   Argument drop is boolean to indicate whether         */
/* dropping samples (TRUE) or inserting (FALSE).                             */

static int32_t
recommend_skew_adjust_dur(media_data *md, int drop, timestamp_t *adjust)
{
        int16_t   matchlen;
        uint32_t  rate;
	uint16_t  channels, samples;
        sample   *buffer;
        int16_t   i;

        i = md->nrep - 1;
        while(i >= 0) {
                if (codec_get_native_info(md->rep[i]->id, &rate, &channels)) {
                        break;
                }
                i--;
        }
        assert(i != -1);

        buffer  = (sample*)md->rep[i]->data;
        samples = md->rep[i]->data_len / (sizeof(sample) * channels);
        if (drop) {
                /* match with first samples of frame start just past
                 * search window and finish at end of frame
                 */
                matchlen = find_local_match((sample*)md->rep[i]->data,                                     /* buffer            */
                                            0,                                                             /* window start      */
                                            (uint16_t)(SOURCE_COMPARE_WINDOW_SIZE * channels),             /* window len        */
                                            (uint16_t)(SOURCE_COMPARE_WINDOW_SIZE * channels),             /* search area start */
                                            (uint16_t)((samples - SOURCE_COMPARE_WINDOW_SIZE) * channels), /* search area len   */
                                            channels);
                if (matchlen == -1) {
                        return FALSE;
                }
        } else {
                /* match with last samples of frame.  Start at the
                 * start of frame and finish just before search window.
                 */
                matchlen = find_local_match((sample*)md->rep[i]->data,                                         /* buffer */
                                            (uint16_t)((samples - SOURCE_COMPARE_WINDOW_SIZE) * channels),     /* wstart */
                                            (uint16_t)(SOURCE_COMPARE_WINDOW_SIZE * channels),                 /* wlen   */
                                            0,                                                                 /* sstart */
                                            (uint16_t)((samples - 2 * SOURCE_COMPARE_WINDOW_SIZE) * channels), /* slen   */
                                            channels);
                /* Want to measure from where frames will overlap. */
                if (matchlen == -1) {
                        return FALSE;
                }
                matchlen += SOURCE_COMPARE_WINDOW_SIZE;
        }
        assert(matchlen >= 0);
        assert(matchlen <= samples);
        *adjust = ts_map32(rate, matchlen);
        return TRUE;
}

static void
conceal_dropped_samples(media_data *md, timestamp_t drop_dur)
{
        /* We are dropping drop_dur samples and want signal to be            */
        /* continuous.  So we blend samples that would have been played if   */
        /* they weren't dropped with where signal continues after the drop.  */
        uint32_t  drop_samples;
        uint32_t  rate;
	uint16_t  channels;
        int32_t   i;
        sample   *new_start, *buf;

        for (i = md->nrep - 1; i >= 0; i--) {
                if (codec_get_native_info(md->rep[i]->id, &rate, &channels)) {
                        break;
                }
        }

        assert(i != -1);

        buf          = (sample*)md->rep[i]->data;
        drop_dur     = ts_convert(rate, drop_dur);
        drop_samples = channels * drop_dur.ticks;
        new_start    = buf + drop_samples;

        audio_blend(buf, new_start, new_start, SOURCE_MERGE_LEN_SAMPLES, channels);
        xmemchk();
}

/* Source conceal_inserted_samples blends end of omd with overlap in imd    */
/* just before insert takes over.  Aims to provide transparent transitition */
/* between added block and old block.                                       */

static void
conceal_inserted_samples(media_data *omd, media_data *imd, timestamp_t insert_dur)
{
        uint32_t  rate;
	uint16_t  channels;
        uint32_t  dst_samples, src_samples, skip;
        int32_t   i;
        sample   *dst, *src;

        assert(omd != NULL);
        assert(imd != NULL);

        for (i = omd->nrep - 1; i >= 0; i--) {
                if (codec_get_native_info(omd->rep[i]->id, &rate, &channels)) {
                        break;
                }
        }
        assert(i >= 0);

        for (i = imd->nrep - 1; i >= 0; i--) {
                if (codec_get_native_info(imd->rep[i]->id, &rate, &channels)) {
                        break;
                }
        }
        assert(i >= 0);

        dst_samples = omd->rep[i]->data_len / sizeof(sample);
        dst         = ((sample*)omd->rep[i]->data) + dst_samples - SOURCE_MERGE_LEN_SAMPLES * channels;


        src_samples = imd->rep[i]->data_len / sizeof(sample);
        skip        = insert_dur.ticks * channels - SOURCE_MERGE_LEN_SAMPLES;
        if (skip > src_samples - SOURCE_MERGE_LEN_SAMPLES * channels) {
                debug_msg("Clipping insert length\n");
                skip = src_samples - SOURCE_MERGE_LEN_SAMPLES * channels;
        }
        src = ((sample*)imd->rep[i]->data) + skip;

        xmemchk();
        audio_blend(dst, src, dst, channels * SOURCE_MERGE_LEN_SAMPLES, channels);
        xmemchk();
}

/* source_check_buffering is supposed to check amount of audio buffered      */
/* corresponds to what we expect from playout so we can think about skew     */
/* adjustment.                                                               */

int
source_check_buffering(source *src)
{
        timestamp_t actual, desired, diff;

	source_validate(src);
        if (src->post_talkstart_units < 20) {
                /* If the source is new(ish) then not enough audio will be   */
                /* in the playout buffer because it hasn't arrived yet.      */
                return FALSE;
        }

        actual  = source_get_audio_buffered(src);
        desired = source_get_playout_delay(src);
        diff    = ts_abs_diff(actual, desired);

        if (ts_gt(actual, desired) && ts_gt(diff, skew_thresh)) {
                src->skew_adjust = diff;
                /* We're accumulating audio, their clock faster   */
                src->skew = SOURCE_SKEW_FAST;
                src->skew_cnt++;
		source_validate(src);
                return TRUE;
        } else if (ts_gt(desired, actual)) {
                /* We're short of audio, so their clock is slower */
                /* Lower bound is much harder than upper bound    */
                /* since mixer will dry up / repair will start to */
                /* be invoked as we decode units late.            */
                src->skew_adjust = diff;
                src->skew = SOURCE_SKEW_SLOW;
		source_validate(src);
                return TRUE;
        }
        src->skew = SOURCE_SKEW_NONE;
        src->skew_adjust = zero_ts;
	source_validate(src);
        return FALSE;
}

/* source_skew_adapt exists to shift playout units if source clock appears   */
/* to be fast or slow.  The media_data unit is here so that it can be        */
/* examined to see if it is low energy and adjustment would be okay.  Might  */
/* want to be more sophisticated and put a silence detector in rather than   */
/* static threshold.                                                         */
/*                                                                           */
/* Returns what adaption type occurred.                                      */

static skew_t
source_skew_adapt(source *src, media_data *md, timestamp_t playout)
{
        uint32_t    i = 0, e = 0, samples = 0;
        uint32_t    rate;
	uint16_t    channels;
        timestamp_t adjustment, frame_dur;

	source_validate(src);
        assert(src);
        assert(md);
        assert(src->skew != SOURCE_SKEW_NONE);

        for(i = 0; i < md->nrep; i++) {
                if (codec_get_native_info(md->rep[i]->id, &rate, &channels)) {
                        samples = md->rep[i]->data_len / (channels * sizeof(sample));
                        e = audio_avg_energy((sample*)md->rep[i]->data, samples * channels, channels);
                        src->mean_energy = (15 * src->mean_energy + e)/16;
                        frame_dur = ts_map32(rate, samples);
                        break;
                }
        }

        if (i == md->nrep) {
                /* don't adapt if unit has not been decoded (error) or       */
                /* signal has too much energy                                */
		source_validate(src);
                return SOURCE_SKEW_NONE;
        }

        /* When we are making the adjustment we must shift playout buffers   */
        /* and timestamps that the source decode process uses.  Must be      */
        /* careful with last repair because it is not valid if no repair has */
        /* taken place.                                                      */

        if (src->skew == SOURCE_SKEW_FAST && src->skew_cnt > 3) {
                /* source is fast so we need to bring units forward.
                 * Should only move forward at most a single unit
                 * otherwise we might discard something we have not
                 * classified.  */

                if (ts_gt(skew_limit, src->skew_adjust)) {
                        if (recommend_skew_adjust_dur(md, TRUE, &adjustment) == FALSE) {
                                /* No suitable adjustment found, and         */
                                /* adjustment is not urgent so bail here...  */
				source_validate(src);
                                return src->skew;
                        }
                } else {
                        /* Things are really skewed.  We're more than        */
                        /* skew_limit off of where we ought to be.  Just     */
                        /* drop a frame and don't worry.                     */
                        debug_msg("Dropping Frame\n");
                        adjustment = ts_div(src->pdbe->frame_dur, 2);
                }

                if (ts_gt(adjustment, src->skew_adjust) || adjustment.ticks == 0) {
                        /* adjustment needed is greater than adjustment      */
                        /* period that best matches dropable by signal       */
                        /* matching.                                         */
			source_validate(src);
                        return SOURCE_SKEW_NONE;
                }
                debug_msg("dropping %d / %d samples\n", adjustment.ticks, src->skew_adjust.ticks);
                pb_shift_forward(src->media,   adjustment);
                pb_shift_forward(src->channel, adjustment);

                src->samples_added     += adjustment.ticks;
                src->pdbe->transit      = ts_sub(src->pdbe->transit, adjustment);
                src->skew_cnt           = 0;
                /* avg_transit and last_transit are fine.  Difference in     */
                /* avg_transit and transit triggered this adjustment.        */

                if (ts_valid(src->last_repair)) {
                        src->last_repair = ts_sub(src->last_repair, adjustment);
                }

                src->next_played = ts_sub(src->next_played, adjustment);

                /* Remove skew adjustment from estimate of skew outstanding */
                if (ts_gt(src->skew_adjust, adjustment)) {
                        src->skew_adjust = ts_sub(src->skew_adjust, adjustment);
                } else {
                        src->skew = SOURCE_SKEW_NONE;
                }

                conceal_dropped_samples(md, adjustment);
                xmemchk();

                return SOURCE_SKEW_FAST;
        } else if (src->skew == SOURCE_SKEW_SLOW) {
                media_data *fmd;
                timestamp_t        insert_playout;

                xmemchk();
                if (recommend_skew_adjust_dur(md, FALSE, &adjustment) == FALSE) {
                        debug_msg("bad match\n");
			source_validate(src);
                        return src->skew;
                }

                debug_msg("Insert %d samples\n", adjustment.ticks);
                pb_shift_units_back_after(src->media,   playout, adjustment);
                pb_shift_units_back_after(src->channel, playout, adjustment);
                src->pdbe->transit = ts_add(src->pdbe->transit, adjustment);

                /* Insert a unit: buffer looks like current frame -> gap of adjustment -> next frame */
                media_data_dup(&fmd, md);
                insert_playout = ts_add(playout, adjustment);
                xmemchk();
                if (pb_add(src->media, (u_char*)fmd, sizeof(media_data), insert_playout) == TRUE) {
                        xmemchk();
                        conceal_inserted_samples(md, fmd, adjustment);
                        xmemchk();
                } else {
                        debug_msg("Buffer push back: insert failed\n");
                        media_data_destroy(&fmd, sizeof(media_data));
                }

                if (ts_gt(adjustment, src->skew_adjust)) {
                        src->skew_adjust = zero_ts;
                } else {
                        src->skew_adjust = ts_sub(src->skew_adjust, adjustment);
                }

                src->samples_added -= adjustment.ticks;

                debug_msg("Playout buffer shift back %d samples.\n", adjustment.ticks);
                xmemchk();

                src->skew = SOURCE_SKEW_NONE;
		source_validate(src);
                return SOURCE_SKEW_SLOW;
        }

	source_validate(src);
        return SOURCE_SKEW_NONE;
}

static int
source_repair(source	  *src,
              repair_id_t  r,
              timestamp_t  fill_ts)
{
        u_char*      md_get;
        media_data  *fill_md, *prev_md;
        timestamp_t  prev_ts;
        uint32_t     success,  prev_len;

	source_validate(src);
        /* We repair one unit at a time since it may be all we need */
        if (pb_iterator_retreat(src->media_pos) == FALSE) {
                /* New packet when source still active, but dry, e.g. new talkspurt */
		timestamp_t start, end;
                debug_msg("Repair not possible no previous unit!\n");
		if (pb_get_start_ts(pb_iterator_get_playout_buffer(src->media_pos),
								   &start) &&
		    pb_get_end_ts(pb_iterator_get_playout_buffer(src->media_pos),
								 &end)) {
			debug_msg("Range available [%d - %d] want %d\n",
				  timestamp_to_ms(start),
				  timestamp_to_ms(end),
				  timestamp_to_ms(fill_ts));
		}

		source_validate(src);
                return FALSE;
        }

        pb_iterator_get_at(src->media_pos,
                           &md_get,
                           &prev_len,
                           &prev_ts);
        prev_md = (media_data*)md_get;

        media_data_create(&fill_md, 1);
        repair(r,
               src->consec_lost,
               src->codec_states,
               prev_md,
               fill_md->rep[0]);

        success = pb_add(src->media,
                         (u_char*)fill_md,
                         sizeof(media_data),
                         fill_ts);

        if (success) {
                src->consec_lost++;
                src->last_repair = fill_ts;
                /* Advance to unit we just added */
                pb_iterator_advance(src->media_pos);
		debug_msg("Repair added %d\n", timestamp_to_ms(fill_ts));
        } else {
                /* This should only ever fail at when source changes
                 * sample rate in less time than playout buffer
                 * timeout.  This should be a very very rare event...
                 */
                debug_msg("Repair add data failed %d.\n", timestamp_to_ms(fill_ts));
                media_data_destroy(&fill_md, sizeof(media_data));
                src->consec_lost = 0;
		src->hold_repair += 2;
		source_validate(src);
                return FALSE;
        }
	source_validate(src);
        return TRUE;
}

static int
source_repair_required(source *src, timestamp_t playout)
{
	timestamp_t	 gap;

	/* Repair any gap in the audio stream. Conditions for repair:  */
	/* (a) playout point of unit is further away than expected.    */
	/* (b) playout point is not too far away (repair burns cycles) */
	/* (c) playout does not correspond to new talkspurt (don't     */
	/*     fill between end of last talkspurt and start of next).  */
	/*     NB Use post_talkstart_units as talkspurts maybe longer  */
	/*     than timestamp wrap period and want to repair even if   */
	/*     timestamps wrap.                                        */
	/* (d) not start of a talkspurt.                               */
	/* (e) don't have a hold on.                                   */
	gap = ts_sub(playout, src->next_played);
	if ((ts_gt(gap, zero_ts) && ts_gt(repair_max_gap, gap)) &&
	    ((ts_gt(src->next_played, src->talkstart) &&
	      ts_gt(playout, src->talkstart)) || src->post_talkstart_units > 100) &&
	    (src->hold_repair == 0)) {
		return TRUE;
	}
	/* Repair not needed, just maintain loss related variables */
	if (src->hold_repair) {
		src->hold_repair--;
	}
	src->consec_lost = 0;
	return FALSE;
}

void
source_process(session_t 	 *sp,
               source            *src,
               timestamp_t        start_ts,    /* Real-world time           */
               timestamp_t        end_ts)      /* Real-world time + cushion */
{
        media_data  *md;
        coded_unit  *cu;
        codec_state *cs;
        uint32_t     md_len;
        timestamp_t  playout, step;
        uint32_t     sample_rate;
	uint16_t     channels;
	int	     i;

        /* Note: src->hold_repair is used to stop repair occuring.
         * Occasionally, there is a race condition when the playout
         * point is recalculated causing overlap, and when playout
         * buffer shift occurs in middle of a loss.
         */

	session_validate(sp);

	/* The call to source_process_packets() calculates the desired playout    */
	/* point for each packet and inserts it into the channel decoder input    */
	/* buffer (src->channel) at the correct time interval.                    */

	source_process_packets(sp, src, start_ts);
        if (src->packets_done == 0) {
                return;
        }
	source_validate(src);

        /* Split channel coder units up into media units. This takes units from     */
	/* the channel decoder input buffer (src->channel) and, after decoding,     */
	/* adds them to the media buffer (src->media). The channel decoder may keep */
	/* the units for some time in-between these two buffers e.g. if there is j  */
	/* a block interleaver, output will not start until a complete block has    */
	/* been read in. Any intermediate buffer is hidden within the channel       */
	/* decoder, and is not visible here.                                        */
        if (pb_node_count(src->channel)) {
                channel_decoder_decode(src->channel_state, src->channel, src->media, end_ts);
        }
	source_validate(src);

	/* The following loop pulls data out of the media buffer (src->media) when */
	/* it's time to play it out. It then repairs any gaps in the audio stream, */
	/* decodes anything still in encoded form, performs skew adaptation and    */
	/* mixes the data ready for playout.                                       */
        while (ts_gt(end_ts, src->next_played) && pb_iterator_advance(src->media_pos)) {
                u_char *md_get;
		pb_iterator_get_at(src->media_pos, &md_get, &md_len, &playout);
                md = (media_data*)md_get;
		if (source_repair_required(src, playout)) {
			if (source_repair(src, sp->repair, src->next_played)) {
				/* Repair moves media buffer iterator to start of repaired */
				/* frames, need to get media iterator position */
				int success;
				debug_msg("Repair succeeded (% 2d got % 6d exp % 6d talks % 6d)\n",
					  src->consec_lost,
					  playout.ticks,
					  src->next_played.ticks,
					  src->talkstart.ticks);
				success = pb_iterator_get_at(src->media_pos,
							     &md_get, &md_len,
							     &playout);
                                md = (media_data*)md_get;
				assert(success);
				assert(ts_eq(playout, src->next_played));
			}
		}

		/* At this point, md is the media data at the current playout point. */
		/* There may be multiple representations of the data, for example if */
		/* we are receiving a stream using redundancy.                       */
                assert(md     != NULL);
                assert(md_len == sizeof(media_data));
		assert(md->nrep < MAX_MEDIA_UNITS && md->nrep > 0);
		for(i = 0; i < md->nrep; i++) {
			assert(md->rep[i] != NULL);
			assert(codec_is_native_coding(md->rep[i]->id) || codec_id_is_valid(md->rep[i]->id));
		}

                if (ts_gt(playout, end_ts)) {
                        /* This playout point is after now so stop */
                        pb_iterator_retreat(src->media_pos);
                        break;
                }

                assert(md != NULL);
                assert(md_len == sizeof(media_data));
		assert(md->nrep < MAX_MEDIA_UNITS && md->nrep > 0);
		for(i = 0; i < md->nrep; i++) {
			assert(md->rep[i] != NULL);
			assert(codec_is_native_coding(md->rep[i]->id) || codec_id_is_valid(md->rep[i]->id));
		}

                if (!codec_is_native_coding(md->rep[md->nrep - 1]->id)) {
			/* If we've got to here, we have no native coding for this unit */
                        /* We need to decode this unit, may not have to when repair has */
			/* been used.                                                   */
                        for(i = 0; i < md->nrep; i++) {
                                /* If there is a native coding this unit has already */
				/* been decoded and this would be a bug.             */
                                assert(codec_is_native_coding(md->rep[i]->id) == FALSE);
                        }

                        /* Decode frame - use first representation available and make  */
                        /* last coded_unit in current media_data. From here on         */
                        /* codec_is_native_coding(md->rep[md->nrep - 1]) should always */
                        /* be TRUE.                                                    */
                        cu = (coded_unit*) block_alloc(sizeof(coded_unit));
                        memset(cu, 0, sizeof(coded_unit));
                        cs = codec_state_store_get(src->codec_states, md->rep[0]->id);
                        codec_decode(cs, md->rep[0], cu);
                        assert(md->rep[md->nrep] == NULL);
                        md->rep[md->nrep] = cu;
                        md->nrep++;

                        assert(md->nrep < MAX_MEDIA_UNITS && md->nrep > 0);
                        assert(codec_is_native_coding(md->rep[md->nrep - 1]->id));

                        if (sp->render_3d && src->pdbe->render_3D_data) {
                                /* 3d rendering necessary... */
                                coded_unit *decoded, *render;
                                decoded = md->rep[md->nrep - 1];
                                assert(codec_is_native_coding(decoded->id));

                                render = (coded_unit*)block_alloc(sizeof(coded_unit));
                                memset(render, 0, sizeof(coded_unit));

                                render_3D(src->pdbe->render_3D_data,decoded,render);
                                assert(md->rep[md->nrep] == NULL);
                                md->rep[md->nrep] = render;
                                md->nrep++;
                                assert(md->nrep < MAX_MEDIA_UNITS && md->nrep > 0);
                                assert(codec_is_native_coding(md->rep[md->nrep - 1]->id));
                        }

                        if (src->converter) {
                                /* convert frame */
                                coded_unit *decoded, *render;
                                decoded = md->rep[md->nrep - 1];
                                assert(codec_is_native_coding(decoded->id));

                                render = (coded_unit*) block_alloc(sizeof(coded_unit));
                                memset(render, 0, sizeof(coded_unit));
                                converter_process(src->converter, decoded, render);
                                assert(md->rep[md->nrep] == NULL);
                                md->rep[md->nrep] = render;
                                md->nrep++;
                                assert(md->nrep < MAX_MEDIA_UNITS && md->nrep > 0);
                                assert(codec_is_native_coding(md->rep[md->nrep - 1]->id));
                        }
                        if (src->pdbe->gain != 1.0 && codec_is_native_coding(md->rep[md->nrep - 1]->id)) {
                                audio_scale_buffer((sample*)md->rep[md->nrep - 1]->data,
                                                   md->rep[md->nrep - 1]->data_len / sizeof(sample),
                                                   src->pdbe->gain);
                                assert(md->nrep < MAX_MEDIA_UNITS && md->nrep > 0);
                                assert(codec_is_native_coding(md->rep[md->nrep - 1]->id));
                        }
                }

		/* From here on we're working with the native coded media... */
		assert(codec_is_native_coding(md->rep[md->nrep - 1]->id));

                if (src->skew != SOURCE_SKEW_NONE && source_skew_adapt(src, md, playout) != SOURCE_SKEW_NONE) {
			/* We have skew and we have adjusted playout buffer  */
			/* timestamps, so re-get unit to get correct         */
			/* timestamp info.                                   */
                        u_char *md_get;
                        pb_iterator_get_at(src->media_pos, &md_get, &md_len, &playout);
                        md = (media_data*)md_get;
                        assert(md != NULL);
                        assert(md_len == sizeof(media_data));
                        assert(md->nrep < MAX_MEDIA_UNITS && md->nrep > 0);
                        assert(codec_is_native_coding(md->rep[md->nrep - 1]->id));
                }

                codec_get_native_info(md->rep[md->nrep - 1]->id, &sample_rate, &channels);
                step = ts_map32(sample_rate, md->rep[md->nrep - 1]->data_len / (channels * sizeof(sample)));
                src->next_played = ts_add(playout, step);
                src->samples_played += md->rep[md->nrep - 1]->data_len / (channels * sizeof(sample));
                xmemchk();

                assert(md->nrep < MAX_MEDIA_UNITS && md->nrep > 0);
                if (mix_put_audio(sp->ms, src->pdbe, md->rep[md->nrep - 1], playout) == FALSE) {
                        /* Sources sampling rate changed mid-flow? dump data */
                        /* make source look irrelevant, it should get        */
                        /* destroyed and the recreated with proper decode    */
                        /* path when new data arrives.  Not graceful..       */
                        /* A better way would be just to flush media then    */
                        /* invoke source_reconfigure if this is ever really  */
                        /* an issue.                                         */
			debug_msg("flushing buffers ?\n");
                        pb_flush(src->media);
                        pb_flush(src->channel);
                }
		source_validate(src);
        }
        source_update_bps(src, start_ts);
}

int
source_audit(source *src)
{
	source_validate(src);
        if (src->age != 0) {
		source_validate(src);
                pb_iterator_audit(src->media_pos, history_ts);
                return TRUE;
        }
        return FALSE;
}

timestamp_t
source_get_audio_buffered (source *src)
{
        /* Changes in avg_transit change amount of audio buffered. */
        /* It's how much transit is off from start.                */
        timestamp_t delta = ts_sub(src->pdbe->transit, src->pdbe->avg_transit);
	source_validate(src);
        return ts_add(src->pdbe->playout, delta);
}

timestamp_t
source_get_playout_delay (source *src)
{
        return src->pdbe->playout;
}

int
source_relevant(source *src, timestamp_t now)
{
	source_validate(src);

        src->age++;
        if (pb_relevant(src->media, now) || pb_relevant(src->channel, now) || (src->age < 20)) {
                return TRUE;
        } if (ts_valid(src->next_played)) {
                /* Source is quiescent */
                timestamp_t quiet;
                quiet = ts_sub(now, src->next_played);
                if (ts_gt(keep_source_ts, quiet)) {
                        return TRUE;
                }
        }
        return FALSE;
}

struct s_pb*
source_get_decoded_buffer(source *src)
{
	source_validate(src);
        return src->media;
}

uint32_t
source_get_ssrc(source *src)
{
	source_validate(src);
        return src->pdbe->ssrc;
}

double
source_get_skew_rate(source *src)
{
	source_validate(src);
        if (src->samples_played) {
                double r = (double)(src->samples_played + src->samples_added) / (double)src->samples_played;
                return r;
        }
        return 1.0;
}

