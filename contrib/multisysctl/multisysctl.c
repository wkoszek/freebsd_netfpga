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

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/module.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/sysctl.h>

static int
ms_handler(SYSCTL_HANDLER_ARGS)
{

	printf("arg1=%p, arg2=%d\n", arg1, arg2);

	return (0);
}

SYSCTL_NODE(_kern, OID_AUTO, multi, CTLFLAG_RD, NULL, "multiple");
SYSCTL_PROC(_kern_multi, OID_AUTO, multi_0, CTLTYPE_OPAQUE|CTLFLAG_RD,
    0, 0, ms_handler, "I", "ms");
SYSCTL_PROC(_kern_multi, OID_AUTO, multi_a, CTLTYPE_OPAQUE|CTLFLAG_RD,
    0, 10, ms_handler, "I", "ms");
SYSCTL_PROC(_kern_multi, OID_AUTO, multi_b, CTLTYPE_OPAQUE|CTLFLAG_RD,
    0, 20, ms_handler, "I", "ms");
SYSCTL_PROC(_kern_multi, OID_AUTO, multi_c, CTLTYPE_OPAQUE|CTLFLAG_RD,
    0, 30, ms_handler, "I", "ms");
SYSCTL_PROC(_kern_multi, OID_AUTO, multi_d, CTLTYPE_OPAQUE|CTLFLAG_RD,
    0, 40, ms_handler, "I", "ms");

static int
mod_event(struct module *module, int cmd, void *arg)
{
	int error = 0;

	switch (cmd) {
	case MOD_LOAD :
		break;
	case MOD_UNLOAD :
		break;
	default :
		error = EOPNOTSUPP;
		break;
	}
	return (error);
}

static moduledata_t mod_data = {
	"multisysctl",
	mod_event,
	NULL,
};

DECLARE_MODULE(multisysctl, mod_data, SI_SUB_EXEC, SI_ORDER_ANY);
