/*-
 * Copyright (c) 2008-2009 Wojciech A. Koszek <wkoszek@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copy__cla_right
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copy__cla_right
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "cla.h"

static int	cla_debug __unused = 1;
static int	_cla_allocated = 0;
#define DEBUG		if (cla_debug) printf
#define DEBUGGING	if (cla_debug)
#define	MAX(a, b)	(((a) > (b)) ? (a) : (b))
#define	MIN(a, b)	(((a) < (b)) ? (a) : (b))
#define	ARRAY_SIZE(x)	(sizeof((x))/sizeof((x)[0]))
#define CLA_LEN_MAX	128

static int cla_cb_null(struct cla *cmd, int argc, char **argv) __unused;
static int cla_cb_rgt(struct cla *cmd, int argc, char **argv) __unused;

/*
 * Allocate command
 */
static struct cla *
_cla_alloc(void)
{
	struct cla *cmd;
	
	cmd = calloc(1, sizeof(*cmd));
	ASSERT(cmd != NULL && "not enough free memory");
	cla_init(cmd);
	DEBUGGING {
		_cla_allocated++;
	}
	return (cmd);
}

/*
 * Free command
 */
static void
_cla_free(struct cla *cmd)
{

	DEBUGGING {
		ASSERT(cmd != NULL && "you want to free NULL cla");
		_cla_allocated--;
	}
	free(cmd);
}

static struct cla *
cla_err_fmt(struct cla *cla, const char *func, int line, const char *fmt, va_list va)
{
	struct cla_err *e;

	cla_assert(cla);
	e = &cla->__cla_err;
	/* 
	 * If there's an error message, keep it and let the error
	 * message propagate to the upper layers.
	 */
	if (e->clae_msg[0] == '\0') {
		(void)vsnprintf(e->clae_msg, sizeof(e->clae_msg), fmt, va);
		(void)snprintf(e->clae_src, sizeof(e->clae_src), "%s(%d)",
		    func, line);
	}
	return (cla);
}

struct cla *
_cla_err(struct cla *cla, const char *func, int line, const char *fmt, ...)
{
	va_list va;

	cla_assert(cla);
	va_start(va, fmt);
	cla_err_fmt(cla, func, line, fmt, va);
	va_end(va);
	return (cla);
}

int
_cla_erri(struct cla *cla, const char *func, int line, const char *fmt, ...)
{
	va_list va;

	cla_assert(cla);
	va_start(va, fmt);
	(void)cla_err_fmt(cla, func, line, fmt, va);
	va_end(va);

	return (-line);
}

const char *
cla_errmsg(struct cla *cla)
{

	return (cla->__cla_err.clae_msg);
}

/*
 * Create a command.
 *
 * func		holds function handler
 * desc		description of a command
 * fmt		printf(3)-compatible format string
 * va		arguments.
 *
 * Returns NULL if (1) NULL function but non-NULL argument was passed
 * (2) no name specification has been supplied
 */
struct cla *
cla_new_fmt(cla_func_t *func, const char *desc, const char *fmt, va_list va)
{
	struct cla *cmd;
	char buf[CLA_LEN_MAX];
	int l;

	ASSERT(fmt != NULL);
	cmd = _cla_alloc();
	ASSERT(cmd != NULL);
	l = vsnprintf(buf, sizeof(buf), fmt, va);
	ASSERT(l >= 0 && l <= sizeof(buf) - 1);
	cmd->cla_name = strdup(buf);
	ASSERT(cmd->cla_name != NULL);
	if (desc == NULL)
		cmd->cla_desc = strdup("(null)");
	else
		cmd->cla_desc = strdup(desc);

	cmd->cla_func = func;
	cmd->__cla_softc = NULL;

	return (cmd);
}

/*
 * Command creation happens here (user edition).
 */
struct cla *
cla_new(cla_func_t *func, const char *desc, const char *fmt, ...) 
{
	struct cla *cmd;
	va_list va;

	va_start(va, fmt);
	cmd = cla_new_fmt(func, desc, fmt, va);
	va_end(va);
	return (cmd);
}

/*
 * Create node command.
 */
struct cla *
cla_new_node(const char *name, ...)
{
	va_list va;
	struct cla *cmd;

	ASSERT(name != NULL);
	va_start(va, name);
	cmd = cla_new_fmt(NULL, NULL, name, va);
	va_end(va);
	return (cmd);
}

/*
 * Create empty command (useful for regression tests)
 */
struct cla *
cla_new_empty(void)
{

	return (cla_new(NULL, NULL, ""));
}

/*
 * Destroy previously allocated command.
 */
void
cla_destroy(struct cla *cmd)
{
	char *s;

	if (cmd == NULL)
		return;

	cla_destroy(cmd->__cla_left);
	cla_destroy(cmd->__cla_right);

	cla_assert(cmd);
	if (cmd->cla_name != NULL) {
		s = CLA_DECONST(cmd, cla_name);
		memset(s, _CLA_MEMFILL, strlen(s));
		free(s);
	}
	if (cmd->cla_desc != NULL) {
		s = CLA_DECONST(cmd, cla_desc);
		memset(s, _CLA_MEMFILL, strlen(s));
		free(s);
	}
	memset(cmd, _CLA_MEMFILL, sizeof(*cmd));
	_cla_free(cmd);
}

/*--------------------------------------------------------------------------*/

/*
 * Create command belonging to the same hierarchy within a UI instance.
 */
void
cla_add_cmd(struct cla *root, struct cla *cmd)
{
	struct cla *parent, *child;

	ASSERT(root != NULL);
	ASSERT(cmd != NULL);
	CLA_ASSERT(cmd);
	/*
	 * We start from the __cla_right sibling, since main commands (those
	 * of the same hierarchy) are kept there and we try to remember last
	 * non-empty cla.
	 */
	parent = root;
	child = root->__cla_right;
	while (child != NULL) {
		parent = child;
		child = parent->__cla_right;
	}
	/* We stop once there's no more 'right' leafs */
	ASSERT(child == NULL);
	/* At least we have to have a parent -- 'root' */
	ASSERT(parent != NULL);
	/* And there has to be no other commands in a 'parent' */
	ASSERT(parent->__cla_right == NULL);

	/* Add new cla to first free slot */
	parent->__cla_right = cmd;
	cmd->__cla_parent = parent;
}

