/*
 * FILE:    cmd_parser.h
 * PROGRAM: RAT - controller
 * AUTHOR:  Colin Perkins / Orion Hodson
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 */

void usage(char *szOffending);

typedef struct {
        const char *cmdname;                               /* Command line flag */
        int       (*cmd_proc)(struct mbus *m, char *addr, int argc, char *argv[]); /* TRUE = success, FALSE otherwise */
        int        argc;                                   /* No. of args       */
} args_handler;

const args_handler *cmd_args_handler(char *cmdname);

