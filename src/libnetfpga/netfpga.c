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
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <sys/sysctl.h>

#ifdef __linux__
#include <linux/sysctl.h>
#endif

#include <netinet/in.h>

#include <assert.h>
#include <ctype.h>
#include <dlfcn.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "../../include/nf2.h"
#include "../../include/reg_defines.h"
#include "../../include/nf2_common.h"
#include "../../include/netfpga_freebsd.h"
#include "../../contrib/libxbf/xbf.h"

#include "netfpga.h"

#define NF_WR32	nf_wr32
#define NF_RD32	nf_rd32

int nf_debug = 0;
int nf_tbd = 0;

extern struct nf_module nf2_dummy;
extern struct nf_module nf2_freebsd;
extern struct nf_module nf2_linux;

static struct nf_module *nf_modules[] = {
#ifdef __FreeBSD__
	&nf2_freebsd,
#endif
#ifdef __linux__
	&nf2_linux,
#endif
	&nf2_dummy
};

/*
 * Returns true if there was an error in a NetFPGA library, and error
 * message has been filled.
 */
int
nf_has_error(struct netfpga *nf)
{

	nf_assert(nf);
	return (nf->__nf_errmsg[0] != '\0');
}

/*
 * Get error message from library's context
 */
const char *
nf_strerror(struct netfpga *nf)
{

	nf_assert(nf);
	return (nf->__nf_errmsg);
}

/*
 * Base function for error handling -- it takes function name, line
 * number and format string and creates appropriate variables for later
 * use.
 */
void
_nf_err_fmt(struct netfpga *nf, const char *func, int lineno,
    const char *fmt, va_list va)
{

	nf_assert(nf);
	ASSERT(func != NULL);
	ASSERT(fmt != NULL);

	/*
	 * Fill the error message only if we don't have a message yet.
	 * Do nothing otherwise. This lets us to have error handling
	 * that propagates from embedded functions.
	 */
	if (nf_has_error(nf)) {
#if 0
		printf("Error already present: %s\n", nf->__nf_errmsg);
#endif
		return;
	}

	memset(nf->__nf_errmsg_src, 0, sizeof(nf->__nf_errmsg_src));
	memset(nf->__nf_errmsg, 0, sizeof(nf->__nf_errmsg));
	snprintf(nf->__nf_errmsg_src, sizeof(nf->__nf_errmsg_src) - 1,
	    "%s(%d): ", func, lineno);
	vsnprintf(nf->__nf_errmsg, sizeof(nf->__nf_errmsg), fmt, va);
}

/*
 * Fill the error report and return a library context.
 */
struct netfpga *
_nf_err(struct netfpga *nf, const char *func, int lineno, const char *fmt, ...)
{
	va_list va;

	nf_assert(nf);
	va_start(va, fmt);
	_nf_err_fmt(nf, func, lineno, fmt, va);
	va_end(va);

	return (nf);
}

/*
 * Fill the error report and return error code.
 */
int
_nf_erri(struct netfpga *nf, const char *func, int lineno, const char *fmt, ...)
{
	va_list va;

	nf_assert(nf);
	va_start(va, fmt);
	_nf_err_fmt(nf, func, lineno, fmt, va);
	va_end(va);

	return (-lineno);
}

/*
 * Main function for NetFPGA startup. Passed pointer must be initialized
 * with nf_init() prior to the nf_start().
 */
