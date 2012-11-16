/*
 * FILE:    tcltk.c
 * PROGRAM: RAT
 * AUTHOR:  Isidor Kouvelas + Colin Perkins + Orion Hodson
 *
 * Copyright (c) 1995-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: tcltk.c 4552 2010-01-07 16:17:03Z piers $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "tcl.h"
#include "tk.h"
#include "debug.h"
#include "auddev.h"
#include "memory.h"
#include "version.h"
#include "mbus.h"
#include "mbus_parser.h"
#include "mbus_ui.h"
#include "tcltk.h"
#include "util.h"

extern char 	ui_audiotool[];
extern char	ui_transcoder[];

#ifdef WIN32
int
WinPutsCmd(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    FILE *f;
    int i, newline;
    char *fileId;

    i = 1;
    newline = 1;
    if ((argc >= 2) && (strcmp(argv[1], "-nonewline") == 0)) {
	newline = 0;
	i++;
    }
    if ((i < (argc-3)) || (i >= argc)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" ?-nonewline? ?fileId? string\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * The code below provides backwards compatibility with an old
     * form of the command that is no longer recommended or documented.
     */

    if (i == (argc-3)) {
	if (strncmp(argv[i+2], "nonewline", strlen(argv[i+2])) != 0) {
	    Tcl_AppendResult(interp, "bad argument \"", argv[i+2],
		    "\": should be \"nonewline\"", (char *) NULL);
	    return TCL_ERROR;
	}
	newline = 0;
    }
    if (i == (argc-1)) {
	fileId = "stdout";
    } else {
	fileId = argv[i];
	i++;
    }

    if (strcmp(fileId, "stdout") == 0 || strcmp(fileId, "stderr") == 0) {
	char *result;

	if (newline) {
	    int len = strlen(argv[i]);
	    result = ckalloc(len+2);
	    memcpy(result, argv[i], len);
	    result[len] = '\n';
	    result[len+1] = 0;
	} else {
	    result = argv[i];
	}
	OutputDebugString(result);
	if (newline)
	    ckfree(result);
    } else {
	return TCL_OK;
	clearerr(f);
	fputs(argv[i], f);
	if (newline) {
	    fputc('\n', f);
	}
	if (ferror(f)) {
	    Tcl_AppendResult(interp, "error writing \"", fileId,
		    "\": ", Tcl_PosixError(interp), (char *) NULL);
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

int
WinGetUserName(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    char user[256];
    int size = sizeof(user);

    if (!GetUserName(user, &size)) {
	Tcl_AppendResult(interp, "GetUserName failed", NULL);
	return TCL_ERROR;
    }
    purge_chars(user, " \"`'![]");
    Tcl_AppendResult(interp, user, NULL);
    return TCL_OK;
}

static HKEY
regroot(root)
    char *root;
{
    if (strcasecmp(root, "HKEY_LOCAL_MACHINE") == 0)
	return HKEY_LOCAL_MACHINE;
    else if (strcasecmp(root, "HKEY_CURRENT_USER") == 0)
	return HKEY_CURRENT_USER;
    else if (strcasecmp(root, "HKEY_USERS") == 0)
	return HKEY_USERS;
    else if (strcasecmp(root, "HKEY_CLASSES_ROOT") == 0)
	return HKEY_CLASSES_ROOT;
    else
	return (HKEY)-1;
}

int
WinReg(ClientData clientdata, Tcl_Interp *interp, int argc, char **argv)
{
	static char szBuf[255], szOutBuf[255];
        char *szRegRoot = NULL, *szRegPath = NULL, *szValueName;
        int cbOutBuf = 255;
        HKEY hKey, hKeyResult;
        DWORD dwDisp;

        if (argc < 4 || argc > 5) {
                Tcl_AppendResult(interp, "wrong number of args\n", szBuf, NULL);
                return TCL_ERROR;
        }

        strncpy(szBuf, argv[2], 255);
        szValueName = argv[3];
        szRegRoot   = szBuf;
        szRegPath   = strchr(szBuf, '\\');

        if (szRegPath == NULL || szValueName == NULL) {
                Tcl_AppendResult(interp, "registry path is wrongly written\n", szBuf, NULL);
                return TCL_ERROR;
        }

        *szRegPath = '\x0';
        szRegPath++;

        hKey = regroot(szRegRoot);

        if (hKey == (HKEY)-1) {
                Tcl_AppendResult(interp, "root not found %s", szRegRoot, NULL);
                return TCL_ERROR;
        }

        if (ERROR_SUCCESS != RegCreateKeyEx(hKey, szRegPath, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKeyResult, &dwDisp)) {
                Tcl_AppendResult(interp, "Could not open key", szRegRoot, szRegPath, NULL);
                return TCL_ERROR;
        }

	if (argc == 4 && !strcmp(argv[1],"get")) {
                DWORD dwType = REG_SZ;
                if (ERROR_SUCCESS != RegQueryValueEx(hKeyResult, szValueName, 0, &dwType, szOutBuf, &cbOutBuf)) {
                        RegCloseKey(hKeyResult);
                        Tcl_AppendResult(interp, "Could not set value", szValueName, NULL);
                        return TCL_ERROR;
                }
                Tcl_SetResult(interp, szOutBuf, TCL_STATIC);
        } else if (argc == 5 && !strcmp(argv[1], "set")) {
                if (ERROR_SUCCESS != RegSetValueEx(hKeyResult, szValueName, 0, REG_SZ, argv[4], strlen(argv[4]))) {
                        RegCloseKey(hKeyResult);
                        Tcl_AppendResult(interp, "Could not set value", szValueName, argv[4], NULL);
                        return TCL_ERROR;
                }
	}
        RegCloseKey(hKeyResult);
        return TCL_OK;
}

#ifndef RegGetValue
int
RegGetValue(HKEY* key, char *subkey, char *value, char *dst, int dlen)
{
        HKEY lkey;
        LONG r;
        LONG len;
        DWORD type;

        r = RegOpenKeyEx(*key, subkey, 0, KEY_READ, &lkey);

        if (ERROR_SUCCESS == r) {
                r = RegQueryValueEx(lkey, value, 0, &type, NULL, &len);
                if (ERROR_SUCCESS == r && len <= dlen && type == REG_SZ) {
                        type = REG_SZ;
                        r = RegQueryValueEx(lkey, value, 0, &type, dst, &len);
                } else {
                        SetLastError(r);
                        perror("");
                }
        } else {
                SetLastError(r);
                perror("");
                return FALSE;
        }
        RegCloseKey(lkey);
        return TRUE;
}
#endif
#endif

Tcl_Interp 	*interp;	/* Interpreter for application. */
char       	*engine_addr;
struct mbus	*mbus_ui;

void
tcl_send(char *command)
{
	/* This is called to send a message to the user interface...  */
	/* If the UI is not enabled, the message is silently ignored. */
	assert(command != NULL);

	if (Tk_GetNumMainWindows() <= 0) {
		return;
	}

	if (Tcl_Eval(interp, command) != TCL_OK) {
		debug_msg("TCL error: %s\n", Tcl_GetVar(interp, "errorInfo", 0));
	}
}

static int
mbus_send_cmd(ClientData ttp, Tcl_Interp *i, int argc, char *argv[])
{
	if (argc != 4) {
		i->result = "mbus_send <reliable> <cmnd> <args>";
		return TCL_ERROR;
	}
	mbus_qmsg((struct mbus *)ttp, engine_addr, argv[2], argv[3], strcmp(argv[1], "R") == 0);
	return TCL_OK;
}

static int
mbus_encode_cmd(ClientData ttp, Tcl_Interp *i, int argc, char *argv[])
{
	UNUSED(ttp);
	if (argc != 2) {
		i->result = "mbus_encode_str <str>";
		return TCL_ERROR;
	}
        Tcl_SetResult(i, mbus_encode_str(argv[1]), (Tcl_FreeProc *) xfree);
	return TCL_OK;
}

#include "xbm/rat_small.xbm"
#include "xbm/disk.xbm"
#include "xbm/play.xbm"
#include "xbm/rec.xbm"
#include "xbm/pause.xbm"
#include "xbm/stop.xbm"
#include "xbm/left.xbm"
#include "xbm/right.xbm"
#include "xbm/balloon.xbm"
#include "xbm/reception.xbm"

static char*
parse_assignment(char* cp)
{
	cp = strchr(cp, '=');
	if (cp != 0) {
		*cp = 0;
		return (cp + 1);
	} else
		return ("true");
}

int
tcl_init1(int argc, char **argv)
{
	char		*cmd_line_args, buffer[10];
	Tcl_Obj 	*audiotool_obj;
	int 		i;

	Tcl_FindExecutable(argv[0]);
	interp        = Tcl_CreateInterp();
	cmd_line_args = Tcl_Merge(argc - 1, argv + 1);
	Tcl_SetVar(interp, "argv", cmd_line_args, TCL_GLOBAL_ONLY);
#ifndef WIN32
	ckfree(cmd_line_args);
#endif
	sprintf(buffer, "%d", argc - 1);
	Tcl_SetVar(interp, "argc", buffer, TCL_GLOBAL_ONLY);
	Tcl_SetVar(interp, "argv0", argv[0], TCL_GLOBAL_ONLY);
	Tcl_SetVar(interp, "tcl_interactive", "0", TCL_GLOBAL_ONLY);
	for(i=1; i<argc; i++ ) {
		if( strcmp(argv[i],"-X") == 0) {
			const char* value = parse_assignment(argv[++i]);
			Tcl_SetVar(interp, argv[i], value, TCL_GLOBAL_ONLY);
		}
	}

	/*
	 * There is no easy way of preventing the Init functions from
	 * loading the library files. Ignore error returns and load
	 * built in versions.
	 */
	if (Tcl_Init(interp) != TCL_OK) {
                fprintf(stderr, "%s\n", Tcl_GetStringResult(interp));
                exit(-1);
        }
        if (Tk_Init(interp) != TCL_OK) {
                fprintf(stderr, "%s\n", Tcl_GetStringResult(interp));
                exit(-1);
        }
#ifdef WIN32
        Tcl_SetVar(interp, "win32", "1", TCL_GLOBAL_ONLY);
        Tcl_CreateCommand(interp, "puts",        WinPutsCmd,     NULL, NULL);
        Tcl_CreateCommand(interp, "getusername", WinGetUserName, NULL, NULL);
	Tcl_CreateCommand(interp, "registry",    WinReg,         NULL, NULL);
#else
	Tcl_SetVar(interp, "win32", "0", TCL_GLOBAL_ONLY);
#endif
	Tk_DefineBitmap(interp, Tk_GetUid("rat_small"), rat_small_bits, rat_small_width, rat_small_height);
	Tk_DefineBitmap(interp, Tk_GetUid("disk"), disk_bits, disk_width, disk_height);
	Tk_DefineBitmap(interp, Tk_GetUid("play"), play_bits, play_width, play_height);
	Tk_DefineBitmap(interp, Tk_GetUid("rec"),  rec_bits,  rec_width,  rec_height);
	Tk_DefineBitmap(interp, Tk_GetUid("pause"), pause_bits, pause_width, pause_height);
	Tk_DefineBitmap(interp, Tk_GetUid("stop"),  stop_bits,  stop_width,  stop_height);
	//SV-XXX cast 3rd argument to (char *)
	Tk_DefineBitmap(interp, Tk_GetUid("left"),  (char *) left_bits,  left_width, left_height);
	Tk_DefineBitmap(interp, Tk_GetUid("right"), (char *) right_bits, right_width,  right_height);
	Tk_DefineBitmap(interp, Tk_GetUid("balloon"), (char *) balloon_bits, balloon_width,  balloon_height);
	Tk_DefineBitmap(interp, Tk_GetUid("reception"), (char *) reception_bits, reception_width,  reception_height);

	audiotool_obj = Tcl_NewStringObj(ui_audiotool, strlen(ui_audiotool));
	if (Tcl_EvalObj(interp, audiotool_obj) != TCL_OK) {
		fprintf(stderr, "ui_audiotool error: %s\n", Tcl_GetStringResult(interp));
	}
        while (Tcl_DoOneEvent(TCL_DONT_WAIT | TCL_ALL_EVENTS)) {
		/* Process Tcl/Tk events */
	}

	return TRUE;
}

int
tcl_init2(struct mbus *mbus_ui, char *mbus_engine_addr)
{
	engine_addr   = xstrdup(mbus_engine_addr);

	//SV-XXX: NetBSD: added cast to (Tcl_CmdProc *) for 3rd argument
	Tcl_CreateCommand(interp, "mbus_send",	     (Tcl_CmdProc *)mbus_send_cmd,   (ClientData) mbus_ui, NULL);
	Tcl_CreateCommand(interp, "mbus_encode_str", (Tcl_CmdProc *)mbus_encode_cmd, NULL, NULL);

	/* Process Tcl/Tk events */
        while (Tcl_DoOneEvent(TCL_DONT_WAIT | TCL_ALL_EVENTS)) {
		/* Process Tcl/Tk events */
	}
	Tcl_ResetResult(interp);

	/* We do this last, so it is executed within the main loop... */
	Tcl_Eval(interp, "rendezvous_with_media_engine");
	return TRUE;
}

void
tcl_exit()
{
        xfree(engine_addr);
}
