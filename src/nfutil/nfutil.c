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

/*
 * TODO -- Plugin support for:
 * - switch
 * - nic
 * - router
 * 
 * --
 *
 * Original NetFPGA code passes information about operating on
 * socket/file descriptor. If we really need remote functionality,
 * instead of passing fd/socket as it's done in the original NetFPGA's
 * code, we should simply implemented something like this:
 *
 * netfpga server [-l addr] [-p port]
 *
 * 	starts simple TCP server and wait for connections. When a client
 * 	arrives, we should simply use the very same syntax we use on the
 * 	command line:
 *
 * 		reg read ...
 * 		reg write ...
 *
 * Either "nfutil client" or simply "telnet" could be used to connect to
 * this server. 
 */
#include <sys/types.h>

#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <xbf.h>
#include <cla.h>
#include <netfpga.h>

static int	flag_quiet = 0;

static cla_func_t	nfu_cnet_write;
static cla_func_t	nfu_cnet_info;
static cla_func_t	nfu_cpci_write;
static cla_func_t	nfu_cpci_info;
static cla_func_t	nfu_reg_read;
static cla_func_t	nfu_reg_write;
static cla_func_t	nfu_reg_list;

#define	TBD()	do {						\
	printf("%s(%d) This function isn't implemented yet.\n",	\
	    __func__, __LINE__);				\
} while (0)
#define ARP()	printf("argc=%d\n", argc)

/*
 * Write CNET bitstream to the Virtex.
 */
static int
nfu_cnet_write(struct cla *cla, int argc, char **argv)
{
	struct netfpga *nf;
	int error;

	cla_assert(cla);
	nf = cla_get_func_arg(cla);
	if (argc != 2) {
		fprintf(stderr, "Command requires one argument <file>");
		return -1;
	}
	error = nf_image_write(nf, argv[1]);
	if (error != 0) {
		fprintf(stderr, "Programming failed");
		return -2;
	}
	nf_image_name_print(nf);
	return (error);
}

/*
 * Get information about bitstream, that is actually programmed
 * on the Virtex FPGA.
 */
static int
nfu_cnet_info(struct cla *cla, int argc, char **argv)
{

	nf_image_name_print(cla_get_func_arg(cla));
	return (0);
}

/*
 * Write CPCI image.
 */
static int
nfu_cpci_write(struct cla *cla, int argc, char **argv)
{
	struct netfpga *nf;

	nf = cla_get_func_arg(cla);
	if (argc != 2) {
		fprintf(stderr, "Command requires an argument <file>");
		return -1;
	}
	return (nf_cpci_write(nf, argv[1]));
}

/*
 * Obtain information from the CPCI.
 */
static int
nfu_cpci_info(struct cla *cla, int argc, char **argv)
{

	TBD();
	return (0);
}

/*
 * Read register by name or offset.
 */
static int
nfu_reg_read(struct cla *cla, int argc, char **argv)
{
	struct netfpga *nf;
	uint32_t reg;
	uint32_t value;
	int ret;

	nf = cla_get_func_arg(cla);
	if (argc != 2) {
		fprintf(stderr, "Command requires an argument <reg>");
		return -1;
	}
	ret = sscanf(argv[1], "0x%x", &reg);
	if (ret != 1)
		ret = sscanf(argv[1], "%d", &reg);
	if (ret != 1)
		ret = nf_reg_byname(nf, argv[1], &reg);
	if (ret != 1) {
		fprintf(stderr, "Couldn't convert register name"
		    " %s to the offset value", argv[1]);
		return -1;
	}
	value = nf_rd32(nf, reg);
	if (!flag_quiet)
		printf("Register %s = ", argv[1]);
	printf("%#x\n", value);
	return (0);
}

/*
 * Write register by name or offset.
 */
static int
nfu_reg_write(struct cla *cla, int argc, char **argv)
{
	struct netfpga *nf;
	int ret;
	uint32_t value;
	uint32_t reg;

	cla_assert(cla);
	if (argc != 3) {
		fprintf(stderr, "Command requires two arguments"
			" <reg> and <value>");
		return -1;
	}

	nf = cla_get_func_arg(cla);
	/* Read register offset or name */
	ret = sscanf(argv[1], "0x%x", &reg);
	if (ret != 1)
		ret = sscanf(argv[1], "%d", &reg);
	if (ret != 1)
		reg = nf_reg_byname(nf, argv[1], &reg);
	if (ret != 1) {
		fprintf(stderr, "Register format '%s' is wrong",
		    argv[1]);
		return -1;
	}

	/* Read value */
	ret = sscanf(argv[2], "0x%x", &value);
	if (ret != 1)
		ret = sscanf(argv[2], "%d", &value);
	if (ret != 1) {
		fprintf(stderr, "Value format '%s' is wrong", argv[2]);
		return -1;
	}
	nf_wr32(nf, reg, value);
	return (0);
}

