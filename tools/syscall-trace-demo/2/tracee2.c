/*
 * tracee2.c
 *
 * Copyright (c) 2023 Leng Xujun <lengxujun2007@126.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

 
#include <unistd.h>
#include <stdio.h>


int main(void)
{
	for (;;) {
		write(fileno(stdout), "@", 1);
		sleep(1);
	}

	return 0;
}
