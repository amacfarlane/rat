/*
 * FILE:    main_control.c
 * PROGRAM: RAT - controller
 * AUTHOR:  Colin Perkins / Orion Hodson
 *
 * This is the main program for the RAT controller.  It starts the
 * media engine and user interface, and controls them via the mbus.
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
"$Id: main_control.c 4550 2010-01-07 16:12:48Z piers $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "version.h"
#include "mbus_control.h"
#include "crypt_random.h"
#include "fatal_error.h"
#include "mbus.h"
#include "mbus_parser.h"
#include "net_udp.h"
#include "util.h"
#include "process.h"
#include "cmd_parser.h"

#ifdef WIN32
#define UI_NAME      "ratui.exe"
#define ENGINE_NAME  "ratmedia.exe"
#define CONTROL_NAME "rat.exe"
#else
#define UI_NAME      "rat-"RAT_VERSION"-ui"
#define ENGINE_NAME  "rat-"RAT_VERSION"-media"
#endif

#define DEFAULT_RTP_PORT 5004

char       *u_addr, *e_addr[2];
pid_t       pid_ui, pid_engine;
int         should_exit;
int	    ui_enabled = TRUE;
int	    continuing_from_stop=0;

static int ttl = 127;

#ifdef NEED_SNPRINTF
static int snprintf(char *s, int buf_size, const char *format, ...)
{
        /* Quick hack replacement for snprintf... note that this */
        /* doesn't check for buffer overflows, and so is open to */
        /* many really nasty attacks!                            */
        va_list	ap;
        int	rc;

        UNUSED(buf_size);
        va_start(ap, format);
#ifdef WIN32
        rc = _vsnprintf(s, buf_size, format, ap);
#else
        rc = vsprintf(s, format, ap);
#endif
        va_end(ap);
        return rc;
}
#endif

static int
address_is_valid(const char *candidate)
{
	char *addr, *p;
	int   i, port, okay = TRUE;
	addr = xstrdup(candidate);

        addr = strtok(addr, "/");
        if (udp_addr_valid(addr) == FALSE) {
                char *msg = "Invalid address: %s";
		char *m   = xmalloc(strlen(candidate) + strlen(msg));
		sprintf(m, msg, candidate);
		usage(m);
		xfree(m);
                okay = FALSE;
        }

	/* rx_port then tx_port */
	for (i = 0; i < 2; i++) {
		p = strtok(NULL, "/");
		if (p != NULL) {
			port = atoi(p);
			if (port < 1024 || port > 65534) {
				usage("Port must be > 1023 and <= 65534 (need one more for RTCP)\n");
				okay = FALSE;
			}
		}
	}
        xfree(addr);

        return okay;
}

static int parse_options_early(int argc, const char **argv)
{
        int i;

        if (argc < 2) {
                usage(NULL);
                return FALSE;
        }

        /* Iterate over all args and pick out anything needed before
         * launching other processes.
         */
        for (i = 1; i < argc; i++) {
                if (argv[i][0] != '-' && argv[i][0] != '/') {
                        continue;
                } else if (tolower((int)argv[i][1]) == 'v') { //SV-XXX: NetBSD
#ifdef WIN32
			MessageBox(NULL, "RAT v" RAT_VERSION, "RAT Version", MB_OK);
#else
                        printf("%s\n", "RAT v" RAT_VERSION);
#endif
                        return FALSE;
                } else if (argv[i][1] == '?' || tolower((int)argv[i][1]) == 'h') { //SV-XXX: NetBSD
                        usage(NULL);
                        return FALSE;
                } else if (argv[i][1] == 't' && i + 1 < argc) {
                        /*
                         * Handle ttl here because it is required before rtp
                         * address can be sent.
                         */
                        i++;
                        ttl = atoi(argv[i]);
                        /* Hack because 128 is max ttl in rtp library
                         * previous versions are 0-255.
                         */
                        if (ttl > 127) {
                                ttl = 127;
                        }
                        if (ttl < 0 || ttl > 127) {
                                usage("Usage: -t 0-127.\n");
                                return FALSE;
                        }
                } else if (strcmp(argv[i], "-noui") == 0) {
			ui_enabled = FALSE;
		}
        }

        /*
         * Validate destination address.  Do it here before launching
         * sub-processes and mbus.  This only checks the last argument
	 * for being a valid address.  In the case of layering this
	 * will not be the only one, but we have to parse all args to
	 * find this out.
         */
	return address_is_valid(argv[argc-1]);
}

