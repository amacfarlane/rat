/*
 * FILE:   rtp_dump.c
 * AUTHOR: Colin Perkins <c.perkins@cs.ucl.ac.uk>
 *
 * $Revision: 3671 $
 * $Date: 2004-11-25 09:25:40 -0800 (Thu, 25 Nov 2004) $
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, is permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Computer Science
 *      Department at University College London.
 * 4. Neither the name of the University nor of the Department may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "rtp.h"
#include "rtp_dump.h"

#define SECS_BETWEEN_1900_1970 2208988800u

void rtpdump_header(FILE *logger, char *type, rtp_event *e)
{
	fprintf(logger, "%s %lu.%06lu 0x%08lx ",
	        type,
	        (unsigned long) e->ts->tv_sec + SECS_BETWEEN_1900_1970,
	        (unsigned long) e->ts->tv_usec,
	        (unsigned long) e->ssrc);
}

void rtpdump_callback(FILE *logger, rtp_event *e)
{
	rtp_packet	*p    = (rtp_packet *)     e->data;
	rtcp_sr         *sr   = (rtcp_sr *)        e->data;
	rtcp_rr         *rr   = (rtcp_rr *)        e->data;
	rtcp_sdes_item	*sdes = (rtcp_sdes_item *) e->data;
	rtcp_app        *app  = (rtcp_app *)       e->data;
	int		 i;

	/* We're just printing out the events we receive, so we shouldn't */
	/* need to use s. Plus, if we don't, this can be used as a test   */
	/* routine with the undump library, which doesn't have a valid s. */

	switch (e->type) {
		case RX_RTP:
			rtpdump_header(logger, "rtp       ", e);
			fprintf(logger, "-v %d -p %d -x %d -cc %d -m %d -pt %d -seq %u -ts %u -payload_len %d\n", 
                                p->fields.v, p->fields.p, p->fields.x, p->fields.cc, p->fields.m, p->fields.pt, p->fields.seq, (unsigned) p->fields.ts, p->meta.data_len);
			break;
		case RX_RTCP_START:
			rtpdump_header(logger, "rtcp_start", e);
			fprintf(logger, "\n");
			break;
		case RX_RTCP_FINISH:
			rtpdump_header(logger, "rtcp_end  ", e);
			fprintf(logger, "\n");
			break;
		case RX_SR:
			rtpdump_header(logger, "sr        ", e);
			fprintf(logger, "-ntp_ts %lu.%lu -rtp_ts %lu -pckts_sent %u -bytes_sent %u\n",
			        (unsigned long) ntohl(sr->ntp_sec),
				(unsigned long) ntohl(sr->ntp_frac),
				(unsigned long) ntohl(sr->rtp_ts),
				(unsigned)      ntohl(sr->sender_pcount),
				(unsigned)      ntohl(sr->sender_bcount));
			break;
		case RX_RR:
			rtpdump_header(logger, "rr        ", e);
			fprintf(logger, "-ssrc 0x%08lx -fract_lost %f -total_lost %u -seq_cycles %u -seq %u -jitter %u -lsr %u -dlsr %u\n",
			        (unsigned long) rr->ssrc,
				rr->fract_lost / 256.0,
				rr->total_lost,
				(unsigned) ((rr->last_seq & 0xffff0000) >> 16),
				(unsigned) (rr->last_seq & 0xffff),
				(unsigned) rr->jitter,
				(unsigned) rr->lsr,
				(unsigned) rr->dlsr);
			break;
		case RX_RR_EMPTY:
			rtpdump_header(logger, "rr        ", e);
			fprintf(logger, "empty\n");
			break;
		case RR_TIMEOUT:
			rtpdump_header(logger, "rr        ", e);
			fprintf(logger, "timeout\n");
			break;
		case RX_SDES:
			switch (sdes->type) {
				case RTCP_SDES_CNAME:
					rtpdump_header(logger, "cname     ", e);
					break;
				case RTCP_SDES_NAME:
					rtpdump_header(logger, "name      ", e);
					break;
				case RTCP_SDES_EMAIL:
					rtpdump_header(logger, "email     ", e);
					break;
				case RTCP_SDES_PHONE:
					rtpdump_header(logger, "phone     ", e);
					break;
				case RTCP_SDES_LOC:
					rtpdump_header(logger, "loc       ", e);
					break;
				case RTCP_SDES_TOOL:
					rtpdump_header(logger, "tool      ", e);
					break;
				case RTCP_SDES_NOTE:
					rtpdump_header(logger, "note      ", e);
					break;
				default:
					rtpdump_header(logger, "???sdes???", e);
					break;
			}
			fprintf(logger, "\"");
			for (i = 0; i < sdes->length; i++) {
				fprintf(logger, "%c", sdes->data[i]);
			}
			fprintf(logger, "\"\n");
			break;
		case RX_BYE:
			rtpdump_header(logger, "bye       ", e);
			fprintf(logger, "\n");
			break;
		case SOURCE_CREATED:
			rtpdump_header(logger, "created   ", e);
			fprintf(logger, "\n");
			break;
		case SOURCE_DELETED:
			rtpdump_header(logger, "deleted   ", e);
			fprintf(logger, "\n");
			break;
	        case RX_APP:
			rtpdump_header(logger, "app       ", e);
			fprintf(logger, "-name \"%c%c%c%c\" -subtype %d -data_len %d\n",
			       app->name[0],
			       app->name[1],
			       app->name[2],
			       app->name[3],
			       app->subtype,
			       app->length);
			break;
		default:
			rtpdump_header(logger, "??????????", e);
			fprintf(logger, "event_type=%d\n", e->type);
			break;
	}
}

