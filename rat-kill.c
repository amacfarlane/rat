/*
 * Copyright (c) 2000-2002 University of Chicago. All Rights Reserved.
 *
 * This file is part of the Access Grid Toolkit.
 * 
 * The Access Grid Toolkit is free software. You can redistribute
 * it and/or modify it under the terms of the Access Grid Toolkit
 * Public License. 
 * 
 * You should have received a copy of the Access Grid Toolkit Public
 * License along with this program; if not write to the Futures Laboratory 
 * at Argonne National Laboratory at fl-info@mcs.anl.gov or download a 
 * copy from http://www.accessgrid.org/release_docs/1.0/LICENSE.TXT
 *
 */
#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"

#include <stdio.h>
#ifdef WIN32
#include <windows.h>
#endif
#include <mbus.h>

static void cmd_handler(char *src, char *cmd, char *arg, void *dat)
{
	UNUSED(src);
	UNUSED(cmd);
	UNUSED(arg);
	UNUSED(dat);
}

static void err_handler(int seqnum, int reason)
{
	UNUSED(seqnum);
	UNUSED(reason);
}

#ifdef WIN32
#define WS_VERSION_ONE MAKEWORD(1,1)
#define WS_VERSION_TWO MAKEWORD(2,2)
#endif

int main()
{
    struct mbus *m;

    #ifdef WIN32

    WSADATA WSAdata;
    if (WSAStartup(WS_VERSION_TWO, &WSAdata) != 0 &&
        WSAStartup(WS_VERSION_ONE, &WSAdata) != 0) {
        printf("Windows Socket initialization failed. TCP/IP stack\nis not installed or i\
s damaged.");
        exit(-1);
    }

    #endif

    m = mbus_init(cmd_handler, err_handler, "(app:ag)");

    mbus_qmsg(m,
	      "(app:rat)",
	      "mbus.quit",
	      "",
	      1);
    mbus_send(m);
    mbus_exit(m);
    return 0;
}
