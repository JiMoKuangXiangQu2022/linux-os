#
# Copyright (c) 2023 Leng Xujun <lengxujun2007@126.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#

CROSS_COMPILE ?= arm-linux-gnueabihf-

CC := $(CROSS_COMPILE)gcc

.PHONY: all clean

all: syscall_trace tracee1

syscall_trace: syscall_trace.c
	$(CC) -o $@ $<

tracee1: tracee1.c
	$(CC) -o $@ $<

clean:
	-rm -fr syscall_trace tracee1