/*
 * (1) For a given leaf command 'cla' and subcommand 'subcmd' to it.
 * (2) For node command that already has subcommands, push 'subcmd' at
 *     the end of it's list.
 * 
 * (1) cla_add_subcmd(cmd, subcmd);  
 *                 /
 *                /
 *            subcmd
 *
 * (2) cla_add_subcmd(cmd, subcmd2); 
 *                 /
 *                /
 *              subcmd
 *                 \
 *                  \
 *                 subcmd2
 */
void
cla_add_subcmd(struct cla *cmd, struct cla *subcmd)
{

	cla_assert(cmd);
	cla_assert(subcmd);

	if (cmd->__cla_left == NULL) {
		/* 
		 * This is the very first subcommand
		 */
		cmd->__cla_left = subcmd;
		subcmd->__cla_parent = cmd;
	} else {
		/*
		 * subcmd2 from the picture below is in the same
		 * hierarchy as subcmd, so treat subcmd2 as next command
		 * of subcmd.
		 */
		cla_add_cmd(cmd->__cla_left, subcmd);
	}
}

static int
_cla_name_len(struct cla *cmd)
{
	const char *name;
	char *space;
	int len;

	name = cmd->cla_name;

	space = strchr(name, ' ');
	if (space == NULL)
		space = strchr(name, '\t');
	if (space == NULL)
		len = strlen(name);
	else
		len = space - name;
	return (len);
}

/*
 * Match the argument name to the first part of the command name -- the
 * one before first whitespace character.
 */
static int
_cla_name_match(struct cla *cmd, const char *arg)
{
	const char *name;
	int clen;
	int alen;
	int minl;

	cla_assert(cmd);
	ASSERT(arg != NULL);

	name = cla_get_name(cmd);
	clen = _cla_name_len(cmd);
	alen = strlen(arg);
	if (clen != alen)
		return (0);
	minl = MIN(clen, alen);
	/* Check, if body of a command matches */
	if (strncmp(name, arg, minl) != 0)
		return (0);
	/* Argument must match exactly */
	if (arg[minl] != '\0')
		return (0);
	return (1);
}

/*
 * Perform lookup of commands argv, starting from the 'root'
 * Remember that it's not 'main()' -- we start dispatching from
 * argv[0] here.
 */
struct cla *
cla_lookup(struct cla *root, int argc, char **argv, int *rargidx, int *rdepth)
{
	struct cla *cmdcur = NULL;
	struct cla *cmdmatch = NULL;
	int ai;
	int nomorearg;
	int lastcmd;
	int namematch;
	int depth;

	cla_assert(root);
	ASSERT(rargidx != NULL);
	ASSERT(rdepth != NULL);

	depth = 0;
	for (ai = 0, cmdcur = root;;) {
		nomorearg = (ai >= argc);
		lastcmd = (cmdcur == NULL);
		if (nomorearg || lastcmd)
			break;
		ASSERT(cmdcur->cla_name != NULL);
		namematch = (_cla_name_match(cmdcur, argv[ai]) == 1);
		/*
		 * If there isn't a matching command, it means current
		 * argument might point to the next command. Otherwise,
		 * we move on to processing the later arguments.
		 */
		if (!namematch) {
			cmdcur = cmdcur->__cla_right;
			continue;
		} else {
			cmdmatch = cmdcur;
			cmdcur = cmdcur->__cla_left;
			depth++;
			ai++;
		}
	}
	if (cmdmatch != NULL) {
		*rargidx = ai - 1;
		*rdepth = depth - 1;
	}
	return (cmdmatch);
}

/*
 * Return fully qualified command length.
 */
static __inline int
cla_fqcn_len(struct cla *cmd, int l)
{
	int totall, ll, rl;

	if (cmd == NULL)
		return (0);
	totall = strlen(cla_get_name(cmd)) - 1;
#if 0
	totall = _cla_name_len(cmd) - 1;
#endif
	totall += l;
	if (cla_is_leaf(cmd))
		return (totall);
	ll = cla_fqcn_len(cmd->__cla_left, totall);
	rl = cla_fqcn_len(cmd->__cla_right, totall);
	return (MAX(ll, rl));
}

/*
 * Walk the command tree the way up and return the top-most (parent)
 * command
 *
 * While walking, remember the path and depth we came from and return
 * those as 'o_path', it's length -- 'o_pathlen' and 'o_depth' respectively.
 */
static struct cla *
cla_root_get(struct cla *cmd, unsigned long *o_path, int *o_pathlen, int *o_depth)
{
	struct cla *curcmd, *subcmd, *root;
	int depth, i, path, pathlen;

	/* Nothing can be missing. */
	ASSERT(cmd != NULL);

	/* We start from the passed 'cla' */
	curcmd = subcmd = root = cmd;
	depth = i = path = pathlen = 0;

	while (curcmd->__cla_parent != NULL) {
		/*
		 * Make further walk possible.
		 */
		subcmd = curcmd;
		curcmd = root = curcmd->__cla_parent;
		
		/*
		 * Check, how to reach us. In the path, 0 = left, 1 =
		 * right.
		 */
		if (subcmd == curcmd->__cla_left) {
			path &= ~(1 << pathlen);
			/*
			 * Depth changes only for subcommands.
			 */
			depth++;
		} else if (subcmd == curcmd->__cla_right) {
			path |= (1 << pathlen);
		} else {
			/* 
			 * This is bad internal error. Shouldn't
			 * happen
			 */
			abort();
		}
		pathlen++;
	}

	if (o_pathlen != NULL)
		*o_pathlen = pathlen;
	if (o_path != NULL)
		*o_path = path;
	if (o_depth != NULL)
		*o_depth = depth;
	return (root);
}

