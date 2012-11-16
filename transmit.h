/*
 * FILE:    transmit.h
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson / Isidor Kouvelas
 *
 * Copyright (c) 1995-2001 University College London
 * All rights reserved.
 *
 * $Id: transmit.h 3642 2004-01-12 17:14:56Z ucacoxh $
 */

#ifndef _transmit_h_
#define _transmit_h_

#include "session.h"

struct s_tx_buffer;
struct s_session;
struct s_speaker_table;
struct s_minibuf;

int   tx_create      (struct s_tx_buffer **tb,
                      struct s_session    *sp,
		      uint32_t             sample_rate,
                      uint16_t             channels,
                      uint16_t             unit_size);

void  tx_destroy     (struct s_tx_buffer **tb);
void  tx_start       (struct s_tx_buffer  *tb);
void  tx_stop        (struct s_tx_buffer  *tb);

int   tx_is_sending    (struct s_tx_buffer *tb);
int   tx_read_audio    (struct s_tx_buffer *tb);
int   tx_process_audio (struct s_tx_buffer *tb);
void  tx_send          (struct s_tx_buffer *tb);
void  tx_update_ui     (struct s_tx_buffer *tb);
void  tx_igain_update  (struct s_tx_buffer *tb);

double tx_get_bps      (struct s_tx_buffer *tb);

uint32_t tx_get_rtp_time(struct s_session *sp);

#endif /* _transmit_h_ */
