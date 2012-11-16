/*
 * FILE:    mbus_control.c
 * PROGRAM: RAT - controller
 * AUTHOR:  Colin Perkins
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: mbus_control.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "mbus.h"
#include "mbus_parser.h"
#include "mbus_control.h"

extern int 	 should_exit;
extern char	*u_addr, *e_addr;
extern pid_t	 pid_ui, pid_engine;

/* Mbus command reception function type */
typedef void (*mbus_rx_proc)(char *srce, char *args, void *data);

/* Tuple to associate string received with it's parsing fn */
typedef struct {
        const char   *rxname;
        mbus_rx_proc  rxproc;
} mbus_cmd_tuple;

static char *wait_token;
static char *wait_addr;

void mbus_control_wait_init(char *token)
{
	wait_token = token;
	wait_addr  = NULL;
}

char *mbus_control_wait_done(void)
{
	return wait_addr;
}

static void rx_mbus_quit(char *srce, char *args, void *data)
{
	UNUSED(srce);
	UNUSED(args);
	UNUSED(data);

        debug_msg("Got quit %s\n", srce);

	/* We mark ourselves as ready to exit. The main() will */
	/* cleanup and terminate all our subprocesses.         */
	should_exit = TRUE;
}

static void rx_mbus_bye(char *srce, char *args, void *data)
{
        pid_t pid_msgsrc, pid_cur;
        char *lc;
        int   i;

        /* Find last colon */
        for (i = 0, lc = NULL; srce[i] != 0; i++) {
                if (srce[i] == ':') {
                        lc = srce + i;
                }
        }
        assert(lc != NULL);
        /* Skip past colon, next char should okay for atoi */
        pid_msgsrc = atoi(lc + 1);
        pid_cur    = (pid_t)getpid();
        if (pid_msgsrc == pid_cur ||
            pid_msgsrc == pid_ui   ||
            pid_msgsrc == pid_engine) {
                /* We mark ourselves as ready to exit. The main() will */
                /* cleanup and terminate all our subprocesses.         */
                should_exit = TRUE;
        }
	UNUSED(args);
	UNUSED(data);
}

static void rx_mbus_waiting(char *srce, char *args, void *data)
{
	UNUSED(srce);
	UNUSED(args);
	UNUSED(data);
}

static void rx_mbus_go(char *srce, char *args, void *data)
{
	struct mbus_parser	*mp;
	char			*t;

	UNUSED(data);

	mp = mbus_parse_init(args);
	mbus_parse_str(mp, &t);
	if (strcmp(mbus_decode_str(t), wait_token) == 0) {
		wait_addr = xstrdup(srce);
	}
	mbus_parse_done(mp);
}

static void rx_mbus_hello(char *srce, char *args, void *data)
{
	UNUSED(srce);
	UNUSED(args);
	UNUSED(data);
}

static const mbus_cmd_tuple control_cmds[] = {
        { "mbus.quit",                             rx_mbus_quit },
        { "mbus.bye",                              rx_mbus_bye },
        { "mbus.waiting",                          rx_mbus_waiting },
        { "mbus.go",                               rx_mbus_go },
        { "mbus.hello",                            rx_mbus_hello },
};

#define NUM_CONTROL_CMDS sizeof(control_cmds)/sizeof(control_cmds[0])

void mbus_control_rx(char *srce, char *cmnd, char *args, void *data)
{
	uint32_t i;

	for (i=0; i < NUM_CONTROL_CMDS; i++) {
		if (strcmp(control_cmds[i].rxname, cmnd) == 0) {
                        control_cmds[i].rxproc(srce, args, data);
			return;
		}
	}
	debug_msg("Unknown mbus command: %s (%s)\n", cmnd, args);
}

