/*
 * FILE:    convert_types.h
 * PROGRAM: RAT
 * AUTHOR:  O.Hodson
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 *
 * $Id: converter_types.h 3477 2001-01-08 20:30:13Z ucaccsp $
 */

#ifndef __CONVERT_TYPES_H__
#define __CONVERT_TYPES_H__

struct s_coded_unit;

typedef struct s_converter_fmt {
        uint16_t src_channels;
        uint16_t src_freq;
        uint16_t dst_channels;
        uint16_t dst_freq;
} converter_fmt_t;

typedef uint32_t converter_id_t;

typedef struct {
        converter_id_t id;
        const char*    name;
} converter_details_t;

struct  s_converter;

#endif /* __CONVERT_TYPES_H__ */
