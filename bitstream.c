/*
 * FILE:    bitstream.c
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: bitstream.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "memory.h"
#include "bitstream.h"

typedef struct s_bitstream {
        u_char *buf;    /* head of bitstream            */
        u_char *pos;    /* current byte in bitstream    */
        unsigned int   remain; /* bits remaining               */
        unsigned int   len;    /* length of bitstream in bytes */
} bs;

int
bs_create(bitstream_t **ppb)
{
        bs *pb;
        pb = (bs*)xmalloc(sizeof(bs));
        if (pb) {
                memset(pb, 0, sizeof(bs));
                *ppb = pb;
                return TRUE;
        }
        return FALSE;
}

int
bs_destroy(bitstream_t **ppb)
{
        xfree(*ppb);
        return TRUE;
}

int
bs_attach(bitstream_t *b,
          u_char *buf,
          int blen)
{
        b->buf    = b->pos = buf;
        b->remain = 8;
        b->len    = blen;
        return TRUE;
}

int
bs_put(bitstream_t *b,
       u_char       bits,
       uint8_t       nbits)
{
        assert(nbits != 0 && nbits <= 8);

        if (b->remain == 0) {
                b->pos++;
                b->remain = 8;
        }

        if (nbits > b->remain) {
                unsigned int over = nbits - b->remain;
                (*b->pos) |= (bits >> over);
                b->pos++;
                b->remain = 8 - over;
                (*b->pos)  = (bits << b->remain);
        } else {
                (*b->pos) |= bits << (b->remain - nbits);
                b->remain -= nbits;
        }

        assert((unsigned int)(b->pos - b->buf) <= b->len);
        return TRUE;
}

u_char
bs_get(bitstream_t *b,
       uint8_t  nbits)
{
        u_char out;

        if (b->remain == 0) {
                b->pos++;
                b->remain = 8;
        }

        if (nbits > b->remain) {
                /* Get high bits */
                out = *b->pos;
                out <<= (8 - b->remain);
                out >>= (8 - nbits);
                b->pos++;
                b->remain += 8 - nbits;
                out |= (*b->pos) >> b->remain;
        } else {
                out = *b->pos;
                out <<= (8 - b->remain);
                out >>= (8 - nbits);
                b->remain -= nbits;
        }

        assert((unsigned int)(b->pos - b->buf) <= b->len);
        return out;
}

int
bs_bytes_used(bitstream_t *b)
{
        unsigned int used = (unsigned int)(b->pos - b->buf);
        if (b->remain != 8) {
                used++;
        }
        return used;
}