int
nf_start(struct netfpga *nf)
{
	nf_open_t *nfopen;
	struct utsname ut;
	void *ctx;
	char *tmp;
	unsigned i;
	int error;

	nf_assert(nf);
	tmp = NULL;
	error = 0;

	if (!nf_initialized(nf))
		return (nf_erri(nf, "Library hasn't been initialized"));
	/*
	 * If user passed specific module name, use it.
	 */
	if (nf->nf_module != NULL) {
		tmp = strdup(nf->nf_module);
	} else {
		/*
		 * Otherwise, get the system's name and convert it to lower
		 * case letters,
		 */
		memset(&ut, 0, sizeof(ut));
		error = uname(&ut);
		if (error != 0)
			return (nf_erri(nf, "Couldn't get system type "
			    "(uname(2) failed)!"));
		tmp = calloc(1, strlen(ut.sysname) + 1);
		ASSERT(tmp != NULL);
		for (i = 0; i < strlen(ut.sysname); i++)
			tmp[i] = tolower(ut.sysname[i]);
	}
	ASSERT(tmp != NULL);
	nf->__nf_mod = nf_modules[0];
	nfopen = nf_modules[0]->nf_open;
	if (nfopen == NULL)
		return (nf_erri(nf, "There is no 'open' method in a module"));
	ctx = nfopen(nf);
	if (ctx == NULL)
		return (nf_erri(nf, "Method 'open' failed"));
	nf->__nf_mod_ctx = ctx;
	nf->__nf_regs = NULL;
	return (error);
}

/*
 * Stop NetFPGA library.
 */
int
nf_stop(struct netfpga *nf)
{
	int error;
	nf_close_t *nfclose;

	nf_assert(nf);
	if (nf->__nf_mod == NULL)
		return (nf_erri(nf, "No module loaded -- internal error"));
	ASSERT(nf->__nf_mod != NULL);
	/*
	 * Call handlers private close() function and later, unmap our
	 * handler's module.
	 */
	nfclose = nf->__nf_mod->nf_close;
	if (nfclose == NULL)
		return (nf_erri(nf, "There is no 'close' method' in a module"));
	error = nfclose(nf, nf->__nf_mod_ctx);
	if (error != 0)
		return (-1);
	/* Clear the library state */
	nf_init(nf);
	nf->__nf_flags = 0;
	return (0);
}

/*
 * Reset card.
 */
void
nf_reset(struct netfpga *nf)
{
	uint32_t ctrl;

	nf_assert(nf);
	ctrl = NF_RD32(nf, CPCI_REG_CTRL);
	ctrl |= CTRL_CNET_RESET;
	NF_WR32(nf, CPCI_REG_CTRL, ctrl);
}

/*
 * Reset all PHYs.
 */
void
nf_reset_allphy(struct netfpga *nf)
{
	uint32_t m;
	nf_assert(nf);

	m = MDIO_RESET_MAGIC;
	NF_WR32(nf, MDIO_0_CONTROL_REG, m);
	NF_WR32(nf, MDIO_1_CONTROL_REG, m);
	NF_WR32(nf, MDIO_2_CONTROL_REG, m);
	NF_WR32(nf, MDIO_3_CONTROL_REG, m);
}

/*
 * Read ``buf_len'' bytes from address ``reg'' to ``buf''. Take care of
 * all requirements regarding memory alignment.
 */
int
nf_read(struct netfpga *nf, uint32_t reg, void *buf, size_t buf_len)
{

	nf_assert(nf);
	ASSERT(buf != NULL);
	ASSERT(nf->__nf_mod != NULL && "i/o module must exist");
	ASSERT(nf->__nf_mod->nf_read != NULL && "module must have read"
	    " handler");
	ASSERT(reg % 4 == 0 && "must be 4 aligned");
	ASSERT(buf_len % 4 == 0 && "must be 4 aligned");
	return (nf->__nf_mod->nf_read(nf, nf->__nf_mod_ctx, reg, buf, buf_len));
}

/*
 * Write ``buf_len'' bytes to address ``reg'' from ``buf''. Take care of
 * all requirements regarding memory alignment.
 */
int
nf_write(struct netfpga *nf, uint32_t reg, void *buf, size_t buf_len)
{

	nf_assert(nf);
	ASSERT(buf != NULL);
	ASSERT(nf->__nf_mod != NULL);
	ASSERT(nf->__nf_mod->nf_read != NULL);
	ASSERT(reg % 4 == 0 && "must be 4 aligned");
	ASSERT(buf_len % 4 == 0 && "must be 4 aligned");
	return (nf->__nf_mod->nf_write(nf, nf->__nf_mod_ctx, reg, buf, buf_len));
}

