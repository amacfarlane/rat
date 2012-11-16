/*
 * FILE:      channel_types.c
 * AUTHOR(S): Orion Hodson
 *
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: channel_types.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */
#include "config_unix.h"
#include "config_win32.h"

#include "channel_types.h"

#include "util.h"
#include "debug.h"

int
channel_data_create(channel_data **ppcd, int nelem)
{
        channel_data *pcd;
        int i;

        *ppcd = NULL;
        pcd = (channel_data*)block_alloc(sizeof(channel_data));

        if (pcd) {
                memset(pcd, 0, sizeof(channel_data));
                for(i = 0; i < nelem; i++) {
                        pcd->elem[i] = (channel_unit*)block_alloc(sizeof(channel_unit));
                        if (pcd->elem[i] == NULL) {
                                pcd->nelem = i;
                                channel_data_destroy(&pcd, sizeof(channel_data));
                                return FALSE;
                        }
                        memset(pcd->elem[i], 0, sizeof(channel_unit));
                }
                pcd->nelem   = nelem;
                *ppcd = pcd;
                return TRUE;
        }
        return FALSE;
}

void
channel_data_destroy(channel_data **ppcd, uint32_t cd_size)
{
        channel_data *pcd;
        channel_unit *pcu;
        int i;

        pcd = *ppcd;
        assert(pcd != NULL);
        assert(cd_size == sizeof(channel_data));

        for(i = 0; i < pcd->nelem; i++) {
                pcu = pcd->elem[i];
                if (pcu->data) {
                        block_free(pcu->data, pcu->data_len);
                        pcu->data_len = 0;
                }
                assert(pcu->data_len == 0);
                block_free(pcu, sizeof(channel_unit));
                pcd->elem[i] = NULL;
        }

#ifdef DEBUG
        while (i < MAX_CHANNEL_UNITS) {
                assert(pcd->elem[i] == NULL);
                i++;
        }
#endif

        block_free(pcd, sizeof(channel_data));
        *ppcd = NULL;
}

uint32_t
channel_data_bytes(channel_data *cd)
{
        uint32_t len, i;

        len = 0;
        for(i = 0; i < cd->nelem; i++) {
                len += cd->elem[i]->data_len;
        }
        return len;
}

