/*
 * FILE:    codec_compat.h
 * AUTHORS: Orion Hodson
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 *
 * $Id: codec_compat.h 3477 2001-01-08 20:30:13Z ucaccsp $
 */

/* Backward compatibility name mapping for earlier MBONE audio applications */
/* Returns compatible name if found, or cname passed to it if not.          */
const char *codec_get_compatible_name(const char *cname);