/*
 * Most commonly used helper around nf_read() which returns 32bit
 * value of register ``reg''
 */
uint32_t
nf_rd32(struct netfpga *nf, uint32_t reg)
{
	uint32_t u32;
	int ret;

	nf_assert(nf);
	ret = nf_read(nf, reg, &u32, sizeof(u32));
	ASSERT(ret == sizeof(u32));
	return (u32);
}

/*
 * Most commonly used helper around nf_write() which writes 32bit
 * value ``value'' to register ``reg''
 */
void
nf_wr32(struct netfpga *nf, uint32_t reg, uint32_t value)
{
	int ret;

	nf_assert(nf);
	ret = nf_write(nf, reg, &value, sizeof(value));
	ASSERT(ret == sizeof(value));
}

/*
 * Get registers offset from the kernel.
 */
static struct nf_regs *
_nf_get_regs(void)
{
	FILE *fp;
	struct nf_regs *list;
	struct nf_reg *reg;
	char line[512];
	char *e, *b;
	char *regname;
	char *regval;
	uint32_t offset;
	int error, ret;

	list = calloc(1, sizeof(*list));
	ASSERT(list != NULL);
	TAILQ_INIT(list);

	error = 0;
#ifndef __linux__
	error = sysctlbyname("dev.nfc.0.dev_uiface", NULL, NULL, NULL, 0);
#endif
	if (error != 0)
		return (NULL);

	/*
	 * sysctlbyname() should be used, but since we execute gunzip
	 * anyway..
	 */
	fp = popen("/sbin/sysctl -b dev.nfc.0.dev_uiface | "
	    "/usr/bin/gunzip -c -", "w+");
	ASSERT(fp != NULL);

	/* That's naive parsing. */
	while (fgets(line, sizeof(line), fp)) {
		/* Strip end of line */
		e = strrchr(line, '\0');
		ASSERT(e != NULL);
		e--;
		while (isspace(*e) && e > line) {
			*e = '\0';
			e--;
		}
		if (line == e || *line == '\0')
			continue;

		/* Pick the right beginning */
		b = strstr(line, "#define ");
		if (b == NULL)
			continue;
		b += sizeof("#define ") - 1;

		/* Check if we have register name */
		regname = b;
		while (!isspace(*b) && b < e && *b != '\0')
			b++;
		if (*b == '\0')
			continue;

		/* Check to see if we have register value */
		*b = '\0';
		b++;
		while (isspace(*b) && b < e && *b != '\0')
			b++;
		if (*b == '\0')
			continue;
		regval = b;

		/* Try to get the right register offset */
		ret = sscanf(regval, "%x", &offset);
		if (ret != 1)
			ret = sscanf(regval, "%d", &offset);
		if (ret != 1)
			continue;

		/* Insert register to the list */
		reg = calloc(1, sizeof(*reg));
		reg->nfr_name = strdup(regname);
		reg->nfr_offset = offset;
		TAILQ_INSERT_TAIL(list, reg, next);
	}
	fclose(fp);
	return (list);
}

/*
 * Try to get register by name. It will generate a list of registers
 * once it's used for the first time. Otherwise, it'll use list that was
 * generated by the previous calls to this function.
 */
int
nf_reg_byname(struct netfpga *nf, const char *name, uint32_t *reg)
{
	struct nf_reg *nfr;

	nf_assert(nf);
	ASSERT(name != NULL);

	if (nf->__nf_regs == NULL)
		nf->__nf_regs = _nf_get_regs();
	ASSERT(nf->__nf_regs != NULL && "couldn't read register list");

	/* This call is for register list re-reading */
	if (reg == NULL)
		return (0);

	TAILQ_FOREACH(nfr, nf->__nf_regs, next) {
		if (strcmp(nfr->nfr_name, name) == 0) {
			*reg = nfr->nfr_offset;
			return (1);
		}
	}
	return (0);
}

