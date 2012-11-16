/*
 * FILE:      pktbuf.h
 * AUTHOR(S): Orion Hodson
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 *
 * $Id: pktbuf.h 3642 2004-01-12 17:14:56Z ucacoxh $
 */

/* This provides a finite length buffer for queueing packets.  It is   */
/* necessary since it process blocks it will find lots of packets when */
/* it gets around to doing a read.  However, most of these packets     */
/* won't be any use, discard them to conserve processing power.        */

/* Assumes data is allocated with xmalloc that is enqueued with        */
/* pktbuf_enqueue.  It's important since the pktbuf frees this data if */
/* the number of enqueued packets exceeds the maxpackets selected when */
/* the buffer was created.                                             */

#include "rtp.h"

#ifndef __PKTBUF_H__
#define __PKTBUF_H__

typedef struct s_pktbuf pktbuf_t;

int     pktbuf_create    (pktbuf_t **ppb,
                          uint16_t    maxpackets);

void    pktbuf_destroy   (pktbuf_t **ppb);

int     pktbuf_enqueue   (pktbuf_t *pb,
                          rtp_packet *p);
int     pktbuf_dequeue   (pktbuf_t *pb,
                          rtp_packet **p);

/* Peak at last packet sent by source.  May have bursting at start of       */
/* talkspurt because of pre-hang, or process may have blocked.  Useful      */
/* to check when determining offset between source time and local time.     */
int     pktbuf_peak_last (pktbuf_t *pb,
                          rtp_packet **p);
uint16_t pktbuf_get_count (pktbuf_t *pb);

#endif /* __PKTBUF_H__ */