static void
cla_usage_one(struct cla *cmd, FILE *fp, int maxlen)
{
	struct cla *root, *curcmd;
	unsigned long path;
	int depth, pathlen;
	int i;
	int wbytes;

	path = 0;
	depth = 0;
	pathlen = 0;
	curcmd = NULL;
	wbytes = 0;

	/*
	 * We have a command, and we try to get back to the root of a
	 * tree. This is done to get a "path".
	 */
	ASSERT(cmd != NULL);
	root = cla_root_get(cmd, &path, &pathlen, &depth);
	ASSERT(root != NULL);

	/*
	 * Once we have a path, we try to walk down the tree, starting
	 * from root to gather names of the commands on our road.
	 */
	fprintf(fp, "\t");
	curcmd = root;
	for (i = pathlen - 1; i >= 0; i--) {
		ASSERT(curcmd != NULL && curcmd->cla_name != NULL);
		if ((path & (1 << i)) != 0) {
			curcmd = curcmd->__cla_right;
		} else {
			wbytes += fprintf(fp, "%s ", curcmd->cla_name);
			curcmd = curcmd->__cla_left;
		}
	}
	/*
	 * Now, we should be in the very same command we started from
	 * We can now print final name and description string.
	 */
	ASSERT(curcmd == cmd);
	wbytes += fprintf(fp, "%s", cmd->cla_name);
	ASSERT(wbytes >= 0);
	maxlen -= wbytes;
	ASSERT(maxlen >= 0);
	while (maxlen--)
		fprintf(fp, " ");
	fprintf(fp, "  %s\n", cmd->cla_desc);
}

/*
 * Recursive helper for printing usage of the whole command tree.
 */
static void
_cla_usage(struct cla *cmd, FILE *fp, int maxlen)
{
#if 0
	struct cla *curcmd;
#endif

	if (cmd != NULL) {
		if (!cla_has_subcmds(cmd))
			cla_usage_one(cmd, fp, maxlen);
		_cla_usage(cmd->__cla_left, fp, maxlen);
		_cla_usage(cmd->__cla_right, fp, maxlen);
#if 0
		for (curcmd = cmd->__cla_right; curcmd != NULL;
		    curcmd = curcmd->__cla_right)
			_cla_usage(curcmd, fp, maxlen);
		if (cmd->__cla_left != NULL)
			_cla_usage(cmd->__cla_left, fp, maxlen);
#endif
	}
}

/*
 * Print usage for the whole command tree to the stream fp.
 */
void
cla_usage_fp(struct cla *cmd, FILE *fp)
{
	struct cla *root;
	int maxlen;

	cla_assert(cmd);
	if (cmd->__cla_parent == NULL)
		root = cmd;
	else
		root = cla_root_get(cmd, NULL, NULL, NULL);
	ASSERT(root != NULL);
	maxlen = cla_fqcn_len(root, 0);
	_cla_usage(cmd, fp, maxlen);
}

/*
 * Print usage for the whole command tree.
 */
void
cla_usage(struct cla *cmd)
{

	cla_assert(cmd);
	cla_usage_fp(cmd, stdout);
}

/*
 * Dispatch commands based on argc/argv.
 */
int
cla_dispatch(struct cla *root, const char *usage, int argc, char **argv, int opts)
{
	struct cla *lastmcmd;
	int callfunc;
	int showusage;
	int depth;
	int error;
	int idx;

	cla_assert(root);
	ASSERT(opts != 0 && "specify what happens for `node` command");
	ASSERT((opts & (CLADIS_NODE_CALL|CLADIS_NODE_USAGE)) !=
	    (CLADIS_NODE_CALL|CLADIS_NODE_USAGE));

	/* What to do if we find 'node' command? */
	callfunc = ((opts & CLADIS_NODE_CALL) != 0);
	showusage = ((opts & CLADIS_NODE_USAGE) != 0);
	error = -1;
	idx = -1;

	/*
	 * Find out the best match for argv[0..argc-1]. In return, get
	 * the last matched command, it's possition and depth, in which
	 * it's lying.
	 *
	 * We also print usage if there's no parent, since this is what
	 * probably user wants when first command in the hierarchy
	 * corresponds to the program name passed in argv[0].
	 */
	lastmcmd = cla_lookup(root, argc, argv, &idx, &depth);
	if (lastmcmd == NULL ||
	    (lastmcmd != NULL && lastmcmd->__cla_parent == NULL && showusage)) {
		/*
		 * Even first command didn't match. Return if no usage
		 * needs to be printed.
		 */
		if (!showusage)
		    	return (CLA_ENOCMD);

		if (usage != NULL)
			printf("%s", usage);
		cla_usage(root);
		return (0);
	}

	/* 
	 * Ok, so there was something found -- if it's a `node command',
	 * show usage for it, but only if we need. Return success.
	 */
	if (cla_has_subcmds(lastmcmd) && showusage) {
		cla_usage(lastmcmd->__cla_left);
		return (0);
	}

	/* 
	 * We're in 'node or lastmcmd' mode. We always execute lastmcmd
	 * commands. Node command will get executed in CLADIS_NODE_FUNC
	 * case.
	 */
	if (!cla_has_subcmds(lastmcmd))
		callfunc++;
	if (callfunc) {
#if 0 && !CHECK_6x
		idx--;
#endif
		if (lastmcmd->cla_func == NULL)
			return (CLA_ENOFUNC);
		error = lastmcmd->cla_func(lastmcmd, argc - idx, argv + idx);
		if (error != 0)
			/*
			 * Error messages are present in command's error
			 * buffer, but we'll be asking about root's
			 * error buffer later.
			 */
			(void)memcpy(&root->__cla_err, &lastmcmd->__cla_err,
			    sizeof(root->__cla_err));
	}
	return (error);
}

/*
 * Helper for printing tree in hierarchical order.
 */
void
_cla_print_fp(struct cla *cmd, FILE *fp, unsigned int indent)
{
	unsigned int i;

	if (cmd != NULL) {
		for (i = 0; i < indent; i++)
			(void)putchar('\t');
		(void)fprintf(fp, "%s\n", cmd->cla_name);
		_cla_print_fp(cmd->__cla_left, fp, indent + 1);
		_cla_print_fp(cmd->__cla_right, fp, indent);
	}
}

/*
 * Print UI hierarchy to file descriptor 'fp'
 */
void
cla_print_fp(struct cla *cmd, FILE *fp)
{

	cla_assert(cmd);
	_cla_print_fp(cmd, fp, 0);
}

/*
 * Print UI hierarchy to standard output.
 */