void
nf_reg_print_all(struct netfpga *nf, int verbose)
{
	struct nf_reg *nfr;
	struct nf_regs *list;

	nf_assert(nf);

	/* For NetFPGA library to read register offsets */
	(void)nf_reg_byname(nf, "", NULL);

	list = nf->__nf_regs;
	ASSERT(list != NULL);

	TAILQ_FOREACH(nfr, list, next) {
		printf("%s", nfr->nfr_name);
		if (verbose)
			printf("\t%#x", nfr->nfr_offset);
		printf("\n");
	}
}

/*
 * Return image's internal name. This way one can recognize what has
 * been synthetized on a device.
 */
int
nf_image_name(struct netfpga *nf, void *dev_name, size_t dev_name_len)
{
	uint32_t *u32;
	int i;

	nf_assert(nf);
	ASSERT(dev_name != NULL);
	if (dev_name_len < NF2_DEVICE_STR_LEN)
		return (nf_erri(nf, "Buffer lenght for image name must "
		    "have at least %d bytes", NF2_DEVICE_STR_LEN));
	u32 = dev_name;
	for (i = 0; i < NF2_DEVICE_STR_LEN / 4; i += 4, u32++)
		*u32 = htonl(nf_rd32(nf, DEVICE_STR_REG + i));
	return (0);
}

/*
 * Validate if a bit file represented by ``xbf'' fits our card's needs
 */
static int
nf_cnet_validate(struct netfpga *nf, struct xbf *xbf)
{
	uint32_t version;
	unsigned exblen;

	nf_assert(nf);
	ASSERT(xbf != NULL);

	/* Board version */
	version = NF2_GET_VERSION(NF_RD32(nf, CPCI_REG_ID));
	DEBUG("Board version: %d\n", version);
	/* 
	 * Before we touch anything, check if we got the correct
	 * bitstream file.
	 */
	if (version == 1)
		exblen = VIRTEX_BIN_SIZE_V2_0;
	else if (version == 2 || version == 3)
		exblen = VIRTEX_BIN_SIZE_V2_1;
	else
		return (nf_erri(nf, "Version %d of the board unsupported",
		    version));
	if (exblen != xbf_get_len(xbf))
		return (nf_erri(nf, "Expected Virtex bit stream length "
		    "of '%s' to be %d, but it's %d", xbf_get_fname(xbf),
		    exblen, xbf_get_len(xbf)));
	return (0);
}

/*
 * Check, if passed bit file actually may contain data feasible to be
 * programmed on a CPCI chip.
 */
static int
nf_cpci_validate(struct netfpga *nf, struct xbf *xbf)
{
	unsigned exblen;

	exblen = CPCI_BIN_SIZE;
	if (exblen != xbf_get_len(xbf))
		return (nf_erri(nf, "Expected CPCI bit stream length "
		    "of '%s' to be %d, but it's %d", xbf_get_fname(xbf),
		    exblen, xbf_get_len(xbf)));
	return (0);
}

/*
 * Perform a reset before we're going to program Virtex device.
 */
int
nf_prog_reset(struct netfpga *nf)
{
	uint32_t status;
	uint32_t errreg;
	const char *done;
	const char *empty;

	nf_assert(nf);

	done = "(NOTYET)";
	empty = "(NOTYET)";

	/* Reset the programming interface */
	NF_WR32(nf, CPCI_REG_PROG_CTRL, RESET_CPCI);

	/* Clear error register */
	NF_WR32(nf, CPCI_REG_ERROR, 0);

	/* Sleep a bit */
	usleep(100);

	/* 
	 * Read programming status: Check that DONE bit (bit 8) is zero
	 * and FIFO (bit 1) is empty (1).
	 */
	status = NF_RD32(nf, CPCI_REG_PROG_STATUS);
	if ((status & (PROG_DONE | PROG_FIFO_EMPTY)) != PROG_FIFO_EMPTY) {
		if (status & PROG_DONE)
			done = "DONE";
		if (status & PROG_FIFO_EMPTY)
			empty = "EMPTY";

		return (nf_erri(nf, "After programming interface "
			"reset, FIFO isn't empty(flags: %s %s)", done, empty)
		);
	}

	/* 
	 * XXWKOSZEK: Try to find error bits and print something
	 * sensible ONLY if something goes wrong. Users won't understand
	 * error flags in the raw manner anyway. Error registers. nf2_download
	 * consistency.
	 */
	errreg = NF_RD32(nf, CPCI_REG_ERROR);
	fprintf(stderr, "Error Registers: %x\n", errreg);

	/* XXWKOSZEK: This doesn't make much sense */
	fprintf(stderr, "Good, after resetting programming interface the FIFO is empty\n");

	/* Sleep a bit */
	usleep(1000);
	status = NF_RD32(nf, CPCI_REG_PROG_STATUS);
	ASSERT(status & (PROG_INIT | PROG_FIFO_EMPTY));
	return (0);
}

