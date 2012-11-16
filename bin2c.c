/*
 * FILE:    bin2c.c
 * PROGRAM: bin2c
 * AUTHOR:  Colin Perkins
 *
 * Copyright (c) 2000-2001 University College London
 * All rights reserved.
 */

#include "config_unix.h"
#include "debug.h"
#include "assert.h"

#ifdef WIN32
#error This program is only needed on unix - use InstallShield on windows
#endif

int main(int argc, char *argv[])
{
	FILE 	*inf;
	int	 c, l;

	assert(argc == 2);
	inf = fopen(argv[1], "r");
	c = fgetc(inf);
	l = 1;
	printf("char encoded_binaries[] = \"\\x%02x", c);
	while (1) {
		c = fgetc(inf);
		if (feof(inf)) {
			break;
		}
		printf("\\x%02x", c);
		l++;
	}
	printf("\";\n");
	printf("int encoded_length = %d;\n", l);
	printf("char encoded_filename[] = \"%s\";\n", argv[1]);
	fclose(inf);
	return 0;
}

