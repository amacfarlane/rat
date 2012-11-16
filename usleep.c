/*
 * FILE:    usleep.c
 * PROGRAM: RAT
 * AUTHORS: Colin Perkins
 *
 * Some platforms, notably Win32 and Irix 5.3 don't have a usleep
 * system call. This file provides a workaround.
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: usleep.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"

#ifdef WIN32
int usleep(unsigned int usec)
{
        DWORD dur = usec/1000;
        if (dur != 0) {
		Sleep(dur);
	}
        return 0;
}
#endif

/* On Irix 6.5.4m this function isn't needed.... we should probably have */
/* a check in the configure script for usleep().                         */
#ifdef NEED_USLEEP
int usleep(unsigned int usec)
{
	struct timespec sleeptime,t2;

	sleeptime.tv_sec  = usec/1000000;
	sleeptime.tv_nsec = (usec%1000000)*1000;

	while(nanosleep(&sleeptime,&t2)){ /* if interrupted, sleep again*/
		sleeptime.tv_sec  = t2.tv_sec;
		sleeptime.tv_nsec = t2.tv_nsec;
	}
}
#endif

