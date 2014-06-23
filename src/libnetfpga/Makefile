#
# Copyright (c) 2009 HIIT <http://www.hiit.fi/>
# All rights reserved.
#
# Author: Wojciech A. Koszek <wkoszek@FreeBSD.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $Id$
#

.PATH: ${.CURDIR}/../libxbf

up:
	(cd ../../ && make up)
PROG=	netfpga

SRCS+=	netfpga.c 
SRCS+=	netfpga_dummy.c
SRCS+=	netfpga_linux.c
SRCS+=	netfpga_freebsd.c
SRCS+=	xbf.c


CFLAGS+=	-DNETFPGA_PROG
CFLAGS+=	-g -ggdb -O0

WARNS+=	6

NO_MAN=	sorry

LIBS+=	xbf.so
LIBS+=	netfpga.so
LIBS+=	netfpga_freebsd.so
LIBS+=	netfpga_linux.so
LIBS+=	netfpga_dummy.so

libs: $(LIBS)

xbf.so: ../libxbf/xbf.c Makefile
	$(CC) $(CFLAGS) -shared ../libxbf/xbf.c -o xbf.so

netfpga.so: netfpga.c Makefile
	$(CC) $(CFLAGS) -shared netfpga.c -o netfpga.so

netfpga_freebsd.so: netfpga.so netfpga_freebsd.c netfpga.h
	$(CC) $(CFLAGS) -shared netfpga.so xbf.so netfpga_freebsd.c -o netfpga_freebsd.so

netfpga_linux.so: netfpga_linux.c netfpga.h
	$(CC) $(CFLAGS) -shared netfpga.so xbf.so netfpga_linux.c -o netfpga_linux.so

netfpga_dummy.so: netfpga_dummy.c netfpga.h
	$(CC) $(CFLAGS) -shared netfpga.so xbf.so netfpga_dummy.c -o netfpga_dummy.so

CLEANFILES+=	$(LIBS)

testman:
	groff -man -Tascii netfpga.3

.include <bsd.prog.mk>
