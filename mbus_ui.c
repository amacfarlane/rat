/*
 * FILE:    mbus_ui.c
 * AUTHORS: Colin Perkins
 *
 * Copyright (c) 1998-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: mbus_ui.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "mbus.h"
#include "mbus_parser.h"
#include "mbus_ui.h"
#include "tcltk.h"

extern char 	*e_addr, *c_addr;
extern int	 ui_active;
extern int	 should_exit;
extern int	 got_detach;
extern int	 got_quit;

/* Mbus command reception function type */
typedef void (*mbus_rx_proc)(char *srce, char *args);

/* Tuple to associate string received with it's parsing fn */
typedef struct {
        const char   *rxname;
        mbus_rx_proc  rxproc;
} mbus_cmd_tuple;

static void rx_tool_rat_ui_detach(char *srce, char *args)
{
	UNUSED(srce);
	UNUSED(args);
	assert(should_exit == TRUE);
	got_detach = TRUE;
}

static void rx_mbus_hello(char *srce, char *args)
{
	UNUSED(srce);
	UNUSED(args);
}

static void rx_mbus_waiting(char *srce, char *args)
{
	char			*s;
	struct mbus_parser	*mp;

	UNUSED(srce);

	mp = mbus_parse_init(args);
	if (mbus_parse_str(mp, &s)) {
		if (strcmp("rat-ui-requested", mbus_decode_str(s)) == 0 && e_addr == NULL) {
			/* FIXME: This assumes id:12345 is the last element in the mbus address */
                        char *c_id, *e_id;
                        /* srce is a candidate engine address */
                        c_id = strstr(c_addr, "id:");
                        e_id = strstr(srce, "id:");
                        /* If pid's match up this is our engine */
                        if (strcmp(c_id, e_id) == 0) {
                                e_addr = xstrdup(srce);
                        }
		}
	} else {
		debug_msg("mbus: usage \"mbus.waiting (token)\"\n");
	}
	mbus_parse_done(mp);
}

static void rx_mbus_quit(char *srce, char *args)
{
	UNUSED(args);
	should_exit = TRUE;
	got_quit    = TRUE;
	debug_msg("Got mbus.quit() from %s\n", srce);
}

static void rx_mbus_bye(char *srce, char *args)
{
	UNUSED(args);
	UNUSED(srce);
}

static const mbus_cmd_tuple ui_cmds[] = {
	{ "tool.rat.ui.detach",		rx_tool_rat_ui_detach },
        { "mbus.hello",			rx_mbus_hello },
        { "mbus.waiting",		rx_mbus_waiting },
        { "mbus.quit",			rx_mbus_quit },
	{ "mbus.bye",			rx_mbus_bye }
};

#define NUM_UI_CMDS sizeof(ui_cmds)/sizeof(ui_cmds[0])

void mbus_ui_rx(char *srce, char *cmnd, char *args, void *data)
{
	char        	 command[1500];
	unsigned int 	 i;

	UNUSED(data);

	/* Some commands are handled in C for now... */
	for (i=0; i < NUM_UI_CMDS; i++) {
		if (strcmp(ui_cmds[i].rxname, cmnd) == 0) {
                        ui_cmds[i].rxproc(srce, args);
			return;
		}
	}
	/* ...and some are in Tcl... */
	if (ui_active) {
		/* Pass it to the Tcl code to deal with... */
		sprintf(command, "mbus_recv %s %s", cmnd, args);
		for (i = 0; i < (unsigned)strlen(command); i++) {
			if (command[i] == '[') command[i] = '(';
			if (command[i] == ']') command[i] = ')';
		}
		tcl_send(command);
	} else {
		debug_msg("Got early mbus command %s (%s)\n", cmnd, args);
	}
}

