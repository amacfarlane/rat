/*
 * FILE:    codec_l16.h
 * AUTHORS: Orion Hodson
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 *
 * $Id: codec_l16.h 3477 2001-01-08 20:30:13Z ucaccsp $
 */

#ifndef _CODEC_L16_H_
#define _CODEC_L16_H_

uint16_t               l16_get_formats_count (void);
const codec_format_t* l16_get_format        (uint16_t idx);
int                   l16_encode            (uint16_t idx, u_char *state, sample     *in, coded_unit *out);
int                   l16_decode            (uint16_t idx, u_char *state, coded_unit *in, sample     *out);
int                   l16_peek_frame_size   (uint16_t idx, u_char *data,  int data_len);

#endif /* _CODEC_L16_H_ */
