/*
 * FILE:      source.h
 * AUTHOR(S): Orion Hodson
 *
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 *
 * $Id: source.h 3642 2004-01-12 17:14:56Z ucacoxh $
 */

#ifndef __SOURCE_H__
#define __SOURCE_H__

#include "converter.h"
#include "ts.h"
#include "rtp.h"

struct s_source;
struct s_source_list;
struct s_rtcp_dbentry;
struct s_mixer;
struct s_pb;
struct s_session;

int              source_list_create  (struct s_source_list **pplist);

void             source_list_destroy (struct s_source_list **pplist);

void             source_list_clear   (struct s_source_list *plist);

uint32_t         source_list_source_count(struct s_source_list *plist);

struct s_source* source_list_get_source_no (struct s_source_list *plist,
                                            uint32_t              src_no);

struct s_source* source_get_by_ssrc (struct s_source_list  *list,
                                     uint32_t               ssrc);

struct s_source* source_create             (struct s_source_list  *list,
                                            uint32_t               ssrc,
					    pdb_t		  *pdb);

void             source_remove             (struct s_source_list *list,
                                            struct s_source      *src);

int              source_add_packet         (struct s_source *src,
                                            rtp_packet      *p);

int              source_check_buffering    (struct s_source   *src);

void             source_process            (struct s_session  *sp,
                                            struct s_source   *src,
                                            timestamp_t               start_ts,
                                            timestamp_t               end_ts);

int              source_relevant           (struct s_source *src,
                                            timestamp_t             now);

int              source_audit              (struct s_source *src);

timestamp_t             source_get_audio_buffered (struct s_source *src);

timestamp_t             source_get_playout_delay  (struct s_source *src);

double           source_get_bps            (struct s_source *src);
double           source_get_skew_rate      (struct s_source *src);

struct s_pb*
                 source_get_decoded_buffer (struct s_source *src);

uint32_t         source_get_ssrc           (struct s_source *src);

#endif /* __SOURCE_H__ */