void
cla_print(struct cla *cmd)
{

	cla_assert(cmd);
	_cla_print_fp(cmd, stdout, 0);
}

/*
 * Get command's name.
 */
const char *
cla_get_name(struct cla *cmd)
{

	cla_assert(cmd);
	return (cmd->cla_name);
}

/*
 * Get command's description.
 */
const char *
cla_get_desc(struct cla *cmd)
{

	cla_assert(cmd);
	return (cmd->cla_desc);
}

/*
 * Set command's description
 */
void
cla_set_desc(struct cla *cmd, const char *desc)
{
	char *s;

	cla_assert(cmd);
	ASSERT(desc != NULL);
	s = strdup(desc);
	ASSERT(s != NULL);
	if (cmd->cla_desc != NULL)
		free(CLA_DECONST(cmd, cla_desc));
	cmd->cla_desc = s;
}

/*
 * Set command's function argument.
 */
void
cla_set_softc(struct cla *cmd, void *arg)
{

	if (cmd == NULL)
		return;
	cla_assert(cmd);
	cmd->__cla_softc = arg;
	cla_set_softc(cmd->__cla_left, arg);
	cla_set_softc(cmd->__cla_right, arg);
}

/*
 * Get command's function argument.
 */
void *
cla_get_softc(struct cla *cmd)
{

	cla_assert(cmd);
	return (cmd->__cla_softc);
}

/*
 * Set function of a command.
 */
void
cla_set_func(struct cla *cmd, cla_func_t *func)
{

	cla_assert(cmd);
	cmd->cla_func = func;
}

/*
 * XXWKOSZEK: Implement non-recursive version.
 */
static void
_cla_print_dot_fp(struct cla *cmdp, FILE *fp, int dir)
{
	void *currp, *rightp, *leftp;
	const char *name;

	currp = cmdp;
	name = cmdp->cla_name;
	rightp = cmdp->__cla_right;
	leftp = cmdp->__cla_left;

	if (dir == _CLA_DOT_MAIN) {
		printf("digraph tree {\n");
		printf("\troot=n%p\n", currp);
		printf("\t%s\n", CLA_DOT_GRAPH_ATTR);
	}
	if (currp != NULL) {
		/*
		 * Having leftp of rightp being NULL lets us to
		 * terminate the recursion; additionally, we create
		 * different NULL-node for each childer having no futher
		 * siblings in order to generate readable .dot graph.
		 */
		printf("n%p [%s, label=\"%p:%10s\"]\n", currp, CLA_DOT_OK, currp, name);
		if (leftp == NULL) {
			printf("n%plnull [%s]\n", currp, CLA_DOT_NULL_LEFT);
			printf("n%p->n%plnull [label=\"left\"]\n", currp, currp);
		} else {
			printf("n%p->n%p [label=\"left\"]\n", currp, leftp);
			_cla_print_dot_fp(leftp, fp, _CLA_DOT_LEFT);
		}

		if (rightp == NULL) {
			printf("n%prnull [%s]\n", currp, CLA_DOT_NULL_RIGHT);
			printf("n%p->n%prnull [label=\"right\"]\n", currp, currp);
		} else {
			printf("n%p->n%p [label=\"right\"]\n", currp, rightp);
			_cla_print_dot_fp(rightp, fp, _CLA_DOT_RIGHT);
		}
	}
	if (dir == _CLA_DOT_MAIN)
		printf("}\n");
}

/*
 * Print Graphviz-friendly .dot file to 'fp'.
 */
void
cla_print_dot_fp(struct cla *cmd, FILE *fp)
{

	_cla_print_dot_fp(cmd, fp, _CLA_DOT_MAIN);
}

/*
 * Print Graphviz-friendly .dot file to standard output.
 */
void
cla_print_dot(struct cla *cmd)
{

	cla_print_dot_fp(cmd, stdout);
}

/*
 * Does the command have subcommands?
 */
int
cla_has_subcmds(struct cla *cmd)
{

	return (cmd->__cla_left != NULL);
}

/*
 * Does the command have next commands?
 */
int
cla_has_next(struct cla *cmd)
{

	return (cmd->__cla_right != NULL);
}

/*
 * Is the command the last one in the command tree?
 */
int
cla_is_leaf(struct cla *cmd)
{

	return (!cla_has_subcmds(cmd) && !cla_has_next(cmd));
}

/*
 * Helper callbacks.
 */
int
cla_cb_null(struct cla *cmd, int argc, char **argv)
{

	cla_assert(cmd);
	(void)argc;
	(void)argv;
	return (0);
}

static int __unused
cla_cb_rgt(struct cla *cmd, int argc, char **argv)
{
	int i;
	const char *s;

	s = cmd->__cla_softc;

	printf("Func: %s called; Arguments:\n", s);
	puts("--");
	for (i = 0; i < argc; i++) {
#if 0 && !CHECK_6x
		ASSERT(argv[i] != NULL);
#endif
		printf("\targ%d = '%s'\n", i, argv[i]);
	}
	puts("--");
	return (0);
}

#ifdef CLA_STATIC_SUPPORT
/*
 * Pick the way we're about to insert the cla into the tree based on
 * the __cla_flags argument, present in each cla.
 */
static void
__cla_hook(struct cla *cmd, struct cla *subcmd)
{
	cla_assert(cmd);
	cla_assert(subcmd);
	ASSERT(subcmd->__cla_flags != 0);

	if (subcmd->__cla_flags & CLA_FLAG_SIBLING)
		cla_add_subcmd(cmd, subcmd);
	else if (subcmd->__cla_flags & CLA_FLAG_BROTHER)
		cla_add_cmd(cmd, subcmd);
}

/*
 * This works only in static mode!
 *
 * Takes each static definition of a command and tries to link
 * all nodes together in order to create a command tree.
 */
static void
cla_tree_gen(void)
{
	struct cla **cmdl = NULL;
	struct cla *cmd = NULL;

	for (cmdl = CLASECT_START; cmdl <= CLASECT_STOP && *cmdl; cmdl++) {
		cmd = *cmdl;
		if (cmd->__cla_parent != NULL)
			__cla_hook(cmd->__cla_parent, cmd);
	}
}