/*
 * Wait till programming process reaches it's end. Inform, how did the
 * programming process go.
 */
static int
nf_image_write_done(struct netfpga *nf)
{
	uint32_t status;
	int tries;

	nf_assert(nf);
	/* 
	 * Check, if everything is fine once we reach this stage and
	 * wait for some time if we aren't done yet.
	 */
	tries = 5;
	do {
		status = NF_RD32(nf, CPCI_REG_PROG_STATUS);
		if (status & PROG_DONE) {
			fprintf(stderr, "DONE went high - chip has been "
			    "successfully programmed.\n");
			break;
		}
		sleep(1);
		tries--;
	} while (tries > 0);

	/*
	 * Something could go wrong, because all the ``tries''
	 * iterations were necessary.
	 */
	if (tries == 0) {
		printf("status = %#x\n", status);
		if (status & PROG_INIT)
			fprintf(stderr, "INIT went high - appears to be a "
			    "programming error.\n");
		if (!(status & PROG_DONE))
			fprintf(stderr, "DONE has not gone high - looks like "
			    "an error\n");
		return (nf_erri(nf, "Error while programming NetFPGA "
		    "Virtex chip"));
	}
	return (1);
}

/*
 * Start programming process. Bit stream file has to initialized with
 * xbf_open() prior calling this function.
 */
static int
nf_image_write_start(struct netfpga *nf, struct xbf *xbf)
{
	const uint32_t *prog_word;
	uint32_t status;
	int nwrites = 0;
	size_t bytes_written = 0;
	int tries;

	nf_assert(nf);
	ASSERT(xbf != 0);

	(void)tries;
	(void)status;

	bytes_written = 0;
	prog_word = xbf_get_data(xbf);
	printf("Expected to write = %d\n", (int)xbf_get_len(xbf));

	nwrites = xbf_get_len(xbf) / 4;

	while (nwrites) {
		NF_WR32(nf, CPCI_REG_PROG_DATA, *prog_word);
		prog_word++;
		bytes_written += 4;
		nwrites--;
		if (nwrites != 0 && nwrites % 8 == 0) {
#if 0
			printf(".");
			tries = 5;
			do {
				status = NF_RD32(nf, CPCI_REG_PROG_STATUS);
				usleep(1);
				if (status & PROG_DONE)
					break;
				if (!(status & PROG_FIFO_EMPTY))
					sleep(1);
				tries--;
			} while (tries > 0);
			ASSERT(tries != 0);
#endif
		}
		if ((bytes_written % (1024 * 128)) == 0)
			printf(".");
	}
	printf("\n");
	printf("Bytes_written = %d\n", (int)bytes_written);
	return (bytes_written);
}

/*
 * Perform Virtex programming.
 */
