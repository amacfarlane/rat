/*
 * FILE:      channel_types.h
 * AUTHOR(S): Orion Hodson
 *
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 *
 * $Id: channel_types.h 3642 2004-01-12 17:14:56Z ucacoxh $
 */
#ifndef __CHANNEL_TYPES_H__
#define __CHANNEL_TYPES_H__

/* Channel coder description information */

typedef uint32_t cc_id_t;

#define CC_NAME_LENGTH 32

typedef struct {
        cc_id_t    descriptor;
        const char name[CC_NAME_LENGTH];
} cc_details_t;

/* In and out unit types.  On input channel encoder takes a playout buffer
 * of media_units and puts channel_units on the output playout buffer
 */

#define MAX_CHANNEL_UNITS    20
#define MAX_UNITS_PER_PACKET 8

typedef struct {
        uint8_t  pt;
        u_char *data;
        uint32_t data_len;   /* This is the length for processing purposes */
} channel_unit;

typedef struct {
        uint8_t        nelem;
        channel_unit *elem[MAX_CHANNEL_UNITS];
} channel_data;

int  channel_data_create  (channel_data **cd,
                           int            nelem);

void channel_data_destroy (channel_data **cd,
                           uint32_t        cdsize);

uint32_t channel_data_bytes(channel_data *cd);

#endif /* __CHANNEL_TYPES_H__ */
