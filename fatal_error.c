/*
 * FILE:    fatal_error.c
 *
 * Copyright (c) 2000-2001 University College London
 * All rights reserved.
 *
 * $Id: fatal_error.c 3477 2001-01-08 20:30:13Z ucaccsp $
 */

#include <config_unix.h>
#include <config_win32.h>

#include "fatal_error.h"

void
fatal_error(const char *appname, const char *msg)
{
#ifdef WIN32
        MessageBox(NULL, msg, appname, MB_ICONERROR | MB_OK);
#else
        fprintf(stderr, "%s: %s\n", appname, msg);
#endif
}
