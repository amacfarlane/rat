/*
 * FILE:    main_ui.c
 * PROGRAM: RAT
 * AUTHORS: Colin Perkins
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: main_ui.c 4550 2010-01-07 16:12:48Z piers $";
#endif /* HIDE_SOURCE_STRINGS */


#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "fatal_error.h"
#include "mbus.h"
#include "mbus_ui.h"
#include "mbus_parser.h"
#include "tcl.h"
#include "tk.h"
#include "tcltk.h"
#include "util.h"
#include "version.h"

char*       e_addr = NULL; /* Engine address */
char*       c_addr = NULL; /* Controller address */
char        m_addr[100];
char*       c_addr, *token, *token_e;
pid_t       ppid;

int ui_active   = FALSE;
int should_exit = FALSE;
int got_detach  = FALSE;
int got_quit    = FALSE;
static int mbus_shutdown_error = FALSE;


static void parse_args(int argc, char *argv[])
{
	int 	i;
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-ctrl") == 0) {
			c_addr = xstrdup(argv[++i]);
		} else if (strcmp(argv[i], "-token") == 0) {
			token   = xstrdup(argv[++i]);
			token_e = mbus_encode_str(token);
		} else if (strcmp(argv[i], "-X") == 0) {
			/* accept -X arguments, but only because they get passed to tcl */
			i++;
		} else {
			printf("Unknown argument to %s: \"%s\"\n", argv[0], argv[i]);
			printf("UI usage: %s -ctrl <addr> -token <token> [-X arg=value]\n", argv[0]);
			abort();
		}
	}
        /*
         * Want app instance to be same across all processes that
         * consitute this rat.  Parent pid appears after last colon.
         * Obviously on Un*x we could use getppid...
         */
        i = strlen(c_addr) - 1;
        while(i > 1 && c_addr[i - 1] != ':') {
                i--;
        }
        ppid = (pid_t)strtoul(&c_addr[i], NULL, 10);
}

#ifdef WIN32
extern HINSTANCE hAppInstance;
extern int       TkWinXInit(HINSTANCE);
extern void      TkWinXCleanup(HINSTANCE);
#endif

static void
mbus_error_handler(int seqnum, int reason)
{
        debug_msg("mbus message failed (%d:%d)\n", seqnum, reason);
        if (should_exit == FALSE) {
                abort();
        }
	mbus_shutdown_error = TRUE;
        UNUSED(seqnum);
        UNUSED(reason);
        /* Ignore error we're closing down anyway */
}