/*
 * Returns root cla pointer or NULL if an error occured.
 *
 * This works only in static mode. It means that you may want to disable
 * it on Windows.
 */
struct cla *
cla_root_get_static(void)
{
	struct cla **cmdl = NULL;
	struct cla *root = NULL;
	struct cla *cmd = NULL;
	int rootcnt = 0;

	cla_tree_gen();
	for (cmdl = CLASECT_START; cmdl <= CLASECT_STOP && *cmdl; cmdl++) {
		cmd = *cmdl;
		cla_assert(cmd);
		/*
		 * We check cla with "." (indicating that it's a ROOT
		 * cla). We count such command appearances as well.
		 */
		if (cmd->cla_name[0] == '.' && cmd->__cla_parent == NULL) {
			rootcnt++;
			root = cmd;
		}
	}
	/*
	 * Return NULL if more that 1 root node has been found.
	 */
	if (rootcnt != 1)
		return (NULL);
	return (root);
}

/*
 * You need this in order to perform static mode test.
 */
COMMAND_ROOT(root);
COMMAND_BROTHER(root, list, cla_cb_rgt, "root");
COMMAND_BROTHER(root, show, cla_cb_rgt, "root");
COMMAND_BROTHER(root, print, cla_cb_rgt, "root");
COMMAND_SIBLING(root_list, girls, cla_cb_rgt, "root_list_girls");
COMMAND_SIBLING(root_list, boys, cla_cb_rgt, "root_list_boys");
COMMAND_SIBLING(root_list, users, cla_cb_rgt, "root_list_users");
COMMAND_SIBLING(root_list, groups, cla_cb_rgt, "root_list_groups");
COMMAND_SIBLING(root_list_users, all, cla_cb_rgt, "root_list_users_all");
COMMAND_SIBLING(root_list_users, active, cla_cb_rgt, "root_list_users_active");
COMMAND_SIBLING(root_list_users, passive, cla_cb_rgt, "root_list_users_passive");
COMMAND_SIBLING(root_list_groups, my, cla_cb_rgt, "root_list_groups_my");
COMMAND_SIBLING(root_list_groups, global, cla_cb_rgt, "root_list_groups_global");
COMMAND_SIBLING(root_list_groups, local, cla_cb_rgt, "root_list_groups_local");
COMMAND_SIBLING(root_show, s1, cla_cb_rgt, "root_show_s1");
COMMAND_SIBLING(root_show, s2, cla_cb_rgt, "root_show_s2");
COMMAND_SIBLING(root_show, s3, cla_cb_rgt, "root_show_s3");
COMMAND_SIBLING(root_print, comp, cla_cb_rgt, "root_print_comp");
COMMAND_SIBLING(root_print, comp2, cla_cb_rgt, "root_print_comp2");

/*
 * This works only in static mode and is here for debugging.
 *
 * Returns rendered tree;
 */
struct cla *
check_static(void)
{
	struct cla *root;

	root = cla_root_get_static();
	cla_assert(root);
	cla_tree_gen();
	/*
	 * cla_tree_gen() will tie everything together; it'll find the
	 * sibling of the "." (root cla) as well. "." doesn't have much
	 * sense, so we just skip it.
	 */
	root = root->__cla_right;
	cla_assert(root);
	return (root);
}
#endif

#ifdef CLA_TEST_PROG
static int flag_v = 0;

typedef enum {
	TEST_OK,
	TEST_ER
} test_experr_t;
typedef int test_func_t(void);

struct test {
	test_func_t	 *t_func;
	test_experr_t	  t_experr;
	const char	 *t_desc;
	int		 _t_num;
	const char	*_t_name;
};

#define TEST_DECL(fcn, errcode, desc)		\
	static struct test test_##fcn = {	\
		.t_func = (fcn),		\
		.t_experr = (errcode),		\
		.t_desc = (desc),		\
		._t_num = __LINE__,		\
		._t_name = #fcn,		\
	}

static struct cla *
test_tree_build(void)
{
	struct cla *c1, *c2, *c3, *c4, *c5, *c6, *c7;

	c1 = c2 = c3 = c4 = c5 = c6 = c7 = NULL;
	/*
	 *          list
	 *
	 *      users
	 *        active
	 *        passive
	 *    groups
	 *      small
	 *      big
	 */
	c1 = cla_new(cla_cb_rgt, "List.", "list");
	c2 =   cla_new(cla_cb_rgt, "Users.", "users");
	c3 =     cla_new(cla_cb_rgt, "Active.", "active");
	c4 =     cla_new(cla_cb_rgt, "Passive.", "passive");
	c5 =   cla_new(cla_cb_rgt, "Groups.", "groups");
	c6 =     cla_new(cla_cb_rgt, "Small.", "small");
	c7 =     cla_new(cla_cb_rgt, "Big.", "big");

	cla_set_softc(c1, "list");
	cla_set_softc(c2, "list_users");
	cla_set_softc(c3, "list_users_active");
	cla_set_softc(c4, "list_users_passive");
	cla_set_softc(c5, "list_groups");
	cla_set_softc(c6, "list_groups_small");
	cla_set_softc(c7, "list_groups_big");

	cla_add_subcmd(c1, c2);
	  cla_add_subcmd(c2, c3);
	  cla_add_subcmd(c2, c4);
	cla_add_subcmd(c1, c5);
	  cla_add_subcmd(c5, c6);
	  cla_add_subcmd(c5, c7);

	return (c1);
}

static cla_func_t claf_ag_callback;
static int claf_ag_dummy = 10;

static int
claf_ag_callback(struct cla *cmd, int argc, char **argv)
{
	int *dummy;

	cla_assert(cmd);
	dummy = cla_get_softc(cmd);
	ASSERT(dummy == &claf_ag_dummy);
	ASSERT(*dummy == claf_ag_dummy);
	*dummy = _CLA_MEMFILL;
	return (0);
}

static int
test_claf_ag(void)
{
	struct cla *c1;
	int ac;
	char *av[2] = {
		"c1",
		NULL,
	};

	ac = 1;
	c1 = cla_new(claf_ag_callback, NULL, "small");
	cla_set_softc(c1, &claf_ag_dummy);
	c1->cla_func(c1, ac, av);
	ASSERT(claf_ag_dummy == _CLA_MEMFILL);
	cla_destroy(c1);
	return (0);
}
TEST_DECL(test_claf_ag, TEST_OK, "Test cla_function_arg_get()");

