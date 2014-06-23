/*-
 * Copyright (c) 2008-2009 Wojciech A. Koszek <wkoszek@FreeBSD.org>
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
#ifndef _CLA_H_
#define _CLA_H_

/*
 * Design and implementation
 * -------------------------
 * 
 * Command hierarchy is represented by the forest. For example:
 * 
 *  'list' -+-> 'users'  -+-> 'active'
 *    +     |             |
 *    |     |             +-> 'passive'
 *    |     |
 *    |     |
 *    |     +-> 'staff' -+--> 'admin'
 *    |     |             |
 *    |     |             +-> 'programmer'
 *    |     |
 *    |     |
 *    |     +-> 'objects' +-> 'all'
 *    |                   |
 *    |                   +-> 'small'
 *    |                   |
 *    |                   +-> 'big'
 *    |
 *    +
 *  'show' ---------------+--> 'computers'
 *                        |
 *                        +--> 'cars'
 *
 * There always has to be at least one top-hierarchy command.
 *
 * Each command can have unlimited number of subcommands (siblings),
 * and its companions (brothers).
 *
 * Each command is represented by a name, and may contain obligatory
 * description; such a description might be used later to print
 * complete structure of a command forest together with short help-alike
 * screen.
 *
 * Each command may have function connected to yet. Once a lookup of a
 * particular command name from a forest is finished successfuly,
 * function can be called to involve further processing. Having a
 * function isn't mandatory.
 *
 * cla_new(name, func, arg, opts, desc) creates new command 'name'
 * with function 'func', and function's first argument 'arg'. opts and
 * desc are obligatory and are left for future.
 *
 * cla_add_cmd() ties two commands together in a "brother's" relationsship
 * (equal depth).
 *
 * cla_add_subcmd(a, b) inserts a command to the 'children's' relationship.
 * Command b will land under 'a' in the hierarchy.
 *
 * Central function
 * 
 *	cla_dispatch(struct cla *cla, const char *usage, int argc,
 *	    char **argv, int opts);
 *
 * Tries to match as many arguments argv[0], argv[1], ..., argv[argc]
 * as possible. In case of possitive dispatching, cla's function is
 * called, and CLA_EOK code is returned.
 *
 * 'opts' steer  cla_dispatch() in cases, when as a result of processing node
 * command has been found (eg. "objects" from the picture above).
 * Available behavious is based on following flags:
 *
 * - CLADIS_NODE_USAGE
 *   
 *   if present, will print `usage' string followed by a forest topology of
 *   last matched command from the `cla' forst
 *
 * - CLA_FUNC_ALWAYS
 *
 *   if present, will call node's function for fallback-mode
 *   dispatching.
 *
 * Note that those two flags are exclusive. Those can't exist together.
 *
 * XX: complete it and mantion when each error code is returned.
 */
#include <assert.h>

/*
 * Windows Visual C++ 9.0 compatibility glue.
 */
#ifdef _CHECK_IF_NEEDED
#include <windows.h>
#endif /* _CHECK_IF_NEEDED */

#ifndef EX_USAGE
#define EX_USAGE	64 /* as in UNIX */
#endif

#ifndef __unused
#define __unused
#endif

#ifndef inline
#define inline __inline
#endif

/* Just in case.. */
#define ASSERT		assert

/* Memory fill used just before free'ing memory */
#define	_CLA_MEMFILL	0x55

/*
 * Command structure. 
 *
 * __cla_magic is mostly for debugging purposes to verify that all
 * pointer references are __cla_right.
 *
 * __cla_flags contain __cla_flags that modify command behaviour or behaviour
 * of functions that operate on commands. Flags are mostly present
 * because of the static mode support.
 *
 * __cla_parent, __cla_left and __cla_right are private data for a
 * library and are not supposed to be modified by any other function
 * than those already present in a library.
 *
 * cla_name is the actual name of a command, based on which command
 * dispatching will take place.
 *
 * Two modes for the operation within this library exist:
 *
 * (1) Static mode, in which commands are being hard coded within a
 *     source code, and each command line hierarchy modification
 *     requires application to be recompiled.
 *
 *     It's faster to add command line handling that way. However, it
 *     only works in *NIX systems with GNU C Compiler. It's fairly
 *     easy to bring commands from separate source files that way.
 *
 * (2) Dynamic mode, in which commands are being created dynamically.
 *     This mode should be portable between *NIX/Windows systems without
 *     a problem.
 *
 *     XXWKOSZEK: See what we can do about dynamic loading facility.
 * 
 *
 * Implementation notes
 * ====================
 * - cla_opt_* handling isn't implemented yet.
 * - most of the code is ASSERTized, which is fine on modern computers;
 *
 */
struct cla_opt {
	unsigned int	 co_name;
	unsigned int	 co_type;
	void		*co_data;
};

