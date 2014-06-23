/*-
 * Copyright (c) 2009 Wojciech A. Koszek <wkoszek@FreeBSD.org>
 * All rights reserved.
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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*--------------------------------------------------------------------------*/

static int debug = 1;
#define DEBUGGING if (debug > 0)

struct cmd {
	const char *name;
	const char *args;
	void (*f)(int ac, char **av);
	struct cmd *next_cmds[8];
};

void h(int ac, char **av);
void printer(struct cmd *cmd, void *arg);

/*--------------------------------------------------------------------------*/

void
h(int ac, char **av)
{
	int i;

	assert(ac != 0);
	puts("-- arguments -- ");
	for (i = 0; i < ac; i++)
		printf("av[%d] = %s\n", i, av[i]);
	puts("--");
}

struct cmd cmd_image_read = {
	"read",
	NULL,
	h,
	{ NULL },
};

struct cmd cmd_image_write = {
	"write",
	NULL,
	h,
	{ NULL },
};

struct cmd cmd_reg_read = {
	"read",
	NULL,
	h,
	{ NULL },
};

struct cmd cmd_reg_write = {
	"write",
	NULL,
	h,
	{ NULL },
};

struct cmd cmd_stats_all = {
	"all",
	NULL,
	h,
	{ NULL },
};

struct cmd cmd_stats_regs = {
	"regs",
	NULL,
	h,
	{ NULL },
};

struct cmd cmd_stats = {
	"stats",
	NULL,
	h,
	{
		&cmd_stats_all,
		&cmd_stats_regs,
		NULL,
	},
};

struct cmd cmd_image = {
	"image", "v", h,
	{
		&cmd_image_read,
		&cmd_image_write,
		NULL,
	},
};

struct cmd cmd_reg = {
	"reg", "v", h,
	{
		&cmd_reg_read,
		&cmd_reg_write,
		NULL,
	},
};

struct cmd cmd_main = {
	".", NULL, NULL,
	{
		&cmd_image,
		&cmd_reg,
		&cmd_stats,
		NULL,
	},
};

static void
cmd_dispatch(int argc, char **argv)
{
	struct cmd *cmd = NULL;
	struct cmd **cmdlist = NULL;
	struct cmd *matched = NULL;
	int m, ai;
	int matched_isleaf = 0;

	/*
	 * Rethink what cmd_dispatch() should get as it's first
	 * argument.
	 */
	if (argc <= 1) {
		fprintf(stderr, "Nothing to do\n");
		return;
	}

	cmdlist = cmd_main.next_cmds;
	cmd = cmdlist[0];
	for (ai = 1;;) {
		if (ai >= argc) { printf("ai >= argc\n"); break; }
		if (cmd == NULL) { printf("cmd == NULL\n"); break; }
		if (cmd->name == NULL) { printf("cmd->name == NULL\n"); break; }

		m = (strcmp(cmd->name, argv[ai]) == 0);
		if (!m) {
			printf("'%s' didn't match '%s'\n", cmd->name, argv[ai]);
			cmd = *++cmdlist;
		} else {
			printf("'%s' matched!\n", cmd->name);
			matched = cmd;

			cmdlist = cmd->next_cmds;
			cmd = cmdlist[0];
			ai++;
		}
	}
	if (matched == NULL) {
		/*
		 * First command wasn't even found.
		 */
		fprintf(stderr, "No commands matching.\n");
		return;
	}
	assert(matched != NULL);
	DEBUGGING {
		printf("matched->next_cmds = %p\n", (void *)matched->next_cmds);
		printf("matched->next_cmds[0] = %p\n", (void *)matched->next_cmds[0]);
	}

	matched_isleaf = (matched->next_cmds[0] == NULL);
	if (matched_isleaf) {
		if (matched->f != NULL) {
			ai--;
			matched->f(argc - ai, argv + ai);
		}
	} else {
		printf("Last matched: '%s'\n", matched->name);
	}
}

static void
cmd_print(struct cmd *cmd, int indent)
{
	struct cmd **list;
	int i;

	if (cmd == NULL || cmd->name == NULL)
		return;
	for (i = 0; i < indent; i++)
		printf("\t");
	printf("%s\n", cmd->name);
	list = cmd->next_cmds;
	for (i = 0; i < 8; i++)
		cmd_print(list[i], indent + 1);
}

static void
print_indent(int j)
{

	while (j--)
		printf("\t");
}

static void
cmd_printX(struct cmd **list, int indent)
{
	struct cmd *cmd = NULL;
	int i= 0;

	if (list == NULL || *list == NULL)
		return;

	for (i = 0; i < 8; i++) {
		cmd = list[i];
		if (cmd == NULL || cmd->name == NULL)
			break;
		print_indent(indent);
		printf("%s\n", cmd->name);
		cmd_printX(cmd->next_cmds, indent + 1);
	}
}


static void
cmd_list_foreach(struct cmd **list, void (*callback)(struct cmd *, void *arg), void *arg)
{
	struct cmd *cmd = NULL;
	int i= 0;

	assert(callback != NULL);
	if (list == NULL || *list == NULL)
		return;

	for (i = 0; i < 8; i++) {
		cmd = list[i];
		if (cmd == NULL || cmd->name == NULL)
			break;
		callback(cmd, arg);
		cmd_list_foreach(cmd->next_cmds, callback, arg);
	}
}

void
printer(struct cmd *cmd, void *arg)
{
	int *indent = (int *)arg;

	print_indent(*indent);
	(*(int *)arg)++;
	printf("%s\n", cmd->name);
}

static void
cmd_printXX(struct cmd **list, int indent)
{
	int i = indent;
	cmd_list_foreach(list, printer, &i);
}

int
main(int argc, char **argv)
{

	puts("--------------");
	cmd_print(&cmd_main, 0);
	puts("--------------");
	cmd_printX(cmd_main.next_cmds, 0);
if (0) {
	puts("--------------");
	cmd_printXX(cmd_main.next_cmds, 0);
}
	cmd_dispatch(argc, argv);
	exit(EXIT_SUCCESS);
}