int
nf_image_write(struct netfpga *nf, const char *fname)
{
	struct xbf xbf;
	int exblen;
	int ret;

	nf_assert(nf);

	xbf_init(&xbf);
	ret = xbf_open(&xbf, fname);
	if (ret != 0)
		return (nf_erri(nf, "Problem with the image file: %s",
		    xbf_errmsg(&xbf)));
	exblen = xbf_get_len(&xbf);
	ret = nf_cnet_validate(nf, &xbf);
	if (ret != 0)
		return (nf_erri(nf, "Invalid image for this NetFPGA"
		    " card"));
	ret = nf_prog_reset(nf);
	if (ret != 0)
		return (nf_erri(nf, "Couldn't get CPCI programmer to "
		    "reset"));
	ret = nf_image_write_start(nf, &xbf);
	if (ret != exblen)
		return (nf_erri(nf, "Couldn't program a device: "
			"(written only %d)", ret));
	ret = xbf_close(&xbf);
	if (ret != 0)
		return (nf_erri(nf, "Couldn't close bit stream file"));
	ret = nf_image_write_done(nf);
	if (ret != 1)
		return (nf_erri(nf, "Error occured while card"
		    "programming"));
	nf_reset(nf);
	nf_reset_allphy(nf);
	return (0);
}

/*
 * Start CPCI programming.
 */
static int
nf_cpci_write_start(struct netfpga *nf, struct xbf *xbf)
{
	const uint32_t *prog_wordp = NULL;
	size_t bytes_written = 0;
	uint32_t cpci_addr = VIRTEX_PROGRAM_RAM_BASE_ADDR; /* VIRTEX->SPARTAN? */
	int nwrites = 0;
	uint32_t pw;

	nf_assert(nf);
	ASSERT(xbf != 0);

	prog_wordp = (const uint32_t *)xbf_get_data(xbf);
	nwrites = xbf_get_len(xbf) / 4;
	while (nwrites) {
		pw = htonl(*prog_wordp);
		NF_WR32(nf, cpci_addr, pw);
		if (0)
			printf("CPCI word: %#x (%d)\n", pw, pw);
		cpci_addr += 4;
		prog_wordp++;
		bytes_written += 4;
		nwrites--;
		if ((bytes_written % (1024 * 128)) == 0)
			printf(".");
	}
	printf("\n");
	printf("CPCI reprogramming finished (expected %d, written "
	    "%d)\n", (int)xbf_get_len(xbf), (int)bytes_written);
	return (0);
}

/*
 * Cleanup after CPCI programming.
 */
static void
nf_cpci_write_done(struct netfpga *nf)
{

	nf_assert(nf);
	/*
	 * XXWKOSZEK:
	 * It would be nice to know what cpci_reprogrammer.bit actually does.
	 */
	NF_WR32(nf, VIRTEX_PROGRAM_CTRL_ADDR, DISABLE_RESET | START_PROGRAMMING);

	/*
	 * XXWKOSZEK:
	 * Keep this message for the sake of being 100% compatible 
	 * with the old nf2_download program, but anyway -- is the
	 * reload of BARs required?
	 */
	sleep(1);
#if 0
	fprintf(stderr, "Instructed CPCI reprogramming to start. Please reload PCI BARs.\n");
#endif
}

/*
 * Write bitstream present in 'fname' file to the CPCI
 */
int
nf_cpci_write(struct netfpga *nf, const char *fname)
{
	struct xbf xbf;
	int ret;

	xbf_init(&xbf);
	ret = xbf_open(&xbf, fname);
	if (ret != 0)
		return (nf_erri(nf, "Problem with the image file: %s",
		    xbf_errmsg(&xbf)));
	ret = nf_cpci_validate(nf, &xbf);
	if (ret != 0)
		return (nf_erri(nf, "Invalid image for this NetFPGA"
		    " card"));
	ret = nf_cpci_write_start(nf, &xbf);
	if (ret != 0)
		return (nf_erri(nf, "Couldn't write CPCI image"));
	ret = xbf_close(&xbf);
	if (ret != 0)
		return (nf_erri(nf, "Couldn't close CPCI image"));
	nf_cpci_write_done(nf);
	nf_reset_allphy(nf);
	return (0);
}

void
nf_image_name_print_fp(struct netfpga *nf, FILE *fp)
{
	char name[NF2_DEVICE_STR_LEN];
	int error;

	nf_assert(nf);
	ASSERT(fp != NULL && "file descriptor can't be NULL here");

	error = nf_image_name(nf, name, sizeof(name));
	ASSERT(error == 0 && "internal error");
	fprintf(fp, "CNET image name: '%s'\n", name);
}

