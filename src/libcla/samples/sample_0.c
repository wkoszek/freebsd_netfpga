#include <stdio.h>
#include <stdlib.h>

#include <cla.h>

static cla_func_t callback;

static int
callback(struct cla *cla, int argc, char **argv)
{
	int i;

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

	(void)argc;
	(void)argv;

	root = cla_new(callback, NULL, NULL, "just root", "root");
	leaf1 = cla_new(callback, NULL, NULL, "leaf1", "leaf1");
	leaf2 = cla_new(callback, NULL, NULL, "leaf2", "leaf2");
	cla_add_cmd(root, leaf1);
	cla_add_subcmd(leaf1, leaf2);
	cla_print(root);
	cla_destroy(root);

	return (EXIT_SUCCESS);
}
