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

#include <sys/types.h>
#include <sys/ioccom.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "../../include/nf2_common.h"
#include "../../include/nf2.h"
#include "../../include/netfpga_freebsd.h"

#include "netfpga.h"

nf_open_t nf2_dummy_open;
nf_close_t nf2_dummy_close;
nf_read_t nf2_dummy_read;
nf_write_t nf2_dummy_write;

#define DUMMY(...) do {					\
	printf("%s(%d) called,", __func__, __LINE__);	\
	printf(" " __VA_ARGS__);			\
	printf("\n");					\
} while (0)

/*
 * Placeholders for NetFPGA Linux handling code
 */
void *
nf2_dummy_open(struct netfpga *nf)
{
	int *dummy;

	(void)nf;
	dummy = calloc(1, sizeof(*dummy));
	ASSERT(dummy != NULL);

	*dummy = 1;
	DUMMY();

	return (dummy);
}

int
nf2_dummy_close(struct netfpga *nf, void *ctx)
{

	(void)nf;
	ASSERT(ctx != NULL);
	DUMMY();
	free(ctx);
	return (0);
}

int
nf2_dummy_read(struct netfpga *nf, void *ctx, uint32_t reg, void *buf, size_t buf_len)
{

	(void)nf;
	ASSERT(ctx != NULL);
	DUMMY("read request (reg %#x, to %p, length %d)", reg, buf, (int)buf_len);
	return (buf_len);
}

int
nf2_dummy_write(struct netfpga *nf, void *ctx, uint32_t reg, void *buf, size_t buf_len)
{

	(void)nf;
	ASSERT(ctx != NULL);
	DUMMY("write request (reg %#x, to %p, length %d)", reg, buf, (int)buf_len);
	return (buf_len);
}

/*
 * Dummy NetFPGA handler.
 */
struct nf_module nf2_dummy = {
	.nf_version =	0,
	.nf_open =	nf2_dummy_open,
	.nf_close = 	nf2_dummy_close,
	.nf_read =	nf2_dummy_read,
	.nf_write =	nf2_dummy_write,
};
