/*
 * FILE:    process.h
 * PROGRAM: RAT - controller
 * AUTHOR:  Colin Perkins / Orion Hodson
 *
 * Copyright (c) 1999-2001 University College London
 * All rights reserved.
 */

void fork_process(char *proc_name, char *ctrl_addr, pid_t *pid, int num_tokens, char *token[2], int argc, char **argv);
void kill_process(pid_t proc);

