/*
 * FILE:    installer.c
 * PROGRAM: RAT
 * AUTHORS: Colin Perkins
 *
 * Copyright (c) 2000-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] =
	"$Id: installer.c 3642 2004-01-12 17:14:56Z ucacoxh $";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "tcl.h"
#include "tk.h"

extern char ui_installer[];
extern char encoded_binaries[];
extern int  encoded_length;
extern char encoded_filename[];

static int
do_accept(ClientData ttp, Tcl_Interp *interp, int argc, char *argv[])
{
	FILE *outf;
	int   i;

	UNUSED(ttp);
	UNUSED(interp);
	UNUSED(argc);
	UNUSED(argv);

	printf("Generating %s (%d bytes)\n", encoded_filename, encoded_length);
	outf = fopen(encoded_filename, "w");
	for (i = 0; i < encoded_length; i++) {
		fputc((int) encoded_binaries[i], outf);
	}
	fclose(outf);

	return Tcl_Eval(interp, "destroy .");
}

int main(int argc, char *argv[])
{
	char		*cmd_line_args, buffer[10];
	Tcl_Obj 	*ui_obj;
	Tcl_Interp	*interp;

#ifdef WIN32
        HANDLE     hWakeUpEvent;
        TkWinXInit(hAppInstance);
#endif
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

        Tcl_CreateCommand(interp, "do_accept", do_accept, NULL, NULL);

	ui_obj = Tcl_NewStringObj(ui_installer, strlen(ui_installer));
	if (Tcl_EvalObj(interp, ui_obj) != TCL_OK) {
		fprintf(stderr, "ui_installer error: %s\n", Tcl_GetStringResult(interp));
	}

        while (Tk_GetNumMainWindows() > 0) {
		Tcl_DoOneEvent(TCL_ALL_EVENTS);
	}

        return 0;
}

