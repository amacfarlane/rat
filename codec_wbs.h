/*
 * FILE:    codec_wbs.h
 * AUTHORS: Orion Hodson
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 *
 * $Id: codec_wbs.h 3477 2001-01-08 20:30:13Z ucaccsp $
 */

#ifndef _CODEC_WBS_H_
#define _CODEC_WBS_H_

uint16_t               wbs_get_formats_count (void);
const codec_format_t* wbs_get_format        (uint16_t idx);
int                   wbs_state_create      (uint16_t idx, u_char **state);
void                  wbs_state_destroy     (uint16_t idx, u_char **state);
int                   wbs_encoder           (uint16_t idx, u_char *state, sample     *in, coded_unit *out);
int                   wbs_decoder           (uint16_t idx, u_char *state, coded_unit *in, sample     *out);
uint8_t	              wbs_max_layers        (void);
int                   wbs_get_layer         (uint16_t idx, coded_unit *in, uint8_t layer, uint16_t *markers, coded_unit *out);
int                   wbs_combine_layer     (uint16_t idx, coded_unit *in, coded_unit *out, uint8_t nelem, uint16_t *markers);

#endif /* _CODEC_WBS_H_ */



