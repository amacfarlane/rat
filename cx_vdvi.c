/*
 * FILE:    vdvi.h
 * PROGRAM: RAT
 * AUTHOR:  Orion Hodson
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: cx_vdvi.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "bitstream.h"
#include "cx_vdvi.h"


#ifdef TEST_VDVI
#define NUM_TESTS 100000

static  u_char src[80], pad1[4],
        dst[80], pad2[4],
        coded[160], pad3[4], safe[80];

void
check_padding()
{
        assert(pad1[0] == 0xff && pad2[0] == 0xff && pad3[0] == 0xff);
        assert(pad1[1] == 0xff && pad2[1] == 0xff && pad3[1] == 0xff);
        assert(pad1[2] == 0xff && pad2[2] == 0xff && pad3[2] == 0xff);
        assert(pad1[3] == 0xff && pad2[3] == 0xff && pad3[3] == 0xff);
}

int main()
{
        int i, n, coded_len, out_len, a, amp;

        memset(pad1, 0xff, 4); /* Memory overwrite test */
        memset(pad2, 0xff, 4);
        memset(pad3, 0xff, 4);

        srandom(123213);

        for(n = 0; n < NUM_TESTS; n++) {
                amp = (random() &0x0f);
                for(i = 0; i< 80; i++) {
                        a = (int)(amp * sin(M_PI * 2.0 * (float)i/16.0));
                        assert(abs(a) < 16);
                        src[i] = (a << 4) & 0xf0;
                        a = amp;
                        assert(abs(a) < 16);
                        src[i] |= (a & 0x0f);
                }

                memcpy(safe, src, 80);

                coded_len = vdvi_encode(src, 160, coded, 160);

                assert(!memcmp(src,safe,80));

                check_padding();
                out_len   = vdvi_decode(coded, 160, dst, 160);

                assert(!memcmp(src,safe,80));
                assert(!memcmp(dst,safe,80)); /* dst matches sources */

                assert(coded_len == out_len);

                check_padding();

                for(i = 0; i< 80; i++) {
                        assert(src[i] == dst[i]);
                }
                if (0 == (n % 1000)) {
                        printf(".");
                        fflush(stdout);
                }
        }
        printf("\nTested %d frames\n", n);
        return 1;
}
#endif /* TEST_DVI */

/* VDVI translations as defined in draft-ietf-avt-profile-new-00.txt

DVI4  VDVI        VDVI  VDVI
c/w   c/w         hex   rel. bits
_________________________________
  0          00    00    2
  1         010    02    3
  2        1100    0c    4
  3       11100    1c    5
  4      111100    3c    6
  5     1111100    7c    7
  6    11111100    fc    8
  7    11111110    fe    8
  8          10    02    2
  9         011    03    3
  10       1101    0d    4
  11      11101    1d    5
  12     111101    3d    6
  13    1111101    7d    7
  14   11111101    fd    8
  15   11111111    ff    8
*/

static unsigned int dmap[16]   = { 0x00, 0x02, 0x0c, 0x1c,
                            0x3c, 0x7c, 0xfc, 0xfe,
                            0x02, 0x03, 0x0d, 0x1d,
                            0x3d, 0x7d, 0xfd, 0xff
};

static uint8_t dmap_bits[16] = {   2,    3,    4,    5,
                                  6,    7,    8,    8,
                                  2,    3,    4,    5,
                                  6,    7,    8,    8
};

int
vdvi_encode(u_char *dvi_buf, unsigned int dvi_samples, bitstream_t *bs)
{
        register u_char s1, s2;
        u_char *dvi_end, *dp, t;
        int bytes_used;

        assert(dvi_samples == VDVI_SAMPLES_PER_FRAME);

        /* Worst case is 8 bits per sample -> VDVI_SAMPLES_PER_FRAME */

        dvi_end = dvi_buf + dvi_samples / 2;
        dp      = dvi_buf;
        while (dp != dvi_end) {
                t = *dp;
                s1 = (t & 0xf0) >> 4;
                s2 = (t & 0x0f);
                bs_put(bs, (u_char)dmap[s1], dmap_bits[s1]);
                bs_put(bs, (u_char)dmap[s2], dmap_bits[s2]);
                assert(*dp == t);
                dp ++;
        }
        /* Return number of bytes used */
        bytes_used  = bs_bytes_used(bs);
        return bytes_used;
}

int /* Returns number of bytes in in_bytes used to generate dvi_samples */
vdvi_decode(bitstream_t *bs, unsigned char *dvi_buf, unsigned int dvi_samples)
{
        u_char cw, cb;
        unsigned int i, j, bytes_used;

        /* This code is ripe for optimization ... */
        assert(dvi_samples == VDVI_SAMPLES_PER_FRAME);

        for(j = 0; j < dvi_samples; j++) {
#ifdef TEST_DVI
                check_padding();
#endif
                cb = 2;
                cw = bs_get(bs, 2);
                do {
                        for(i = 0; i < 16; i++) {
                                if (dmap_bits[i] != cb) continue;
                                if (dmap[i] == cw) goto dvi_out_pack;
                        }
                        cb++;
                        cw <<=1;
                        cw |= bs_get(bs, 1);
                        assert(cb <= 8);
#ifdef TEST_DVI
                check_padding();
#endif
                } while(1);
        dvi_out_pack:
#ifdef TEST_DVI
                check_padding();
#endif
                if (j & 0x01) {
                        dvi_buf[j/2] |= i;
                } else {
                        dvi_buf[j/2]  = i << 4;
                }
#ifdef TEST_DVI
                check_padding();
#endif
        }

        bytes_used = bs_bytes_used(bs);

        assert(bytes_used <= dvi_samples);
        return bytes_used;
}
