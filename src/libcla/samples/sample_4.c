#include <stdio.h>
#include <stdlib.h>

#include <cla.h>

static cla_func_t	nf_cnet_write;
static cla_func_t	nf_cnet_info;

static cla_func_t	nf_cpci_write;
static cla_func_t	nf_cpci_info;

static cla_func_t	nf_reg_read;
static cla_func_t	nf_reg_write;
static cla_func_t	nf_reg_list;


static int
nf_cnet_write(struct cla *cla, int argc, char **argv)
{

	return (0);
}

static int
nf_cnet_info(struct cla *cla, int argc, char **argv)
{

	return (0);
}

               
static int
nf_cpci_write(struct cla *cla, int argc, char **argv)
{

	return (0);
}

static int
nf_cpci_info(struct cla *cla, int argc, char **argv)
{

	return (0);
}

               
static int
nf_reg_read(struct cla *cla, int argc, char **argv)
{

	return (0);
}

static int
nf_reg_write(struct cla *cla, int argc, char **argv)
{

	return (0);
}

static int
nf_reg_list(struct cla *cla, int argc, char **argv)
{

	return (0);
}

static struct cla *
nf_cmdlist_build(void)
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

	nf =
	reg =
	    cla_new(NULL, NULL, NULL, NULL, "reg");
	cpci = 
	    cla_new(NULL, NULL, NULL, NULL, "cpci");
	cnet = 
	    cla_new(NULL, NULL, NULL, NULL, "cnet");

	cnet_write =
	    cla_new(nf_cnet_write, NULL, NULL,
	    "Write CNET bitstream", "write");
	cnet_info = 
	    cla_new(nf_cnet_info, NULL, NULL,
	    "Obtain information about CNET bitstream", "info");
	cla_add_subcmd(cnet, cnet_write);
	cla_add_subcmd(cnet, cnet_info);

	reg_read = 
	    cla_new(nf_reg_read, NULL, NULL,
	    "Reads NetFPGA register", "read");
	reg_write = 
	    cla_new(nf_reg_write, NULL, NULL,
	    "Writess NetFPGA register", "write");
	reg_list = 
	    cla_new(nf_reg_list, NULL, NULL,
	    "Lists all NetFPGA registers", "list");
	cla_add_subcmd(reg, reg_read);
	cla_add_subcmd(reg, reg_write);
	//cla_add_subcmd(reg, reg_list);

	cpci_write =
	    cla_new(nf_cpci_write, NULL, NULL,
	    "Write CPCI bitstream", "write");
	cpci_info = 
	    cla_new(nf_cpci_info, NULL, NULL,
	    "Obtain information about CPCI bitstream", "info");
	cla_add_subcmd(cpci, cpci_write);
	cla_add_subcmd(cpci, cpci_info);

	cla_add_cmd(reg, cpci);
	cla_add_cmd(cpci, cnet);

	return (nf);
}

int
main(int argc, char **argv)
{
	struct cla *cmdtree;

	cmdtree = nf_cmdlist_build();
	cla_dispatch(cmdtree, "Usage:\n", argc - 1, argv + 1, CLADIS_NODE_USAGE);
	exit(EXIT_SUCCESS);
}
