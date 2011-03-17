/*
 * Copyright (C) 2011 Instituto Nokia de Tecnologia
 *
 * Author:
 *     Paulo Alcantara <paulo.alcantara@openbossa.org>
 *     Aloisio Almeida Jr <aloisio.almeida@openbossa.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111EXIT_FAILURE307, USA.
*/

#include <stdlib.h>
#include <errno.h>

#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>

#include <linux/nfc.h>

#include "nfcctl.h"

int verbose;

#define printdbg(s, ...)						\
	do {								\
		if (verbose)						\
			fprintf(stderr, "%s:%d %s: " s "\n",		\
					__FILE__, __LINE__,		\
					__func__, ##__VA_ARGS__);	\
	} while (0)

static int nlerr2syserr(int err)
{
	switch (abs(err)) {
	case NLE_BAD_SOCK:
		return EBADF;
	case NLE_EXIST:
		return EEXIST;
	case NLE_NOADDR:
		return EADDRNOTAVAIL;
	case NLE_OBJ_NOTFOUND:
		return ENOENT;
	case NLE_INTR:
		return EINTR;
	case NLE_AGAIN:
		return EAGAIN;
	case NLE_INVAL:
		return EINVAL;
	case NLE_NOACCESS:
		return EACCES;
	case NLE_NOMEM:
		return ENOMEM;
	case NLE_AF_NOSUPPORT:
		return EAFNOSUPPORT;
	case NLE_PROTO_MISMATCH:
		return EPROTONOSUPPORT;
	case NLE_OPNOTSUPP:
		return EOPNOTSUPP;
	case NLE_PERM:
		return EPERM;
	case NLE_BUSY:
		return EBUSY;
	case NLE_RANGE:
		return ERANGE;
	default:
		return err;
	}
}

int nfcctl_init(struct nfcctl *ctx)
{
	int rc;

	ctx->nlsk = nl_socket_alloc();
	if (!ctx->nlsk) {
		printdbg("Invalid context");
		return -ENOMEM;
	}

	rc = genl_connect(ctx->nlsk);
	if (rc) {
		rc = -nlerr2syserr(rc);
		printdbg("Error connecting to generic netlink: %s",
								strerror(rc));
		goto free_nlsk;
	}

	ctx->nlfamily = genl_ctrl_resolve(ctx->nlsk, NFC_GENL_NAME);
	if (ctx->nlfamily < 0) {
		rc = -nlerr2syserr(ctx->nlfamily);
		printdbg("Error resolving genl NFC family: %s", strerror(rc));
		goto free_nlsk;
	}

	return 0;

free_nlsk:
	nl_socket_free(ctx->nlsk);
	return rc;
}

void nfcctl_deinit(struct nfcctl *ctx)
{
	if (!ctx->nlsk)
		return;

	nl_socket_free(ctx->nlsk);
	ctx->nlsk = NULL;
}
