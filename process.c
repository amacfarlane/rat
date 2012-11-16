/*
 * FILE:    process.c
 * PROGRAM: RAT - controller
 * AUTHOR:  Colin Perkins / Orion Hodson
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 */

#ifndef HIDE_SOURCE_STRINGS
static const char cvsid[] = "$Id";
#endif /* HIDE_SOURCE_STRINGS */

#include "config_unix.h"
#include "config_win32.h"
#include "debug.h"
#include "mbus.h"
#include "process.h"


void fork_process(char *proc_name, char *ctrl_addr, pid_t *pid, int num_tokens, char *token[2], int argc, char **argv)
{
#ifdef WIN32
        int                      i;
        char			 args[1024];
        LPSTARTUPINFO		 startup_info;
        LPPROCESS_INFORMATION	 proc_info;

        startup_info = (LPSTARTUPINFO) xmalloc(sizeof(STARTUPINFO));
        startup_info->cb              = sizeof(STARTUPINFO);
        startup_info->lpReserved      = 0;
        startup_info->lpDesktop       = 0;
        startup_info->lpTitle         = 0;
        startup_info->dwX             = 0;
        startup_info->dwY             = 0;
        startup_info->dwXSize         = 0;
        startup_info->dwYSize         = 0;
        startup_info->dwXCountChars   = 0;
        startup_info->dwYCountChars   = 0;
        startup_info->dwFillAttribute = 0;
        startup_info->dwFlags         = 0;
        startup_info->wShowWindow     = 0;
        startup_info->cbReserved2     = 0;
        startup_info->lpReserved2     = 0;
        startup_info->hStdInput       = 0;
        startup_info->hStdOutput      = 0;
        startup_info->hStdError       = 0;

        proc_info = (LPPROCESS_INFORMATION) xmalloc(sizeof(PROCESS_INFORMATION));

	if (num_tokens == 1) {
		_snprintf(args, sizeof(args), "%s -ctrl \"%s\" -token %s", proc_name, ctrl_addr, token[0]);
	} else {
		_snprintf(args, sizeof(args), "%s -T -ctrl \"%s\" -token %s -token %s", proc_name, ctrl_addr, token[0], token[1]);
	}
        for(i=0;i<argc;i++) {
                _snprintf(args, sizeof(args), "%s %s", args, argv[i]);
        }

        if (!CreateProcess(NULL, args, NULL, NULL, TRUE, 0, NULL, NULL, startup_info, proc_info)) {
                perror("Couldn't create process");
                abort();
        }
        *pid = (pid_t) proc_info->hProcess;	/* Sigh, hope a HANDLE fits into 32 bits... */
        debug_msg("Forked %s\n", proc_name);
#else /* ...we're on unix */
	char *path, *path_env;
	int i;
#ifdef DEBUG_FORK
	if (num_tokens == 1) {
        	debug_msg("%s -ctrl '%s' -token %s\n", proc_name, ctrl_addr, token[0]);
	} else {
        	debug_msg("%s -T -ctrl '%s' -token %s -token %s\n", proc_name, ctrl_addr, token[0], token[1]);
	}
        UNUSED(pid);
#else
	if ((getuid() != 0) && (geteuid() != 0)) {
		/* Ensure that the current directory is in the PATH. This is a security */
		/* problem, but reduces the number of support calls we get...           */
		path = getenv("PATH");
		if (path == NULL) {
			path_env = (char *) xmalloc(8);
			sprintf(path_env, "PATH=.");
		} else {
			path_env = (char *) xmalloc(strlen(path) + 8);
			sprintf(path_env, "PATH=%s:.", path);
		}
		debug_msg("%s\n", path_env);
		putenv(path_env);
		/* NOTE: we MUST NOT free the memory allocated to path_env. In some    */
		/* cases the string passed to putenv() becomes part of the environment */
		/* and hence freeing the memory removes PATH from the environment.     */
	} else {
		debug_msg("Running as root? PATH unmodified\n");
	}
        /* some glibc versions have issues with malloc/free when used inside or
           in some cases outside a fork/exec, so we now use a args[64] array instead of
           xmalloc'ing it. In the unlikely event the array needs to hold more
           command-line arguments, print a error message */
        if ((2 * num_tokens) + argc + 4 >= 64) {
                fprintf(stderr, "Exceeded max of %i command-line arguments\n",
                        64 - (2 * num_tokens) + 4);
                exit(1);
        }
        /* Fork off the sub-process... */
        *pid = fork();
        if (*pid == -1) {
                perror("Cannot fork");
                abort();
        } else if (*pid == 0) {
                char *args[64];
                int numargs=0;

                args[numargs++] = proc_name;
                args[numargs++] = "-ctrl";
                args[numargs++] = ctrl_addr;
                for(i=0;i<num_tokens;i++) {
                        args[numargs++] = "-token";
                        args[numargs++] = token[i];
                }
                for(i=0;i<argc;i++) {
                        args[numargs++] = argv[i];
                }
                args[numargs++] = NULL;
                execvp( proc_name, args );

                perror("Cannot execute subprocess");
                /* Note: this MUST NOT be exit() or abort(), since they affect the standard */
                /* IO channels in the parent process (fork duplicates file descriptors, but */
                /* they still point to the same underlying file).                           */
                _exit(1);
        }
#endif
#endif
}

void kill_process(pid_t proc)
{
        if (proc == 0) {
                debug_msg("Process already dead\n", proc);
                return;
        }
        debug_msg("Killing process %d\n", proc);
#ifdef WIN32
        /* This doesn't close down DLLs or free resources, so we have to  */
        /* hope it doesn't get called. With any luck everything is closed */
        /* down by sending it an mbus.exit() message, anyway...           */
        TerminateProcess((HANDLE) proc, 0);
#else
        kill(proc, SIGINT);
#endif
}