static int
parse_options_late(struct mbus *m, char *addr, int argc, char *argv[])
{
        const args_handler *a;
        int                 i;

        if (argc < 2) {
                usage(NULL);
                return FALSE;
        }
        argc -= 1; /* Skip process name */
        argv += 1;

        for (i = 0; i < argc; i++) {
                a = cmd_args_handler(argv[i]);
                if (a == NULL) {
                        break;
                }
                if (a->cmd_proc && (argc - i) > a->argc) {
                        a->cmd_proc(m, addr, a->argc, argv + i + 1);
                }
                i += a->argc;
        }
        return (i != argc);
}

static int
address_count(int argc, char *argv[])
{
        const args_handler *a;
        int                 i;

        for (i = 0; i < argc; i++) {
                a = cmd_args_handler(argv[i]);
                if (a == NULL) {
                        break;
                }
                i += a->argc;
        }
        return argc - i;
}


static int
parse_addr(char *arg, char **addr, int *rx_port, int *tx_port)
{
        char *token;
        int   port;

	if (address_is_valid(arg) == FALSE) {
		*addr    = NULL;
		*rx_port = 0;
		*tx_port = 0;
		return FALSE;
	}

	/* Address is definitely valid... */

        *addr = (char *)strtok(arg, "/");

        *rx_port = DEFAULT_RTP_PORT;
        token = strtok(NULL, "/");
        if (token) {
                port     = atoi(token);
                port    &= ~1;
		*rx_port = port;
        }

        *tx_port = *rx_port;
        token = strtok(NULL, "/");
        if (token) {
                port     = atoi(token);
                port    &= ~1;
                *tx_port = port;
        }
        return TRUE;
}

static int
parse_addresses(struct mbus *m, char *e_addr[2], int argc, char *argv[])
{
        char		*addr;
        int              i, naddr, rx_port, tx_port;

        if (argc < 2) {
                usage(NULL);
                return FALSE;
        }
        argc -= 1; /* Skip process name */
        argv += 1;

        naddr = 1; /* address_count(argc, argv); */
        if (naddr < 1) {
                usage(NULL);
                return FALSE;
        }

        /* Send address to engine before parsing other args.  They need to context to be relevent */
        for(i = 0; i < naddr; i++) {
                if (parse_addr(argv[argc - naddr + i], &addr, &rx_port, &tx_port) == FALSE) {
                        usage(NULL);
                        return FALSE;
                }
		addr    = mbus_encode_str(addr);
		mbus_qmsgf(m, e_addr[0], TRUE, "rtp.addr", "%s %d %d %d", addr, rx_port, tx_port, ttl);
		if (e_addr[1] != NULL) {
			mbus_qmsgf(m, e_addr[1], TRUE, "rtp.addr", "\"224.1.2.3\" 1234 1234 1");
		}
		xfree(addr);
        }
        return TRUE;
}

static void mbus_err_handler(int seqnum, int reason)
{
        /* Something has gone wrong with the mbus transmission. At this time */
        /* we don't try to recover, just kill off the media engine and user  */
        /* interface processes and exit.                                     */

        if (should_exit == FALSE) {
                char msg[64];
                sprintf(msg, "Could not send mbus message (%d:%d)\n", seqnum, reason);
                fatal_error("RAT v" RAT_VERSION, msg);
                kill_process(pid_ui);
                kill_process(pid_engine);
                abort();
        }
}

static void terminate(struct mbus *m, char *addr, pid_t *pid)
{
        if (mbus_addr_valid(m, addr)) {
                /* This is a valid address, ask that process to quit. */
                debug_msg("Sending mbus.quit() to %s...\n", addr);
                mbus_qmsgf(m, addr, TRUE, "mbus.quit", "");
                do {
                        struct timeval	 timeout;
                        mbus_send(m);
                        mbus_heartbeat(m, 1);
                        mbus_retransmit(m);
                        timeout.tv_sec  = 0;
                        timeout.tv_usec = 20000;
                        mbus_recv(m, NULL, &timeout);
                } while (!mbus_sent_all(m));
                debug_msg("...done\n");
        } else {
                /* That process has already terminated, do nothing. */
        }
        *pid = 0;
}

#ifndef WIN32

/* Signal Handlers */
static void
sigint_handler(int signal)
{
        UNUSED(signal);
        debug_msg("Caught signal %d\n", signal);
        kill_process(pid_ui);
        kill_process(pid_engine);
        exit(-1);
}

static void
sigchld_handler(int signal)
{
        UNUSED(signal);
        debug_msg("Caught signal SIGCHLD: %d\n", signal);
	if (signal == SIGCONT)  {
	  continuing_from_stop=2;
          debug_msg("Ignoring signal SIGCONT as we've probably just come back from being suspended(SIGTSTP/SIGSTOP): %d\n", signal);
	} else {
	  if (--continuing_from_stop==-1) {
          /* Child has terminated or stopped.  Try to shutdown nicely... */
		should_exit = TRUE;
          	wait(NULL);	/* Buffy, the zombie slayer... */
		return;
	  }
	  debug_msg("Ignoring signal SIGCHLD as we've probably just come back from being suspended(SIGTSTP/SIGSTOP): %d\n", signal);
	}
}