struct cla;
typedef int cla_func_t(struct cla *cmd, int argc, char **argv);

struct cla_err {
	char	clae_msg[256];
	char	clae_src[64];
};

struct cla {
	/* Debugging */
	unsigned int	  __cla_magic;
#define CLA_MAGIC	((unsigned)0xb47618a2)
#define	CLA_INIT(cla)	((cla)->__cla_magic = CLA_MAGIC)
#define CLA_ASSERT(cla)	ASSERT((cla)->__cla_magic == CLA_MAGIC)

	/* Private */
	unsigned	  __cla_flags;
	struct cla	 *__cla_parent;
	struct cla	 *__cla_left;
	struct cla	 *__cla_right;

	void		 *__cla_softc;
	struct cla_opt	 *__cla_opts;
	unsigned int	  __cla_nopts;
	struct cla_err	  __cla_err;

	/* Public */
	const char	 *cla_name;
	cla_func_t	 *cla_func;
	const char	 *cla_desc;
};
#define	CLA_FLAG_ROOT		(1 << 1)
#define	CLA_FLAG_BROTHER	(1 << 2)
#define	CLA_FLAG_SIBLING	(1 << 3)
#define CLA_FLAG_INITIALIZED	(1 << 4)

#define	CLADIS_NODE_CALL	(1 << 0)
#define	CLADIS_NODE_USAGE	(1 << 1)

/*
 * Since we're going to allocate clas dynamically as well as declare
 * them from the macro, where string will be lying in read-only section
 * and whose attribute will be "const", we need this deconstantiation.
 */
#define CLA_DECONST(cla, member)	\
	((char *)(uintptr_t)(const void *)((cla)->member))

/*
 * Keep this function consistent with the "struct cla" layout.
 */
static inline void
cla_init(struct cla *cmd)
{

	ASSERT(cmd != NULL);
	CLA_INIT(cmd);

	cmd->__cla_flags = CLA_FLAG_INITIALIZED;
	cmd->__cla_parent = 0;
	cmd->__cla_left = 0;
	cmd->__cla_right = 0;

	cmd->__cla_softc = NULL;
	cmd->__cla_opts = NULL;
	cmd->__cla_nopts = 0;
	memset(&cmd->__cla_err, 0, sizeof(cmd->__cla_err));

	cmd->cla_name = NULL;
	cmd->cla_func = NULL;
	cmd->cla_desc = NULL;
}

static inline int
cla_initialized(struct cla *cmd)
{

	ASSERT(cmd != NULL);
	CLA_ASSERT(cmd);
	return ((cmd->__cla_flags & CLA_FLAG_INITIALIZED) != 0);
}

