/*
 * Cedarx media decoder test demo.
 *
 * Copyright (c) 2020-2023 Leng Xujun <lengxujun2007@126.com>.
 *
 * Cedarx is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 */

CC=arm-linux-gcc

all:
	$(CC) VDecoderTest.c \
			-L../../SOURCE/output/lib/full-package-name/ \
			-Wl,-rpath=lib \
			-lcedar_plugin_vd_mjpeg -lcedar_vdecoder -lcedar_common -lcedar_base \
			-I../../SOURCE/base/include/ \
			-I../../SOURCE/vdecoder/include/ \
			-g \
			-o VDecoderTest

clean:
	rm VDecoderTest

