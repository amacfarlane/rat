/*
 * FILE:    sndfile_types.h
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 *
 * $Id: sndfile_types.h 3642 2004-01-12 17:14:56Z ucacoxh $
 */

#ifndef __SNDFILE_TYPES_H__
#define __SNDFILE_TYPES_H__

typedef enum {
        SNDFILE_ENCODING_PCMU,
        SNDFILE_ENCODING_PCMA,
        SNDFILE_ENCODING_L16,
        SNDFILE_ENCODING_L8
} sndfile_encoding_e;

typedef struct {
        sndfile_encoding_e encoding;
        uint32_t sample_rate;
        uint32_t channels;
} sndfile_fmt_t;

#endif /* __SNDFILE_TYPES_H__ */
