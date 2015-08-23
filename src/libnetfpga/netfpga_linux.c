/*-
 * Copyright (c) 2009 HIIT <http://www.hiit.fi/>
 * All rights reserved.
 *
 * Author: Wojciech A. Koszek <wkoszek@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 */

#ifdef __linux__
#include <sys/types.h>

#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../include/nf2_common.h"
#include "../../include/nf2.h"
#include "../../include/netfpga_freebsd.h"

#include "netfpga.h"

nf_open_t nf2_linux_open;
nf_close_t nf2_linux_close;
nf_read_t nf2_linux_read;
nf_write_t nf2_linux_write;

/*
 * Placeholders for NetFPGA Linux handling code
 */
void *
nf2_linux_open(struct netfpga *nf)
{

	(void)nf;
	return (NULL);
}

int
nf2_linux_close(struct netfpga *nf, void *ctx)
{

	(void)nf;
	(void)ctx;
	return (0);
}

int
nf2_linux_read(struct netfpga *nf, void *ctx, uint32_t reg, void *buf, size_t buf_len)
{

	(void)nf;
	(void)ctx;
	(void)reg;
	(void)buf;
	(void)buf_len;
	return (0);
}

int
nf2_linux_write(struct netfpga *nf, void *ctx, uint32_t reg, void *buf, size_t buf_len)
{

	(void)nf;
	(void)ctx;
	(void)reg;
	(void)buf;
	(void)buf_len;
	return (0);
}

/*
 * Linux NetFPGA handler.
 */
struct nf_module nf2_linux = {
	.nf_version =	0,
	.nf_open =	nf2_linux_open,
	.nf_close = 	nf2_linux_close,
	.nf_read =	nf2_linux_read,
	.nf_write =	nf2_linux_write,
};
#endif