#else

/* Can use waitForMultipleObject to check liveness of child processes */
#define NUM_CHILD_PROCS 2

static void
win32_check_children_running(void)
{
        HANDLE hProc[NUM_CHILD_PROCS];
        DWORD  dwResult;

        hProc[0] = (HANDLE)pid_ui;
        hProc[1] = (HANDLE)pid_engine;

        dwResult = WaitForMultipleObjects(NUM_CHILD_PROCS, hProc, FALSE, 0);
        if (dwResult >= WAIT_OBJECT_0 && dwResult < WAIT_OBJECT_0 + NUM_CHILD_PROCS) {
                debug_msg("Process %lu is no longer running\n",
                        (uint32_t)hProc[dwResult - WAIT_OBJECT_0]);
                kill_process(pid_ui);
                kill_process(pid_engine);
                exit(-1);
                return;
        }

        if (dwResult >= WAIT_ABANDONED_0 && dwResult < WAIT_ABANDONED_0 + NUM_CHILD_PROCS) {
                debug_msg("Process %lu wait abandoned\n",
                        (uint32_t)hProc[dwResult - WAIT_ABANDONED_0]);
                return; /* Nothing to do, process quit already */
        }

        if (dwResult == WAIT_FAILED) {
                debug_msg("Wait failed\n");
                return;
        }

        assert(dwResult == WAIT_TIMEOUT);
        return;
}

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
{
        switch(msg) {
        case WM_DESTROY:
                should_exit = TRUE;
                break;
        }
        return DefWindowProc(hWnd, msg, wParam, lParam);
}

static void win32_process_messages(void)
{
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, 0)) {
                if (GetMessage(&msg, NULL, 0, 0) == 0) {
                        OutputDebugString("\nGot WM_QUIT\n");
                        should_exit = TRUE;
                        return;
                }
                TranslateMessage( &msg );
                DispatchMessage( &msg );
        }
}

extern HINSTANCE hAppPrevInstance, hAppInstance;

void
win32_create_null_window()
{
        WNDCLASS wc;
        HWND     hWnd;

        /* Create a non-visible window so we can process messages */

        if( !hAppPrevInstance )
        {
                wc.lpszClassName = "RatControllerClass";
                wc.lpfnWndProc = MainWndProc;
                wc.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
                wc.hInstance = hAppInstance;
                wc.hIcon = NULL;
                wc.hCursor = NULL;
                wc.hbrBackground = (HBRUSH)( COLOR_WINDOW+1 );
                wc.lpszMenuName = NULL;
                wc.cbClsExtra = 0;
                wc.cbWndExtra = 0;

                RegisterClass( &wc );
        }

        hWnd = CreateWindow( "RatControllerClass",
                "RAT",
                WS_OVERLAPPEDWINDOW|WS_HSCROLL|WS_VSCROLL,
                0,
                0,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                NULL,
                NULL,
                hAppInstance,
                NULL
                );
}

#endif /* WIN32 */

#ifdef NDEF
static int main_transcoder(int argc, char *argv[])
{
	/* This is the main function when we are the transcoder controller. The command line */
	/* arguments are: -T -left <options for left side> -right <options for right side>.  */
	/* where the options for each side are standard options you would give to the audio- */
	/* tool version of rat. For example: -T -left 224.2.2.2/2222 -right -f gsm 10.0.1.4  */
	assert(strcmp(argv[1], "-T") == 0);
	UNUSED(argc);
	return 0;
}
#endif

static char *generate_token(void)
{
	char	*t = (char *) xmalloc(21);
	sprintf(t, "rat-token-%08lx", lrand48());
	return t;
}

