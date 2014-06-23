#include <stdio.h>
#include <stdlib.h>

#include <cla.h>

static cla_func_t callback;

static int
callback(struct cla *cla, int argc, char **argv)
{
	int i;

	printf("Function called for command: %s\n", cla_get_name(cla));
	for (i = 0; i < argc; i++)
		printf("arg%02d = '%s'\n", i, argv[i]);
	return (0);
}

int
main(int argc, char **argv)
{
	struct cla *root;
	struct cla *leaf1;
	struct cla *leaf2;
	struct cla *leaf3;
	struct cla *leaf4;
	int error;
	int ac;
	char *av[10]; /* some more pointers */
	int opts;

	(void)argc;
	(void)argv;

	root = cla_new(callback, NULL, NULL, "go command", "go");
	    leaf1 = cla_new(callback, NULL, NULL, "gadget", "gadget");
	        leaf2 = cla_new(callback, NULL, NULL, "carmen", "carmen");
	            leaf3 = cla_new(callback, NULL, NULL, "yataman", "yataman");
	            leaf4 = cla_new(callback, NULL, NULL, "duffy", "duffy");
	cla_add_cmd(root, leaf1);
	cla_add_subcmd(leaf1, leaf2);
	cla_add_subcmd(leaf2, leaf3);
	cla_add_subcmd(leaf2, leaf4);

	av[0] = "go";
	av[1] = "gadget";
	av[2] = NULL;
	ac = 2;
	opts = CLADIS_NODE_CALL;

	error = cla_dispatch(root, "usage", ac, av, opts);
	(void)printf("Dispatch error: %s", cla_strerror(error));
	cla_destroy(root);

	return (EXIT_SUCCESS);
}