static int claf_as_dummy1 = 10;
static int claf_as_dummy2 = 20;
static int claf_as_dummy3 = 30;
static int claf_as_res = 0;

static int
claf_as_callback(struct cla *cmd, int argc, char **argv)
{
	int *dummy;

	cla_assert(cmd);
	dummy = cla_get_softc(cmd);
	if (dummy == NULL)
		return (-1);
	claf_as_res += *dummy;
	return (0);
}

static int
test_claf_as(void)
{
	struct cla *c1;
	int ac;
	int expected;
	int ret;
	char *av[2] = {
		"c1",
		NULL,
	};

	expected =
	    claf_as_dummy1 +
	    claf_as_dummy2 +
	    claf_as_dummy3;

	ac = 1;
	c1 = cla_new(claf_as_callback, NULL, "small");
	ret = c1->cla_func(c1, ac, av);
	ASSERT(ret == -1 && "no argument should lead to failure");

	cla_set_softc(c1, &claf_as_dummy1);
	ret = c1->cla_func(c1, ac, av);
	ASSERT(ret == 0);

	cla_set_softc(c1, &claf_as_dummy2);
	ret = c1->cla_func(c1, ac, av);
	ASSERT(ret == 0);

	cla_set_softc(c1, &claf_as_dummy3);
	ret = c1->cla_func(c1, ac, av);
	ASSERT(ret == 0);

	ASSERT(claf_as_res == expected);

	cla_destroy(c1);
	return (0);
}
TEST_DECL(test_claf_as, TEST_OK, "Test cla_set_softc()");

static int
test_claf_as2(void)
{
	struct cla *c1, *c2, *c3, *c4, *c5;
	char dummy;

	c1 = cla_new_node("c1");
	c2 = cla_new_node("c2");
	c3 = cla_new_node("c3");
	c4 = cla_new_node("c4");
	c5 = cla_new_node("c5");
	ASSERT(c1 != NULL);
	ASSERT(c2 != NULL);
	ASSERT(c3 != NULL);
	ASSERT(c4 != NULL);
	ASSERT(c5 != NULL);

	cla_add_subcmd(c1, c2);
	cla_add_subcmd(c1, c3);
	cla_add_subcmd(c2, c4);
	cla_add_subcmd(c3, c5);

	cla_set_softc(c1, &dummy);

	ASSERT(c1->__cla_softc == &dummy);
	ASSERT(c2->__cla_softc == &dummy);
	ASSERT(c3->__cla_softc == &dummy);
	ASSERT(c4->__cla_softc == &dummy);
	ASSERT(c5->__cla_softc == &dummy);
	return (0);
}
TEST_DECL(test_claf_as2, TEST_OK, "Test cla_set_softc() -- recursive stuff");

static int claf_s_dummy = 0;
static cla_func_t claf_s_callback;

static int
claf_s_callback(struct cla *cmd, int argc, char **argv)
{
	int *dummy;

	dummy = cla_get_softc(cmd);
	ASSERT(dummy != NULL);
	(*dummy)++;
	return (0);
}

static int
claf_s_callback_null(struct cla *cmd, int argc, char **argv)
{

	(void)cmd;
	(void)argc;
	(void)argv;
	return (0);
}

static int
test_claf_s(void)
{
	struct cla *c1;
	int ac;
	int ret;
	char *av[2] = {
		"c1",
		NULL,
	};

	ac = 2;

	ASSERT(claf_s_dummy == 0);
	c1 = cla_new(claf_s_callback, NULL, "small");
	cla_set_softc(c1, &claf_s_dummy);
	ASSERT(c1 != NULL);
	ret = c1->cla_func(c1, ac, av);
	ASSERT(ret == 0);
	ASSERT(claf_s_dummy == 1);
	cla_set_func(c1, claf_s_callback_null);
	ret = c1->cla_func(c1, ac, av);
	ASSERT(ret == 0);
	ASSERT(claf_s_dummy == 1);
	cla_destroy(c1);
	return (0);
}
TEST_DECL(test_claf_s, TEST_OK, "Test cla_new() and cla_destroy()");

/*
 * Test cla_add_cmd() variants.
 */
static int
test_cla_add_cmd(void)
{
	struct cla *ca, *cb, *cc;
	struct cla *c1, *c2, *c3;

	ca = cla_new_empty();
	cb = cla_new_empty();
	cc = cla_new_empty();
	cla_add_cmd(ca, cb);
	cla_add_cmd(cb, cc);
	ASSERT(ca->__cla_right == cb && "wrong linkage in cla_add_cmd(ca, cb)");
	ASSERT(ca->__cla_right->__cla_right == cc &&
	    "wrong linkage in cla_add_cmd(cb, cc)");

	c1 = cla_new_empty();
	c2 = cla_new_empty();
	c3 = cla_new_empty();
	cla_add_cmd(c1, c2);
	cla_add_cmd(c1, c3);
	ASSERT(c1->__cla_right == c2 && "wrong linkage in cla_add_cmd(c1, c2)");
	ASSERT(c2->__cla_right == c3 && "wrong linkage in cla_add_cmd(c2, c3)");

	return (0);
}
TEST_DECL(test_cla_add_cmd, TEST_OK, "Test cla_add_cmd()");

/*
 * Test cla_add_subcmd() functionality
 */