#define cla_assert(cla) do {						\
	ASSERT(cla != NULL && #cla " can't be NULL here");		\
	CLA_ASSERT(cla);						\
	ASSERT(cla_initialized((cla)) == 1);				\
	ASSERT(cla->cla_name != NULL && #cla " can't be NULL here");	\
} while (0)

/*
 * .dot file attributes
 */
#define	CLA_DOT_GRAPH_ATTR	\
    "center=1;"			\
    "bgcolor=gray;"		\
    "style=filled;"
#define	CLA_DOT_NULL	\
    "shape=box,"	\
    "label=NULL,"	\
    "style=filled,"	\
    "color=black,"	\
    "fontcolor=orange"
#define	CLA_DOT_OK	\
    "shape=box,"	\
    "fillcolor=orange,"	\
    "fontcolor=black,"	\
    "color=orange,"	\
    "style=filled"
#define CLA_DOT_NULL_LEFT	CLA_DOT_NULL
#define CLA_DOT_NULL_RIGHT	CLA_DOT_NULL

/*
 * Print UI hierarchy (DOT versiocla)
 */
enum {
	_CLA_DOT_MAIN,
	_CLA_DOT_LEFT,
	_CLA_DOT_RIGHT
};

typedef enum {
	CLA_EOK,
	CLA_ENOCMD,
	CLA_ENOFUNC,
	CLA_ENOLEAF
} cla_err_t;

struct cla *cla_new_fmt(cla_func_t *func, const char *desc, const char *fmt,
    va_list va);
struct cla *cla_new(cla_func_t *func, const char *desc, const char *fmt, ...);
struct cla *cla_new_node(const char *name, ...);
struct cla *cla_new_empty(void);
void  cla_destroy(struct cla *cmd);
void  cla_add_cmd(struct cla *root, struct cla *cmd);
void  cla_add_subcmd(struct cla *cmd, struct cla *subcla);
struct cla *cla_lookup(struct cla *root, int argc, char **argv, int *arg_idx,
    int *depth);
int cla_dispatch(struct cla *root, const char *usage, int argc, char **argv,
    int opts);
const char *cla_get_name(struct cla *cmd);
const char *cla_get_desc(struct cla *cmd);
const char *cla_get_name(struct cla *cmd);
void  cla_set_desc(struct cla *cmd, const char *desc);
void  cla_set_func(struct cla *cmd, cla_func_t *func);
void  cla_set_softc(struct cla *cmd, void *arg);
void *cla_get_softc(struct cla *cmd);
void  cla_print_dot_fp(struct cla *cmd, FILE *fp);
void  cla_print_dot(struct cla *cmd);
int cla_has_subcmds(struct cla *cmd);
int cla_has_next(struct cla *cmd);
int cla_is_leaf(struct cla *cmd);
void  cla_print_fp(struct cla *cmd, FILE *fp);
void  cla_print(struct cla *cmd);
void cla_usage_fp(struct cla *cmd, FILE *fp);
void cla_usage(struct cla *cmd);
struct cla *_cla_err(struct cla *cla, const char *func, int line,
    const char *fmt, ...);
int _cla_erri(struct cla *cla, const char *func, int line,
    const char *fmt, ...);
#define cla_err(cla, ...)					\
	_cla_err((cla), __func__, __LINE__, ## __VA_ARGS__)
#define cla_erri(cla, ...)					\
	_cla_erri((cla), __func__, __LINE__, ## __VA_ARGS__)
const char *cla_errmsg(struct cla *cla);

#ifdef NOTYET
/* next version of the library */
int cla_opt_has(struct cla *cmd, int c);
void *cla_opt_arg(struct cla *cmd, int c);
int cla_flag_set(struct cla *cmd, cla_flag_t flag);
int cla_flag_get(struct cla *cmd, cla_flag_t flag);
#endif /* NOTYET */

#ifdef CLA_STATIC_SUPPORT
struct cla *cla_root_get_static(void);

/*
 * Linker set symbols. In order to make static mode work fine, one
 * needs to provide linker specification file.
 */
extern char __start_clas[];
extern char __stop_clas[];
#define	CLASECT_START	(struct cla **)__start_clas
#define	CLASECT_STOP	(struct cla **)__stop_clas

/*
 * Create static root node.
 */
#define	COMMAND_ROOT(root)					\
	static struct cla __cla_##root = {			\
	    	.__cla_magic = CLA_MAGIC,			\
								\
		.__cla_flags = CLA_FLAG_ROOT,			\
		.__cla_parent = NULL,				\
		.__cla_left = NULL,				\
		.__cla_right = NULL,				\
								\
		.__cla_softc = NULL,				\
		.__cla_opts = NULL,				\
		.__cla_nopts = 0,				\
								\
		.cla_name = ".",				\
		.cla_desc = NULL,				\
		.cla_func = NULL,				\
	};							\
	static void const * const __set_cla_##root 		\
	    __attribute__((section("clas"))) __used = &__cla_##root

/*
 * We can distinguish a "main command" of the same hierarchy as a
 * "brother". We call subcommands of a given command it's "siblings".
 * Each cla can have many brothers, however only one is accessible
 * directly.
 *
 * You're not supposed to use _COMMAND_DECLARE directly.
 */
#define	_COMMAND_DECLARE(root, name, fl, func, arg)		\
	static struct cla __cla_##root##_##name	= {		\
	    	.__cla_magic = CLA_MAGIC,			\
								\
		.__cla_flags = fl,				\
		.__cla_parent = (&__cla_##root),		\
		.__cla_left = NULL,				\
		.__cla_right = NULL,				\
								\
		.__cla_softc = (arg),				\
		.__cla_opts = NULL,				\
		.__cla_nopts = 0,				\
								\
		.cla_name = #name,				\
		.cla_desc = NULL,				\
		.cla_func = (func),				\
	};							\
	static void const * const __set_cla_##root##_##name	\
	    __attribute__((section("clas"))) __used = &__cla_##root##_##name

/*
 * Public macros for main/sub commands definitions.
 */
#define	COMMAND_BROTHER(root, name, func, arg)			\
	_COMMAND_DECLARE(root, name, CLA_FLAG_BROTHER, func, arg)
#define	COMMAND_SIBLING(root, name, func, arg)			\
	_COMMAND_DECLARE(root, name, CLA_FLAG_SIBLING, func, arg)
#endif /* CLA_STATIC_SUPPORT */

static __inline struct cla *
cla_get_nextcmd(struct cla *cmd)
{

	return (cmd->__cla_right);
}

static __inline struct cla *
cla_get_subcmd(struct cla *cmd)
{

	return (cmd->__cla_left);
}

/* Regression tests */
int rtest_main(int argc, char **argv);
struct cla *check_dynamic(void);
struct cla *check_static(void);

#endif /* _CLA_H_ */