int main(int argc, char *argv[])
{
    struct mbus	*m;
        char		 c_addr[60], *token_u[2], *token_e[2];
        int		 seed = (gethostid() << 8) | (getpid() & 0xff), final_iters;
        struct timeval	 timeout;
	int		 i, num_sessions = 0;
		char **xargv = xmalloc(argc);
		int  xargc=0;

#ifdef WIN32
    win32_create_null_window(); /* Needed to listen to messages */
#else
    signal(SIGCONT, sigchld_handler);
    signal(SIGCHLD, sigchld_handler);
    signal(SIGINT, sigint_handler);
	signal(SIGTERM, sigint_handler);
	signal(SIGHUP, sigint_handler);
#endif

	debug_msg("rat-%s started argc=%d\n", RAT_VERSION, argc);

	/* We have two modes: one for operation as a transcoder, one */
	/* when working as a normal end-system audio tool. We choose */
	/* based on the first command line argument supplied.        */
	if ((argc > 2) && (strcmp(argv[1], "-T") == 0)) {
		num_sessions = 2;
	} else {
		num_sessions = 1;
	}

    if (parse_options_early(argc, (const char**)argv) == FALSE) {
            return FALSE;
    }

    srand48(seed);
    snprintf(c_addr, 60, "(media:audio module:control app:rat id:%lu)", (unsigned long) getpid());
    debug_msg("c_addr = %s\n", c_addr);
    m = mbus_init(mbus_control_rx, mbus_err_handler, c_addr);
    if (m == NULL) {
            fatal_error("RAT v" RAT_VERSION, "Could not initialize Mbus: Is multicast enabled?");
            return FALSE;
    }
        
    /* pull out -X arguments */
    for(i=0; i<argc; i++) {
    	if( strcmp(argv[i],"-X") == 0 ) {
    		xargv[xargc] = argv[i]; xargc++; i++;
    		xargv[xargc] = argv[i]; xargc++;
    	}
    }
        

	if (ui_enabled) {
		token_u[0] = generate_token();
		fork_process(UI_NAME, c_addr, &pid_ui, 1, token_u, xargc, xargv);
  		debug_msg("Controller waiting for %s from UI...\n", token_u[0]);
        if ((u_addr = mbus_rendezvous_waiting(m, "()", token_u[0], m, 20000000)) == NULL) {
                fatal_error("RAT v" RAT_VERSION, "MBUS Failed to rendezvous with UI - Likely firewall/VPN issue");
                return FALSE;
        }
		debug_msg("Controller has rendezvous'd with UI (%s)\n",u_addr);
	}

	token_e[0] = generate_token();
	token_e[1] = generate_token();
        fork_process(ENGINE_NAME, c_addr, &pid_engine, num_sessions, token_e, xargc, xargv);
        should_exit = FALSE;
	for (i = 0; i < num_sessions; i++) {
		debug_msg("Controller waiting for %s from media engine...\n", token_e[i]);
        if ((e_addr[i] = mbus_rendezvous_waiting(m, "()", token_e[i], m, 20000000)) == NULL ) {
                fatal_error("RAT v" RAT_VERSION, "Failed to rendezvous with media engine - Likely firewall/VPN issue");
                return FALSE;
        }
		debug_msg("Controller rendezvous'd with media engine (%s)\n",e_addr[i]);
	}
		
    if (parse_addresses(m, e_addr, argc, argv) == TRUE) {
		char	*peer;

		if (ui_enabled) {
                    if ((peer = mbus_rendezvous_go(m, token_u[0], (void *) m, 20000000)) == NULL) {
                               fatal_error("RAT v" RAT_VERSION, "Failed to rendezvous with UI - Likely firewall/VPN issue");
                               return FALSE;
                    }
                    debug_msg("User interface is %s\n", peer);
		}
		for (i = 0; i < num_sessions; i++) {
                    if ((peer = mbus_rendezvous_go(m, token_e[i], (void *) m, 20000000)) == NULL) {
                               fatal_error("RAT v" RAT_VERSION, "Failed to rendezvous with UI - Likely firewall/VPN issue");
                               return FALSE;
                    }
                    debug_msg("Media engine %d is %s\n", i, peer);
		}
		debug_msg("Parsing options\n");
		for (i = 0; i < num_sessions; i++) {
			parse_options_late(m, e_addr[i], argc, argv);
		}
		debug_msg("Entering main loop\n");
		final_iters = 25;
                while (final_iters > 0) {
                        mbus_send(m);
                        mbus_heartbeat(m, 1);
                        mbus_retransmit(m);
                        timeout.tv_sec  = 0;
                        timeout.tv_usec = 20000;
#ifdef WIN32
                        win32_check_children_running();
                        win32_process_messages();
#endif
                        mbus_recv(m, NULL, &timeout);
			if (should_exit) {
				final_iters--;
			}
                }
		if (ui_enabled) {
			terminate(m, u_addr, &pid_ui);
		}
		for (i = 0; i < num_sessions; i++) {
			terminate(m, e_addr[i], &pid_engine);
		}
    }

	if (ui_enabled) {
		kill_process(pid_ui);
	}
        kill_process(pid_engine);

#ifdef WIN32
        WSACleanup();
#endif
	if (ui_enabled) xfree(token_u[0]);
	xfree(token_e[0]);
	xfree(token_e[1]);
	xfree(xargv);
        debug_msg("Controller exit\n");
        return 0;
}