int main(int argc, char *argv[])
{
	struct mbus	*m;
	struct timeval	 timeout;

#ifdef WIN32
	HANDLE waitHandles[2];

        HANDLE     hWakeUpEvent;
	HANDLE	hParentProcess;
	DWORD waitRet;

        TkWinXInit(hAppInstance);
        hWakeUpEvent = CreateEvent(NULL, FALSE, FALSE, "Local\\RAT UI WakeUp Event");

#endif

        debug_set_core_dir(argv[0]);

	debug_msg("rat-ui-%s started argc=%d\n", RAT_VERSION, argc);
	parse_args(argc, argv);
	tcl_init1(argc, argv);

#ifdef WIN32
	hParentProcess = OpenProcess(SYNCHRONIZE, FALSE, ppid);
	debug_msg("OpenPRocess returns %d\n", hParentProcess);
#endif

	sprintf(m_addr, "(media:audio module:ui app:rat id:%lu)", (unsigned long) ppid);
	m = mbus_init(mbus_ui_rx, mbus_error_handler, m_addr);
        if (m == NULL) {
                fatal_error("RAT v" RAT_VERSION, "Could not initialize Mbus: Is multicast enabled?");
                return FALSE;
        }

	/* Next, we signal to the controller that we are ready to go. It should be sending  */
	/* us an mbus.waiting(foo) where "foo" is the same as the "-token" argument we were */
	/* passed on startup. We respond with mbus.go(foo) sent reliably to the controller. */
	debug_msg("UI waiting for mbus.waiting(%s) from controller...\n", token);
        if (mbus_rendezvous_go(m, token, (void *) m, 20000000) == NULL) {
            fatal_error("RAT v" RAT_VERSION, "UI failed mbus.waiting rendezvous with controller");
            return FALSE;
        }

	debug_msg("UI got mbus.waiting(%s) from controller...\n", token);

	/* At this point we know the mbus address of our controller, and have conducted   */
	/* a successful rendezvous with it. It will now send us configuration commands.   */
	/* We do mbus.waiting(foo) where "foo" is the original token. The controller will */
	/* eventually respond with mbus.go(foo) when it has finished sending us commands. */
	debug_msg("UI waiting for mbus.go(%s) from controller...\n", token);
        if ((mbus_rendezvous_waiting(m, c_addr, token, (void *) m, 20000000)) == NULL) {
            fatal_error("RAT v" RAT_VERSION, "UI failed mbus.go rendezvous with controller");
            return FALSE;
        }
	debug_msg("UI got mbus.go(%s) from controller...\n", token);

	/* Okay, we wait for the media engine to solicit for a user interface... */
	debug_msg("UI waiting for mbus.waiting(rat-ui-requested) from media engine...\n");
	do {
		mbus_heartbeat(m, 1);
		mbus_retransmit(m);
		mbus_send(m);
		timeout.tv_sec  = 0;
		timeout.tv_usec = 10000;
		mbus_recv(m, (void *) m, &timeout);
		while (Tcl_DoOneEvent(TCL_DONT_WAIT | TCL_ALL_EVENTS)) {
			/* Process Tcl/Tk events... */
		}
	} while (e_addr == NULL);
	mbus_qmsgf(m, e_addr, TRUE, "mbus.go", "\"rat-ui-requested\"");
	debug_msg("UI got mbus.waiting(%s) from media engine...\n", token);

	tcl_init2(m, e_addr);
	ui_active = TRUE;
	while (!should_exit) {
		timeout.tv_sec  = 0;
		timeout.tv_usec = 20000;
		mbus_recv(m, (void *)m, &timeout);
		mbus_heartbeat(m, 1);
		mbus_retransmit(m);
		mbus_send(m);
		while (Tcl_DoOneEvent(TCL_DONT_WAIT | TCL_ALL_EVENTS)) {
			/* Process Tcl/Tk events... */
		}
		if (Tk_GetNumMainWindows() == 0) {
			should_exit = TRUE;
		}
		/* Throttle CPU usage */
#ifdef WIN32
                /* Just timeout waiting for event that never happens */
                /* WaitForSingleObject(hWakeUpEvent, 30); */

		waitHandles[0] = hWakeUpEvent;
		waitHandles[1] = hParentProcess;
		waitRet = WaitForMultipleObjects(2, waitHandles, FALSE, 30);
		
		if ((waitRet - WAIT_OBJECT_0) == 1)
		{
			debug_msg("UI: Parent process died\n");
			should_exit = TRUE;
		}
#else
		timeout.tv_sec  = 0;
		timeout.tv_usec = 30000;
                select(0, NULL, NULL, NULL, &timeout);
#endif
                /* If controller has died call it a day.  Need this for Win32
                 * as controller can die via terminate call and not be given
                 * chance to send quit message
                 */
                if (mbus_addr_valid(m, c_addr) == FALSE) {
                        should_exit = TRUE;
                        debug_msg("UI: Controller address is no longer valid.  Assuming it exited.\n");
                }
	}

        if (mbus_addr_valid(m, e_addr)) {
                /* Close things down nicely... Tell the media engine we wish to detach... */
                mbus_qmsgf(m, e_addr, TRUE, "tool.rat.ui.detach.request", "");
                mbus_send(m);
		debug_msg("UI Waiting for tool.rat.ui.detach() from media engine...\n");
                while (!got_detach  && mbus_addr_valid(m, e_addr) && mbus_shutdown_error == FALSE) {
                        mbus_heartbeat(m, 1);
                        mbus_retransmit(m);
                        mbus_send(m);
                        timeout.tv_sec  = 0;
                        timeout.tv_usec = 10000;
                        mbus_recv(m, (void *) m, &timeout);
                }
		debug_msg("UI Got tool.rat.ui.detach() from media engine...\n");
        } else {
                debug_msg("UI: Engine looks like it has exited already.\n");
        }

        if (mbus_addr_valid(m, c_addr)) {
                /* Tell the controller it's time to quit... */
                mbus_qmsgf(m, c_addr, TRUE, "mbus.quit", "");
                do {
                        mbus_send(m);
                        mbus_retransmit(m);
                        timeout.tv_sec  = 0;
                        timeout.tv_usec = 20000;
                        mbus_recv(m, NULL, &timeout);
                } while (!mbus_sent_all(m) && mbus_shutdown_error == FALSE);

                debug_msg("UI Waiting for mbus.quit() from controller...\n");
                while (got_quit == FALSE && mbus_shutdown_error == FALSE) {
                        mbus_heartbeat(m, 1);
                        mbus_retransmit(m);
                        mbus_send(m);
                        timeout.tv_sec  = 0;
                        timeout.tv_usec = 10000;
                        mbus_recv(m, (void *) m, &timeout);
                }
                debug_msg("UI Got mbus.quit() from controller...\n");
        } else {
                debug_msg("UI: Controller appears to have exited already.\n");
        }

	mbus_qmsgf(m, "()", FALSE, "mbus.bye", "");
	do {
		mbus_send(m);
		mbus_retransmit(m);
		timeout.tv_sec  = 0;
		timeout.tv_usec = 20000;
		mbus_recv(m, NULL, &timeout);
	} while (mbus_sent_all(m) == FALSE && mbus_shutdown_error == FALSE);
	mbus_exit(m);

        xfree(c_addr);
        xfree(e_addr);
        xfree(token);
        xfree(token_e);
        tcl_exit();
#ifdef WIN32
        TkWinXCleanup(hAppInstance);
#endif
        xmemdmp();
	debug_msg("User interface exit\n");
        return 0;
}

