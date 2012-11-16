/*
 * FILE:    codec_compat.c
 * AUTHORS: Orion Hodson
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: codec_compat.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include <config_unix.h>
#include <config_win32.h>

#include "codec_compat.h"

/* Backward compatibility name mapping for earlier MBONE audio applications */

struct s_compat_entry {
        const char *newname;
        const char *oldname;
};

static struct s_compat_entry name_map[] = {
        {"PCMU-8K-MONO", "pcm"},
        {"PCMU-8K-MONO", "pcmu"},
        {"PCMU-8K-MONO", "ulaw"},
        {"PCMA-8K-MONO", "pcma"},
        {"PCMU-8K-MONO", "alaw"},
        {"DVI-8K-MONO",  "dvi"},
        {"GSM-8K-MONO",  "gsm"},
        {"LPC-8K-MONO",  "lpc"},
        {"L16-8K-MONO",  "l16"}
};

#define NUM_COMPAT_NAMES sizeof(name_map)/sizeof(name_map[0])

const char *
codec_get_compatible_name(const char *name)
{
        uint16_t i;
        if (name != NULL) {
                for (i = 0; i < NUM_COMPAT_NAMES; i++) {
                        if (!strcasecmp(name, name_map[i].oldname)) {
                                return name_map[i].newname;
                        }
                }
        }
        return name;
}