static int
test_cla_add_subcmd(void)
{
	struct cla *c1;
	struct cla *c2;
	struct cla *c3;
	struct cla *c4;
	struct cla *c5;
	struct cla *c6;
	struct cla *c7;

	c1 = c2 = c3 = c4 = c5 = c6 = c7 = NULL;
	/*
	 *                c1
	 *               /
	 *              /
	 *            c2
	 *           /  \
	 *          /    \  
	 *        c3      c5
	 *         \     /  
	 *          c4  c6
	 *               \
	 *                c7
	 */
	c1 = cla_new_empty();
	c2 =   cla_new_empty();
	c3 =     cla_new_empty();
	c4 =     cla_new_empty();
	c5 =   cla_new_empty();
	c6 =     cla_new_empty();
	c7 =     cla_new_empty();
	cla_add_subcmd(c1, c2);
	  cla_add_subcmd(c2, c3);
	  cla_add_subcmd(c2, c4);
	cla_add_subcmd(c1, c5);
	  cla_add_subcmd(c5, c6);
	  cla_add_subcmd(c5, c7);
#define R(c)	((c)->__cla_right)
#define L(c)	((c)->__cla_left)
#define P(c)	((c)->__cla_parent)

	/* c1 <-> c2 */
	ASSERT(R(c1) == NULL);
	ASSERT(L(c1) == c2);
	ASSERT(P(c2) == c1);

	/* c2 <-> c3 && c2 <-> c5 */
	ASSERT(L(c2) == c3);
	ASSERT(R(c2) == c5);

	/* c3 <-> c2 && c3 <-> c4 */
	ASSERT(P(c3) == c2);
	ASSERT(R(c3) == c4);
	ASSERT(L(c3) == NULL);

	/* c4 <-> c3 && leaf test */
	ASSERT(P(c4) == c3);
	ASSERT(L(c4) == NULL);
	ASSERT(R(c4) == NULL);

	/* c5 <-> c2 && c5 <-> c6 */
	ASSERT(P(c5) == c2);
	ASSERT(R(c5) == NULL);
	ASSERT(L(c5) == c6);

	/* c6 <-> c5 && c6 <-> c7 */
	ASSERT(P(c6) == c5);
	ASSERT(R(c6) == c7);
	ASSERT(L(c6) == NULL);

	/* c7 <-> c6 && leaf test */
	ASSERT(P(c7) == c6);
	ASSERT(R(c7) == NULL);
	ASSERT(L(c7) == NULL);
#undef P
#undef L
#undef R
	return (0);
}
TEST_DECL(test_cla_add_subcmd, TEST_OK, "Test cla_add_subcmd()");

/*
 * Test cla_lookup
 */
static int
test_cla_lookup(void)
{
	struct cla *tree;
	struct cla *cmd;
	int idx = -1;
	char *av[10];
	int ac;
	int depth;

	/*
	 *      list->{users|groups}
	 *      users->{active|passive}
	 *      groups->{big|small}
	 */
	tree = test_tree_build();
	ASSERT(tree != NULL);

	av[0] = "liZT";
	av[1] = NULL;
	ac = 1;
	idx = depth = -1;
	cmd = cla_lookup(tree, ac, av, &idx, &depth);
	ASSERT(cmd == NULL);
	ASSERT(idx == -1);
	ASSERT(depth == -1);

	av[0] = "list";
	av[1] = NULL;
	ac = 1;
	idx = depth = -1;
	cmd = cla_lookup(tree, ac, av, &idx, &depth);
	ASSERT(cmd != NULL);
	ASSERT(strcmp(cla_get_name(cmd), "list") == 0);
	ASSERT(idx == 0);
	ASSERT(depth == 0);
	
	av[0] = "list";
	av[1] = "nic";
	av[2] = "NULL";
	ac = 2;
	idx = depth = -1;
	cmd = cla_lookup(tree, ac, av, &idx, &depth);
	ASSERT(cmd != NULL);
	ASSERT(strcmp(cla_get_name(cmd), "list") == 0);
	ASSERT(idx == 0);
	ASSERT(depth == 0);

	av[0] = "list";
	av[1] = "nic";
	av[2] = NULL;
	ac = 2;
	idx = depth = -1;
	cmd = cla_lookup(tree, ac, av, &idx, &depth);
	ASSERT(cmd != NULL);
	ASSERT(strcmp(cla_get_name(cmd), "list") == 0);
	ASSERT(idx == 0);
	ASSERT(depth == 0);

	cla_destroy(tree);
	return (0);
}
TEST_DECL(test_cla_lookup, TEST_OK, "Test cla_lookup()");

/*
 * Tests cla_fqcn_len()
 */
static int
test_cla_fqcn_len(void)
{
	struct cla *tree;
	int len;
	int exlen;

	len = exlen = 0;
	/*
	 * Checks, if cla_lookup() works. Tree is:
	 *      list  ->{ users|groups}
	 *      users ->{active|passive}
	 *      groups->{   big|small}
	 */
	tree = test_tree_build();
	cla_usage(tree);
	len = cla_fqcn_len(tree, 0);
	exlen += strlen("list");
	exlen += strlen("users");
	exlen += strlen("passive");
	exlen += 2; /* spaces */
	if (len != exlen) {
		printf("len=%d exlen=%d\n", len, exlen);
		ASSERT(len == exlen);
	}
	cla_destroy(tree);
	return (0);
}
TEST_DECL(test_cla_fqcn_len, TEST_OK, "Test cla_fqcn_len()");

/*
 * Check, if cla_root_get() works.
 */
static int
test_cla_root_get(void)
{
	struct cla *tree;
	struct cla *passive;
	struct cla *root;
	unsigned long path;
	int pathlen, depth;

	/*
	 * Checks, if we can actually get root of the command tree. Tree
	 * looks like:
	 *      list->{users|groups}
	 *      users->{active|passive}
	 *      groups->{big|small}
	 */
	tree = test_tree_build();
	passive = tree->__cla_left->__cla_left->__cla_right;

	path = 0;
	pathlen = depth = -1;
	root = cla_root_get(tree, &path, &pathlen, &depth);
	ASSERT(root == tree);
	ASSERT(path == 0);
	ASSERT(pathlen == 0);
	ASSERT(depth == 0);

	path = 0;
	pathlen = depth = -1;
	root = cla_root_get(passive, &path, &pathlen, &depth);
	ASSERT(root == tree);
	cla_print(root);
	ASSERT(path == 0x1);
	ASSERT(pathlen == 3);
	ASSERT(depth == 2);
	cla_destroy(tree);
	return (0);
}
TEST_DECL(test_cla_root_get, TEST_OK, "Test cla_root_get()");

