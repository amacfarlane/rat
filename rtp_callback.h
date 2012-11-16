/*
 * FILE:    rtp_callback.h
 * PROGRAM: RAT
 * AUTHOR:  Colin Perkins / Orion Hodson
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 *
 * $Id: rtp_callback.h 3969 2007-03-02 19:18:26Z piers $
 */

#ifndef __RTP_CALLBACK_H__
#define __RTP_CALLBACK_H__

struct s_session;

void rtp_callback_init (struct rtp *s, struct s_session *sp);
void rtp_callback_proc (struct rtp *s, rtp_event *e);
void rtp_callback_exit (struct rtp *s);


rtcp_app* rtcp_app_site_callback(struct rtp *session,
				 uint32_t rtp_ts,
				 int max_size);

#endif /* __RTP_CALLBACK_H__ */
