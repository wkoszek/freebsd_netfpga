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

#ifdef __FreeBSD__
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

#define NETFPGA_FREEBSD_DEVNAME	"/dev/netfpga0"

nf_open_t nf2_freebsd_open;
nf_close_t nf2_freebsd_close;
nf_read_t nf2_freebsd_read;
nf_write_t nf2_freebsd_write;

struct nf_softc {
	int fd;
};

/*
 * Open one of "/dev/netfpga[0-9]+" devices
 */
void *
nf2_freebsd_open(struct netfpga *nf)
{
	struct nf_softc *sc;
	const char *dev_name;

	sc = calloc(1, sizeof(*sc));
	ASSERT(sc != NULL);
	dev_name = NETFPGA_FREEBSD_DEVNAME;
	if (nf->nf_iface != NULL)
		dev_name = nf->nf_iface;
	sc->fd = open(dev_name, O_RDWR);
	if (sc->fd == -1) {
		(void)nf_erri(nf, "Couldn't open device %s", dev_name);
		free(sc);
		return (NULL);
	}
	return (sc);
}

/*
 * Close "/dev/netfpga[0-9]+" file. Free private memory context.
 */
int
nf2_freebsd_close(struct netfpga *nf, void *ctx)
{
	struct nf_softc *sc;
	int error;

	ASSERT(ctx != NULL);
	sc = ctx;
	error = -1;

	if (sc->fd == -1) {
		(void)nf_erri(nf, "No valid descriptor found--internal error");
		goto errout;
	}
	error = close(sc->fd);
	if (error == -1) {
		(void)nf_erri(nf, "Couldn't close device descriptor");
		goto errout;
	}
errout:
	free(sc);
	return (error);
}

/*
 * Take a context ``ctx'', get a device handler and read ``buf_len''
 * bytes from register ``reg'' to the buffer ``buf''.
 */
int
nf2_freebsd_read(struct netfpga *nf, void *ctx, uint32_t reg, void *buf,
    size_t buf_len)
{
	struct nf_softc *sc;
	struct nf_req req;
	int error = 0;
	uint32_t *u32;
	unsigned int i;
	int bytes_read;

	ASSERT(nf != NULL);
	ASSERT(buf != NULL);
	ASSERT(ctx != NULL);
	sc = ctx;
	memset(&req, 0, sizeof(req));

	/*
	 * Make sure everything is aligned correctly.
	 */
	if (buf_len % 4 != 0)
		return (nf_erri(nf, "Buffer length must be a multiple of"
		    " 4 bytes"));
	/*
	 * Read data
	 */
	bytes_read = 0;
	for (u32 = buf, i = 0; i < buf_len / 4; i += 4) {
		req.offset = reg + i;
		error = ioctl(sc->fd, SIOCREGREAD, &req);
		ASSERT(error == 0);
		*u32 = req.value;
		DEBUG("READ, req.value=%llx(%lld)\n", req.value, req.value);
		bytes_read += 4;
	}
	return (bytes_read);
}

/*
 * Take a context ``ctx'', get a device handler and write``buf_len''
 * bytes from a buffer ``buf'' to register ``reg''.
 */
int
nf2_freebsd_write(struct netfpga *nf, void *ctx, uint32_t reg, void *buf, size_t buf_len)
{
	struct nf_softc *sc;
	struct nf_req req;
	int error = 0;
	uint32_t *u32;
	unsigned int i;
	int written;

	ASSERT(nf != NULL);
	ASSERT(buf != NULL);
	ASSERT(ctx != NULL);
	sc = ctx;
	memset(&req, 0, sizeof(req));

	if (buf_len % 4 != 0)
		return (nf_erri(nf, "Buffer length must be a multiple of 4 bytes"));
	
	written = 0;
	for (u32 = buf, i = 0; i < buf_len / 4; i += 4) {
		req.offset = reg + i;
		req.value = *u32++;
		error = ioctl(sc->fd, SIOCREGWRITE, &req);
		ASSERT(error == 0);
		DEBUG("WRITE, req.offset=%llx(%lld), req.value=%llx(%lld)\n",
		    req.offset, req.offset,
		    req.value, req.value);
		written += 4;
	}
	return (written);
}

/*
 * FreeBSD NetFPGA handler
 */
struct nf_module nf2_freebsd = {
	.nf_version =	0,
	.nf_open =	nf2_freebsd_open,
	.nf_close = 	nf2_freebsd_close,
	.nf_read =	nf2_freebsd_read,
	.nf_write =	nf2_freebsd_write,
};
#endif /* __FreeBSD__ */
