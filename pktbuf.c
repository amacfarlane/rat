/*
 * FILE:      pktbuf.c
 * AUTHOR(S): Orion Hodson
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: pktbuf.c 3671 2004-11-25 17:25:40Z ucaccsp $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "memory.h"
#include "rtp.h"

#include "playout.h"
#include "ts.h"

#include "pktbuf.h"

static void pktbuf_free_rtp(u_char **mem, uint32_t memsz);

struct s_pktbuf {
	struct s_pb	     *rtp_buffer;
	struct s_pb_iterator *rtp_iterator;
	ts_sequencer	      rtp_sequencer;
        uint16_t	      max_packets;
};

int
pktbuf_create(struct s_pktbuf **ppb, uint16_t size)
{
        struct s_pktbuf *pb;

        pb = (struct s_pktbuf*)xmalloc(sizeof(struct s_pktbuf));
        if (pb == NULL) {
                return FALSE;
        }

	if (pb_create(&pb->rtp_buffer, pktbuf_free_rtp) == FALSE) {
		xfree(pb);
		return FALSE;
	}

	if (pb_iterator_create(pb->rtp_buffer, &pb->rtp_iterator) == FALSE) {
		pb_destroy(&pb->rtp_buffer);
		xfree(pb);
		return FALSE;
	}

	pb->max_packets = size;
        *ppb = pb;
        return TRUE;
}

void
pktbuf_destroy(struct s_pktbuf **ppb)
{
        struct s_pktbuf *pb = *ppb;
	pb_iterator_destroy(pb->rtp_buffer, &pb->rtp_iterator);
	pb_destroy(&pb->rtp_buffer);
        xfree(pb);
        *ppb = NULL;
}

static void
pktbuf_free_rtp(u_char **data, uint32_t data_bytes) {
	assert(data_bytes == sizeof(rtp_packet));
	xfree(*data);
	*data = NULL;
}

int
pktbuf_enqueue(struct s_pktbuf *pb, rtp_packet *p)
{
	timestamp_t playout;
	uint32_t    psize;

        assert(p != NULL);
        playout = ts_seq32_in(&pb->rtp_sequencer, 8000 /* Arbitrary */, p->fields.ts);
	psize   = sizeof(rtp_packet);

	if (!pb_add(pb->rtp_buffer, (u_char*)p, psize, playout)) {
                u_char* pp = (u_char*)p;
		pktbuf_free_rtp(&pp, psize);
		return TRUE;
	}

	if (pb_node_count(pb->rtp_buffer) > pb->max_packets) {
		debug_msg("RTP packet queue overflow\n");
		if (pktbuf_dequeue(pb, &p)) {
                        u_char* pp = (u_char*)p;
			pktbuf_free_rtp(&pp, psize);
			return TRUE;
		}
		/* NOTREACHED */
		debug_msg("Failed to detach overflow packet\n");
		abort();
	}

        return TRUE;
}

int
pktbuf_dequeue(struct s_pktbuf *pb, rtp_packet **pp)
{
	timestamp_t playout;
	uint32_t    psize;

	if (pb_iterator_rwd(pb->rtp_iterator) == FALSE) {
		return FALSE;
	}

	pb_iterator_detach_at(pb->rtp_iterator, (u_char**)pp, &psize, &playout);
	return TRUE;
}

int
pktbuf_peak_last(pktbuf_t   *pb,
                 rtp_packet **pp)
{
	timestamp_t playout;
	uint32_t    psize;

	if (pb_iterator_ffwd(pb->rtp_iterator) == FALSE) {
		return FALSE;
	}
	pb_iterator_get_at(pb->rtp_iterator, (u_char**)pp, &psize, &playout);
	return TRUE;
}

uint16_t
pktbuf_get_count(pktbuf_t *pb)
{
        return (uint16_t)pb_node_count(pb->rtp_buffer);
}