static int
test_cla_dispatch(void)
{
	struct cla *tree;
	char *av[10];
	int ac;
	int error;

	tree = test_tree_build();

	av[0] = "list";
	av[1] = "dummy";
	av[2] = NULL;
	ac = 2;
	error = cla_dispatch(tree, "usage: test7a", ac, av, CLADIS_NODE_CALL);
	ASSERT(error == 0);

	av[0] = "test7";
	av[1] = NULL;
	ac = 1;
	error = cla_dispatch(tree, "test7a", ac, av, CLADIS_NODE_CALL);
	ASSERT(error != 0);
	cla_print(tree);

	av[0] = "LIST";
	av[1] = NULL;
	ac = 1;
	error = cla_dispatch(tree, "test7b", ac, av, CLADIS_NODE_CALL);
	ASSERT(error != 0);
	cla_print(tree);

	av[0] = "list";
	av[1] = "users";
	av[2] = NULL;
	ac = 2;
	error = cla_dispatch(tree, "test7c", ac, av, CLADIS_NODE_CALL);
	ASSERT(error == 0);
	cla_print(tree);

	av[0] = "list";
	av[1] = "nic";
	av[2] = NULL;
	ac = 2;
	error = cla_dispatch(tree, "test7d", ac, av, CLADIS_NODE_CALL);
	ASSERT(error == 0);
	cla_print(tree);

	cla_destroy(tree);
	return (0);
}
TEST_DECL(test_cla_dispatch, TEST_OK, "Test cla_dispatch()");

/*
 * Check, if cla_usage_one() works; XXWKOSZEK: ToDO
 */
static int
test_cla_usage_one(void)
{

	return (0);
}
TEST_DECL(test_cla_usage_one, TEST_OK, "Test test_cla_usage_one()");

/*
 * Check, if cla_usage() works.
 */
static int
test_cla_usage(void)
{

	return (0);
}
TEST_DECL(test_cla_usage, TEST_OK, "Test cla_usage()");

#define TEST2_EXPECTED_PRINT_RESULT 	\
"list\n"				\
"	users\n"			\
"		active\n"		\
"		passive\n"		\
"	groups\n"			\
"		small\n"		\
"		big\n"
/*
 * Check, if cla_print() works.
 */
static int
test_cla_print(void)
{
	char buf[1024 * 10];
	char *bp;
	struct cla *tree;
	int alloced;

	alloced = _cla_allocated;
	tree = test_tree_build();
	/*
	 * This can actually fail--depends on printf()
	 * implementations
	 */
	fflush(stdout);
	memset(buf, 0, sizeof(buf));
	setbuf(stdout, buf);
	cla_print_fp(tree, stdout);
	bp = strdup(buf);
	ASSERT(bp != NULL);
	setbuf(stdout, NULL);
	ASSERT(strcmp(bp, TEST2_EXPECTED_PRINT_RESULT) == 0);
	cla_destroy(tree);
	ASSERT(_cla_allocated == alloced);
	return (0);
}
TEST_DECL(test_cla_print, TEST_OK, "Test cla_print()");

static struct test *tests[] = {
#define	TEST_UNIT(ct)	&test_##ct,
#include "cla_tests.h"
#undef TEST_UNIT
};

/*
 * Run regression tests -- all, if no argument was specified, or
 * particular test, which number is present in argv[0].
 */
int
rtest_main(int argc, char **argv)
{
	struct test *t;
	int tnum = -1;
	int i;
	const char *neg;
	test_experr_t experr;

	if (argc == 1) {
		if (strcmp(argv[0], "all") == 0)
			tnum = -1;
		else
			tnum = atoi(argv[0]);
	}

	for (i = 0; i < ARRAY_SIZE(tests); i++) {
		if (tnum != -1 && tnum != i)
			continue;
		t = tests[i];
		if (flag_v >= 2)
			printf("# running %s test\n", t->_t_name);
		experr = t->t_func();
		if (experr == t->t_experr)
			neg = NULL;
		else
			neg = " not";
		printf("%d%s ok", i, (neg == NULL) ? "" : (neg));
		if (neg != NULL || (neg == NULL && flag_v)) {
			printf(" # Test: %-*s\n", 30, t->_t_name);
		} else {
			printf("\n");
		}
		if (neg != NULL)
			exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}

struct cla *
check_dynamic(void)
{

	return (test_tree_build());
}

int
main(int argc, char **argv)
{
	struct cla *tree;
	int error;
	int flag_Dot;
	int flag_static;
	int flag_r;
	int mode_dispatch;
	char *o;
	int opts;

	tree = NULL;
	error =
	flag_Dot = 
	flag_static =
	flag_r =
	mode_dispatch =
	opts =
	0;
	o = NULL;

	/* Remember, if the program is special and move on */
	if (strstr(argv[0], "cla_dispatch") != NULL)
		mode_dispatch = 1;
	argc -= 1;
	argv += 1;

	/* Because Windows has no unistd.h.. */
	if (argc >= 1)
		if (*argv[0] == '-')
			o = argv[0];


	if (o != NULL) {
		while (*o != '\0') {
			switch (*o) {
			case 'a':
				opts |= CLADIS_NODE_CALL;
				break;
			case 'D':
				flag_Dot++;
				break;
			case 'h':
				fprintf(stderr, "./cla[_dispatch] [-s] [-D] [-r] arg ...\n");
				exit(0);
				break;
			case 'r':
				flag_r++;
				break;
			case 's':
				flag_static++;
				break;
			case 'u':
				opts |= CLADIS_NODE_USAGE;
				break;
			case 'v':
				flag_v++;
				break;
			}
			o++;
		}
		argc -= 1;
		argv += 1;
	}

	if (opts == 0)
		opts = CLADIS_NODE_USAGE;

	if (flag_r)
		rtest_main(argc, argv);

	if (flag_static == 0)
		tree = check_dynamic();
#ifdef CLA_STATIC_SUPPORT
	else
		tree = check_static();
#endif /* CLA_STATIC_SUPPORT */

	cla_assert(tree);

	if (mode_dispatch && !flag_Dot) {
		error = cla_dispatch(tree, "program:\n", argc, argv, opts);
		if (error != 0)
			fprintf(stderr, "Error: %s\n", cla_errmsg(cla));
	} else {
		if (flag_Dot)
			cla_print_dot(tree);
		else
			cla_print(tree);
	}

	exit(error);
}
#endif
