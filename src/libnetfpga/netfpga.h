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

#ifndef _NETFPGA_H_
#define _NETFPGA_H_

#include <sys/queue.h>

/*
 * Debugging
 */
extern int	nf_debug;
extern int	nf_tbd;
#define DEBUG(...)						\
	if (nf_debug) printf(__VA_ARGS__)
#define DBGS(x)							\
	printf("%s = '%s'\n", #x, (x))
#define ASSERT assert

/*
 * Helper typedefs for NetFPGA accessory methods.
 */
struct netfpga;
typedef void *nf_open_t(struct netfpga *nf);
typedef int nf_close_t(struct netfpga *nf, void *ctx);
typedef int nf_read_t(struct netfpga *nf, void *ctx, uint32_t reg,
    void *buf, size_t buf_len);
typedef int nf_write_t(struct netfpga *nf, void *ctx, uint32_t reg,
    void *buf, size_t buf_len);

/*
 * OS-specific handlers for NetFPGA manipulation. No function can be
 * left uninitialized.
 */
struct nf_module {
	unsigned int		 nf_version;
	unsigned int		 nf_flags;
	const char		*nf_name;
	char			 __empty[64]; /* Future */

	nf_open_t		*nf_open;
	nf_close_t		*nf_close;
	nf_read_t		*nf_read;
	nf_write_t		*nf_write;
};

struct nf_reg {
	char		*nfr_name;
	uint32_t	 nfr_offset;
	TAILQ_ENTRY(nf_reg)	next;
};
TAILQ_HEAD(nf_regs, nf_reg);

/*
 * Main context of the NetFPGA library.
 */
struct netfpga {
	/* Debugging */
	unsigned		__nf_magic;
#define NETFPGA_MAGIC		0x3d34e37e
#define NETFPGA_INIT(n)		((n)->__nf_magic = NETFPGA_MAGIC)
#define NETFPGA_ASSERT(n)	(assert((n)->__nf_magic == NETFPGA_MAGIC))

	/* Private: error handling */
	FILE			*__nf_err_fp;
	char			 __nf_errmsg_src[512];
	char			 __nf_errmsg[1024];

	/* Private: stuff */
	unsigned		 __nf_flags;
	struct nf_module	*__nf_mod;
	void			*__nf_mod_ctx;
	struct nf_regs		*__nf_regs;

	/* Public: stuff */
	const char		*nf_iface;
	const char		*nf_module;
	int			 nf_verbose;
};
#define	NETFPGA_FLAG_INITIALIZED	(1 << 0)

/*
 * Initialize NetFPGA variables. Keep it here and remember not to forget
 * about anything from the ``netfpga'' structure.
 */
static inline void
nf_init(struct netfpga *nf)
{
	NETFPGA_INIT(nf);

	nf->__nf_err_fp = NULL;
	memset(nf->__nf_errmsg_src, 0, sizeof(nf->__nf_errmsg_src));
	memset(nf->__nf_errmsg, 0, sizeof(nf->__nf_errmsg));

	nf->__nf_flags |= NETFPGA_FLAG_INITIALIZED;
	nf->__nf_mod = NULL;
	nf->__nf_mod_ctx = NULL;
	nf->__nf_regs = NULL;

	nf->nf_iface = NULL;
	nf->nf_module = NULL;
	nf->nf_verbose = 0;
}

/*
 * Has the library been initialized?
 */
static inline int
nf_initialized(struct netfpga *nf)
{

	return ((nf->__nf_flags & NETFPGA_FLAG_INITIALIZED) != 0);
}

#define nf_assert(nf)	do {					\
	ASSERT(nf != NULL &&						\
	    "Pointer to 'struct netfpga *' can't be NULL");		\
	NETFPGA_ASSERT(nf);						\
	ASSERT(nf_initialized(nf) == 1 &&				\
	    "nf_init() hasn't been called");			\
} while (0)

/*
 * Main API.
 */
int nf_start(struct netfpga *nf);
int nf_stop(struct netfpga *nf);
void nf_reset(struct netfpga *nf);
#define MDIO_RESET_MAGIC	(0x8000)
void nf_reset_allphy(struct netfpga *nf);
int nf_read(struct netfpga *nf, uint32_t reg, void *buf, size_t buf_len);
int nf_write(struct netfpga *nf, uint32_t reg, void *buf, size_t buf_len);
uint32_t nf_rd32(struct netfpga *nf, uint32_t reg);
void nf_wr32(struct netfpga *nf, uint32_t reg, uint32_t value);
int nf_image_name(struct netfpga *nf, char *dev_name, size_t dev_name_len);
void nf_image_name_print_fp(struct netfpga *nf, FILE *fp);
void nf_image_name_print(struct netfpga *nf);
int nf_prog_reset(struct netfpga *nf);
int nf_image_write(struct netfpga *nf, const char *fname);
int nf_cpci_write(struct netfpga *nf, const char *fname);
int nf_reg_byname(struct netfpga *nf, const char *name, uint32_t *reg);
void nf_reg_print_all(struct netfpga *nf, int verbose);

/*
 * Error handling
 */
void _nf_err_fmt(struct netfpga *nf, const char *func, int lineno,
    const char *fmt, va_list va);
struct netfpga *_nf_err(struct netfpga *nf, const char *func, int lineno,
    const char *fmt, ...);
#define nf_err(nf, fmt, ...)					\
	(_nf_err(nf, __func__, __LINE__, fmt, ## __VA_ARGS__))
int _nf_erri(struct netfpga *nf, const char *func, int lineno,
    const char *fmt, ...);
#define nf_erri(nf, fmt, ...)					\
	(_nf_erri(nf, __func__, __LINE__, fmt, ## __VA_ARGS__))
int nf_has_error(struct netfpga *nf);
const char *nf_strerror(struct netfpga *nf);
#define NETFPGA_SECTION	"netfpga"

/*
 * Path for module lookup.
 */
#define NETFPGA_PATH_LIB	"."

#endif
