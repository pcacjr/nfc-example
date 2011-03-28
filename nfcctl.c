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

static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err,
				void *arg)
{
	int *ret = arg;

	printdbg("IN");

	*ret = err->error;

	return NL_SKIP;
}

static int ack_handler(struct nl_msg *msg, void *arg)
{
	int *ack = arg;

	printdbg("IN");

	*ack = 1;

	return NL_STOP;
}

static int finish_handler(struct nl_msg *msg, void *arg)
{
	int *done = arg;

	printdbg("IN");

	*done = 1;

	return NL_SKIP;
}

static int send_and_recv_msgs(struct nfcctl *ctx, struct nl_msg *msg,
				int (*handler)(struct nl_msg *, void *),
				void *data)
{
	struct nl_cb *cb;
	int err, done, rc;

	printdbg("IN");

	cb = nl_cb_alloc(NL_CB_DEFAULT);
	if (!cb) {
		printdbg("Error allocating struct nl_cb");
		return -ENOMEM;
	}

	rc = nl_send_auto_complete(ctx->nlsk, msg);
	if (rc < 0) {
		rc = -nlerr2syserr(rc);
		printdbg("Error sending netlink message: %s", strerror(rc));
		goto out;
	}

	err = done = 0;

	nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &done);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &done);

	if (handler)
		nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, handler, data);

	while (!err && !done) {
		rc = nl_recvmsgs(ctx->nlsk, cb);
		if (rc) {
			rc = -nlerr2syserr(rc);
			printdbg("Error receiving netlink message: %s",
								strerror(rc));
			goto out;
		}
	}

	rc = -err;
	if (rc)
		printdbg("Error message received: %s", strerror(rc));

out:
	nl_cb_put(cb);
	return rc;
}

struct get_devices_hdl_data {
	struct nfc_dev *devl;
	uint8_t devl_count;
	uint8_t devl_max;
};

static int get_devices_handler(struct nl_msg *n, void *arg)
{
	struct nlmsghdr *nlh = nlmsg_hdr(n);
	struct nlattr *attrs[NFC_ATTR_MAX + 1];
	struct nfc_dev dev;
	struct get_devices_hdl_data *hdl_data = arg;

	printdbg("IN");

	if (hdl_data->devl_count >= hdl_data->devl_max) {
		printdbg("There are discarded NFC devices");
		return NL_STOP;
	}

	genlmsg_parse(nlh, 0, attrs, NFC_ATTR_MAX, NULL);

	if (!attrs[NFC_ATTR_DEVICE_INDEX] || !attrs[NFC_ATTR_DEVICE_NAME]) {
		nl_perror(NLE_MISSING_ATTR, "NFC_CMD_GET_DEVICE");
		return NL_STOP;
	}

	dev.idx = nla_get_u32(attrs[NFC_ATTR_DEVICE_INDEX]);
	dev.name = nla_get_string(attrs[NFC_ATTR_DEVICE_NAME]);
	dev.protocols = nla_get_u32(attrs[NFC_ATTR_PROTOCOLS]);

	hdl_data->devl[hdl_data->devl_count++] = dev;

	return NL_SKIP;
}

int nfcctl_get_devices(struct nfcctl *ctx, struct nfc_dev *devl,
							uint8_t devl_max)
{
	struct nl_msg *msg;
	void *hdr;
	struct get_devices_hdl_data hdl_data;
	int rc;

	printdbg("IN");

	msg = nlmsg_alloc();
	if (!msg) {
		printdbg("Error allocating struct nl_msg");
		return -ENOMEM;
	}

	hdr = genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, ctx->nlfamily, 0,
			  NLM_F_DUMP, NFC_CMD_GET_DEVICE, NFC_GENL_VERSION);
	if (!hdr) {
		printdbg("Null header on genlmsg_put()");
		rc = -EINVAL;
		goto out;
	}

	hdl_data.devl = devl;
	hdl_data.devl_count = 0;
	hdl_data.devl_max = devl_max;

	rc = send_and_recv_msgs(ctx, msg, get_devices_handler, &hdl_data);
	if (rc)
		goto out;

	rc = hdl_data.devl_count;

out:
	nlmsg_free(msg);
	return rc;
}

int nfcctl_init(struct nfcctl *ctx)
{
	int rc;

	printdbg("IN");

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
	printdbg("IN");

	if (!ctx->nlsk)
		return;

	nl_socket_free(ctx->nlsk);
	ctx->nlsk = NULL;
}