/*
 * Get register list. If the -v is specified in the command line,
 * offsets are also printed.
 */
static int
nfu_reg_list(struct cla *cla, int argc, char **argv)
{
	struct netfpga *nf;
	int verbose;

	verbose = 0;
	if (argc == 2 && strcmp(argv[1], "-v") == 0)
		verbose = 1;
	nf = cla_get_func_arg(cla);
	nf_assert(nf);
	nf_reg_print_all(nf, verbose);
	return (0);
}

/*
 * Build command line tree for nfutil(8).
 */
static struct cla *
nfu_cmdlist_build(void *softc)
{
	struct cla *nf;
	struct cla *reg;
	struct cla *cpci;
	struct cla *cnet;
	struct cla *cnet_write;
	struct cla *cnet_info;
	struct cla *cpci_write;
	struct cla *cpci_info;
	struct cla *reg_read;
	struct cla *reg_write;
	struct cla *reg_list;

	nf = cla_new(NULL, NULL, NULL, NULL, "nfutil");
	reg = cla_new(NULL, NULL, NULL, NULL, "reg");
	cpci = cla_new(NULL, NULL, NULL, NULL, "cpci");
	cnet = cla_new(NULL, NULL, NULL, NULL, "cnet");

	cnet_write = cla_new(nfu_cnet_write, NULL, NULL,
	    "Write CNET bitstream", "write <file>");
	cnet_info = cla_new(nfu_cnet_info, NULL, NULL,
	    "Obtain information about CNET bitstream", "info");
	cla_add_subcmd(cnet, cnet_write);
	cla_add_subcmd(cnet, cnet_info);

	reg_read = cla_new(nfu_reg_read, NULL, NULL,
	    "Reads NetFPGA register", "read <reg>");
	reg_write = cla_new(nfu_reg_write, NULL, NULL,
	    "Writes NetFPGA register", "write <reg> <value>");
	reg_list = cla_new(nfu_reg_list, NULL, NULL,
	    "Lists all NetFPGA registers", "list");
	cla_add_subcmd(reg, reg_read);
	cla_add_subcmd(reg, reg_write);
	cla_add_subcmd(reg, reg_list);

	cpci_write = cla_new(nfu_cpci_write, NULL, NULL,
	    "Write CPCI bitstream", "write <file>");
	cpci_info = cla_new(nfu_cpci_info, NULL, NULL,
	    "Obtain information about CPCI bitstream", "info");
	cla_add_subcmd(cpci, cpci_write);
	cla_add_subcmd(cpci, cpci_info);

	cla_add_cmd(reg, cpci);
	cla_add_cmd(cpci, cnet);

	cla_add_subcmd(nf, reg);
	cla_set_func_arg(nf, softc);

	return (nf);
}

/*
 * nfutil(8)
 */
int
main(int argc, char **argv)
{
	struct netfpga nf;
	struct cla *cmdtree;
	char *arg_iface;
	char *arg_module;
	int error;
	int flag_verbose;
	int flag_help;
	int its_nfutil;
	int o;

	cmdtree = NULL;
	arg_iface = arg_module = NULL;
	o = flag_verbose = flag_quiet = flag_help = error = 0;
	its_nfutil = 0;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	if (strcmp(argv[0], "./nfutil") == 0)
		its_nfutil = 1;

	while ((o = getopt(argc, argv, "i:m:qvh")) != -1)
		switch (o) {
		case 'i':
			arg_iface = optarg;
			break;
		case 'm':
			arg_module = optarg;
			break;
		case 'q':
			flag_quiet++;
			break;
		case 'v':
			flag_verbose++;
			break;
		case 'h':
			flag_help++;
			break;
		}

	argc -= optind - 1;
	argv += optind - 1;

	if (its_nfutil)
		argv[0] = "nfutil";

	nf_init(&nf);
	nf.nf_iface = arg_iface;
	nf.nf_verbose = flag_verbose;
	nf.nf_module = arg_module;

	cmdtree = nfu_cmdlist_build(&nf);
	if (flag_help) {
		cla_print(cmdtree);
		exit(0);
	}

	error = nf_start(&nf);
	if (error != 0)
		err(EXIT_FAILURE, "%s", nf_strerror(&nf));
	error = cla_dispatch(cmdtree, "Usage:\n", argc, argv, CLADIS_NODE_USAGE);
	if (error)
		errx(EXIT_FAILURE, "%s (error: %d)", cla_strerror(error), -error);
	error = nf_stop(&nf);
	if (error != 0)
		err(EXIT_FAILURE, "%s", nf_strerror(&nf));
	exit(EXIT_SUCCESS);
}
