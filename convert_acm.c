/*
 * FILE:    convert.c
 * PROGRAM: RAT
 * AUTHOR:  O.Hodson <O.Hodson@cs.ucl.ac.uk>
 *
 * Copyright (c) 1995-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: convert_acm.c 3962 2007-02-09 16:38:56Z piers $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "assert.h"
#include "audio_types.h"
#include "converter_types.h"
#include "convert_acm.h"
#include "util.h"
#include "memory.h"
#include "debug.h"

#ifdef WIN32

#include <mmreg.h> 
#include <msacm.h>

/* WINDOWS ACM CONVERSION CODE **********************************************/

static HACMDRIVER hDrv;

static BOOL CALLBACK
getPCMConverter (HACMDRIVERID hadid, DWORD dwInstance, DWORD fdwSupport)
{
        if (fdwSupport & ACMDRIVERDETAILS_SUPPORTF_CONVERTER) {
                ACMDRIVERDETAILS add;
                add.cbStruct = sizeof(ACMDRIVERDETAILS);
                if (acmDriverDetails(hadid, &add, 0)
                    || strcmp(add.szShortName,"MS-PCM")
                    || acmDriverOpen(&hDrv, hadid, 0)) return TRUE;
                return FALSE;
        }
        return TRUE;
}

int
acm_cv_startup (void)
{
     acmDriverEnum(getPCMConverter, 0L, 0L);
     if (hDrv) return TRUE;
     return FALSE;                /* Failed initialization, entry disabled in table */
}

void
acm_cv_shutdown (void)
{
        if (hDrv) acmDriverClose(hDrv, 0);
        hDrv = 0;
}

static void
acm_conv_init_fmt (WAVEFORMATEX *pwfx, uint16_t nChannels, uint16_t nSamplesPerSec)
{
       pwfx->wFormatTag      = WAVE_FORMAT_PCM;
       pwfx->nChannels       = nChannels;
       pwfx->nSamplesPerSec  = nSamplesPerSec;
       pwfx->nAvgBytesPerSec = nSamplesPerSec * nChannels * sizeof(sample);
       pwfx->nBlockAlign     = nChannels * sizeof(sample);
       pwfx->wBitsPerSample  = 8 * sizeof(sample);
}

int
acm_cv_create (const converter_fmt_t *cfmt, u_char **state, uint32_t *state_len)
{
        LPHACMSTREAM lpa;
        WAVEFORMATEX wfxSrc, wfxDst;

        lpa        = (LPHACMSTREAM)xmalloc(sizeof(HACMSTREAM));

        acm_conv_init_fmt(&wfxSrc, cfmt->src_channels, cfmt->src_freq);
        acm_conv_init_fmt(&wfxDst, cfmt->dst_channels,   cfmt->dst_freq);

        if (acmStreamOpen(lpa, hDrv, &wfxSrc, &wfxDst, NULL, 0L, 0L, 0L)) {
                xfree(lpa);
                return FALSE;
        }

        *state     = (u_char *)lpa;
        *state_len = sizeof(HACMSTREAM);

        return TRUE;
}

void
acm_cv_convert (const converter_fmt_t *cfmt, u_char *state, sample *src_buf, int src_len, sample *dst_buf, int dst_len)
{
        ACMSTREAMHEADER ash;
        LPHACMSTREAM    lphs;

        UNUSED(cfmt);

        memset(&ash, 0, sizeof(ash));
        ash.cbStruct        = sizeof(ash);
        ash.pbSrc           = (LPBYTE)src_buf;
        ash.cbSrcLength     = src_len * sizeof(sample);
        ash.pbDst           = (LPBYTE)dst_buf;
        ash.cbDstLength     = dst_len * sizeof(sample);

        lphs = (LPHACMSTREAM)state;

        if (acmStreamPrepareHeader(*lphs, &ash, 0) ||
            acmStreamConvert(*lphs, &ash, ACM_STREAMCONVERTF_BLOCKALIGN)) {
                memset(dst_buf, 0, dst_len * sizeof(sample));
        }
        return;
}

void
acm_cv_destroy (u_char **state, uint32_t *state_len)
{
        assert(*state_len == sizeof(HACMSTREAM));

        if (*state) {
                xfree(*state);
                state      = NULL;
                *state_len = 0;
        }
}

#endif /* WIN32 */