void
nf_image_name_print(struct netfpga *nf)
{

	nf_assert(nf);
	nf_image_name_print_fp(nf, stdout);
}

#ifdef NETFPGA_PROG
static void
usage(const char *prog)
{

	if (prog[0] == '.')
		prog++;
	if (prog[0] == '/')
		prog++;

	printf("USAGE:\n");
	printf("%s: Look at source code\n", prog);
	printf("%s: %s <.bit file>\n", prog, prog);
	exit(EX_USAGE);
}

/*
 * Sample program that will be used until I analyze the requirements for
 * the userland tools. Please don't get used to the command line
 * arguments. Useful while NetFPGA library is taking shape.
 *
 * This stuff is only for debugging.
 */
int
main(int argc, char **argv)
{
	struct netfpga nf;
	const char *prog = NULL;
	int error = 0;
	int o = -1;
	char *arg_iface = NULL;
	char *arg_module = NULL;
	int flag_verbose = 0;
	char imgname[1024];
	int flag_cpci = 0;
	char *fname = NULL;
	char *arg_reg = NULL;
	char *arg_write = NULL;
	int flag_Reset = 0;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	prog = argv[0];
	while ((o = getopt(argc, argv, "chi:m:r:Rw:v")) != -1)
		switch (o) {
		case 'c':
			flag_cpci++;
			break;
		case 'h':
			usage(prog);
			break;
		case 'i':
			arg_iface = optarg;
			break;
		case 'm':
			arg_module = optarg;
			break;
		case 'r':
			arg_reg = optarg;
			break;
		case 'R':
			flag_Reset++;
			break;
		case 'w':
			arg_write = optarg;
			break;
		case 'v':
			flag_verbose++;
			break;
		}

	argc -= optind;
	argv += optind;

	if (arg_reg == NULL && flag_Reset == 0) {
		/* If we're not reading/writing registers.. */
		if (argc == 0)
			/* 
			 * And we have no image file,
			 * print usage()
			 */
			usage(prog);
		fname = argv[0];
	}

	nf_init(&nf);
	nf.nf_iface = arg_iface;
	nf.nf_verbose = flag_verbose;
	nf.nf_module = arg_module;
	error = nf_start(&nf);
	if (error != 0)
		err(EXIT_FAILURE, "%s", nf_strerror(&nf));
	if (arg_reg != NULL) {
		uint32_t r, v;

		error = sscanf(arg_reg, "%x", &r);
		ASSERT(error == 1);
		v = nf_rd32(&nf, r);
		printf(" reg[%#x] = %#x ['%c', '%c', '%c', '%c']\n", r,
		    v,
		    (v & 0xff000000) >> 24,
		    (v & 0x00ff0000) >> 16,
		    (v & 0x0000ff00) >> 8,
		    (v & 0x000000ff)
		);
		if (arg_write != NULL) {
			error = sscanf(arg_write, "%x", &v);
			ASSERT(error == 1);
			nf_wr32(&nf, r, v);
			v = nf_rd32(&nf, r);
			printf("Wreg[%#x] = %#x\n", r, v);
		}
	} else if (flag_cpci) {
		nf_cpci_write(&nf, fname);
	} else if (fname != NULL) {
		(void)nf_image_name(&nf, imgname, sizeof(imgname));
		printf("Image name before: '%s'\n", imgname);
		error = nf_image_write(&nf, fname);
		if (error != 0)
			err(EXIT_FAILURE, "%s", nf_strerror(&nf));
		(void)nf_image_name(&nf, imgname, sizeof(imgname));
		printf(" Image name after: %s\n", imgname);
	}

	if (arg_reg != NULL || flag_Reset == 1) {
		nf_reset(&nf);
		nf_reset_allphy(&nf);
	}
	error = nf_stop(&nf);
	if (error != 0)
		err(EXIT_FAILURE, "%s", nf_strerror(&nf));
	exit(EXIT_SUCCESS);
}
#endif
