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
#include <unistd.h>

#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>

#include <linux/nfc.h>

#include "nfcctl.h"

#define AF_NFC 39

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

int nfcctl_target_init(struct nfcctl *ctx, uint32_t dev_idx, uint32_t tgt_idx,
							uint32_t protocol)
{
	int fd;
	struct sockaddr_nfc addr;
	int rc;

	printdbg("IN");

	fd = socket(AF_NFC, SOCK_SEQPACKET, NFC_SOCKPROTO_RAW);
	if (fd == -1)
		return errno;

	addr.sa_family = AF_NFC;
	addr.dev_idx = dev_idx;
	addr.target_idx = tgt_idx;
	addr.nfc_protocol = protocol;

	rc = connect(fd, (struct sockaddr *) &addr, sizeof(addr));
	if (rc) {
		rc = errno;
		goto close_sock;
	}

	ctx->target_fd = fd;
	return 0;

close_sock:
	close(fd);
	return rc;
}

struct targets_found_hdl_data {
	tgt_found_handler_t handler;
	void *hdl_param;
};

static int targets_found_handler(struct nl_msg *n, void *arg)
{
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(n));
	struct targets_found_hdl_data *hdl_data = arg;
	struct nlattr *attr[NFC_ATTR_MAX + 1];
	struct nlattr *attr_nest[NFC_TARGET_ATTR_MAX + 1];
	struct nlattr *attr_tgt;
	int rem;
	uint32_t dev_idx;
	struct nfc_target tgt;
	int rc;

	printdbg("IN");

	if (gnlh->cmd != NFC_EVENT_TARGETS_FOUND) {
		printdbg("The received message is not NFC_EVENT_TARGETS_FOUND");
		return NL_SKIP;
	}

	nla_parse(attr, NFC_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);
	if (!attr[NFC_ATTR_TARGETS] || !attr[NFC_ATTR_DEVICE_INDEX]) {
		printdbg("Missing attribute in received message");
		return NL_SKIP;
	}

	dev_idx = nla_get_u32(attr[NFC_ATTR_DEVICE_INDEX]);

	attr_tgt = nla_data(attr[NFC_ATTR_TARGETS]);
	rem = nla_len(attr[NFC_ATTR_TARGETS]);
	nla_for_each_nested(attr_tgt, attr[NFC_ATTR_TARGETS], rem) {
		nla_parse(attr_nest, NFC_TARGET_ATTR_MAX, nla_data(attr_tgt),
				nla_len(attr_tgt), NULL);
		if (!attr_nest[NFC_TARGET_ATTR_TARGET_INDEX] ||
			!attr_nest[NFC_TARGET_ATTR_SUPPORTED_PROTOCOLS]) {
			printdbg("Missing nested attribute in received"
								" message");
			return NL_SKIP;
		}

		tgt.idx = nla_get_u32(attr_nest[NFC_TARGET_ATTR_TARGET_INDEX]);
		tgt.protocols = nla_get_u32(
				attr_nest[NFC_TARGET_ATTR_SUPPORTED_PROTOCOLS]);

		rc = hdl_data->handler(hdl_data->hdl_param, dev_idx, &tgt);
		if (rc == TARGET_FOUND_STOP)
			return NL_STOP;
	}

	return NL_SKIP;
}

static int no_seq_check(struct nl_msg *n, void *arg)
{
	printdbg("IN");

	return NL_OK;
}

int nfcctl_targets_found(struct nfcctl *ctx, tgt_found_handler_t handler,
								void *hdl_param)
{
	struct nl_cb *cb;
	fd_set rfds;
	int sockfd;
	struct targets_found_hdl_data hdl_data;
	int rc;

	printdbg("IN");

	cb = nl_cb_alloc(NL_CB_VERBOSE);
	if (!cb) {
		printdbg("Error allocating struct nl_cb");
		return -ENOMEM;
	}

	hdl_data.handler = handler;
	hdl_data.hdl_param = hdl_param;

	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, no_seq_check, NULL);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, targets_found_handler,
								&hdl_data);

	do {
		FD_ZERO(&rfds);

		sockfd = nl_socket_get_fd(ctx->nlsk);
		FD_SET(sockfd, &rfds);

		rc = select(sockfd + 1, &rfds, NULL, NULL, NULL);
	} while (rc == -1 && errno == EINTR);

	if (rc) {
		if (FD_ISSET(sockfd, &rfds)) {
			rc = nl_recvmsgs(ctx->nlsk, cb);
			if (rc) {
				rc = -nlerr2syserr(rc);
				goto out;
			}
		}
	}

	rc = 0;

out:
	nl_cb_put(cb);
	return rc;
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

int nfcctl_stop_poll(struct nfcctl *ctx, struct nfc_dev *dev)
{
	struct nl_msg *msg;
	void *hdr;
	int rc;

	printdbg("IN");

	msg = nlmsg_alloc();
	if (!msg) {
		printdbg("Error allocating struct nl_msg");
		return -ENOMEM;
	}

	hdr = genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, ctx->nlfamily, 0,
			  NLM_F_REQUEST, NFC_CMD_STOP_POLL, NFC_GENL_VERSION);
	if (!hdr) {
		printdbg("Null header on genlmsg_put()");
		rc = -EINVAL;
		goto nla_put_failure;
	}

	NLA_PUT_U32(msg, NFC_ATTR_DEVICE_INDEX, dev->idx);

	rc = send_and_recv_msgs(ctx, msg, NULL, NULL);

nla_put_failure:
	nlmsg_free(msg);
	return rc;
}

int nfcctl_start_poll(struct nfcctl *ctx, struct nfc_dev *dev,
							uint32_t protocols)
{
	struct nl_msg *msg;
	void *hdr;
	int rc;

	printdbg("IN");

	msg = nlmsg_alloc();
	if (!msg) {
		printdbg("Error allocating struct nl_msg");
		return -ENOMEM;
	}

	hdr = genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, ctx->nlfamily, 0,
			NLM_F_REQUEST, NFC_CMD_START_POLL, NFC_GENL_VERSION);
	if (!hdr) {
		printdbg("Null header on genlmsg_put()");
		rc = -EINVAL;
		goto nla_put_failure;
	}

	NLA_PUT_U32(msg, NFC_ATTR_DEVICE_INDEX, dev->idx);
	NLA_PUT_U32(msg, NFC_ATTR_PROTOCOLS, protocols);

	rc = send_and_recv_msgs(ctx, msg, NULL, NULL);

nla_put_failure:
	nlmsg_free(msg);
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

struct get_multicast_id_hdl_data {
	const char *group;
	int id;
};

static int get_multicast_id_handler(struct nl_msg *msg, void *arg)
{
	struct get_multicast_id_hdl_data *hdl_data = arg;
	struct nlattr *tb[CTRL_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *mcgrp;
	int i;

	printdbg("IN");

	nla_parse(tb, CTRL_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb[CTRL_ATTR_MCAST_GROUPS])
		return NL_SKIP;

	nla_for_each_nested(mcgrp, tb[CTRL_ATTR_MCAST_GROUPS], i) {
		struct nlattr *tb2[CTRL_ATTR_MCAST_GRP_MAX + 1];

		nla_parse(tb2, CTRL_ATTR_MCAST_GRP_MAX, nla_data(mcgrp),
			  nla_len(mcgrp), NULL);
		if (!tb2[CTRL_ATTR_MCAST_GRP_NAME] ||
		    !tb2[CTRL_ATTR_MCAST_GRP_ID] ||
		    strncmp(nla_data(tb2[CTRL_ATTR_MCAST_GRP_NAME]),
			       hdl_data->group,
			       nla_len(tb2[CTRL_ATTR_MCAST_GRP_NAME])))
			continue;

		hdl_data->id = nla_get_u32(tb2[CTRL_ATTR_MCAST_GRP_ID]);
		break;
	};

	return NL_SKIP;
}

static int get_multicast_id(struct nfcctl *ctx, const char *family,
					const char *group)
{
	struct nl_msg *msg;
	void *hdr;
	struct get_multicast_id_hdl_data hdl_data;
	int rc;

	printdbg("IN");

	msg = nlmsg_alloc();
	if (!msg) {
		printdbg("Error allocating struct nl_msg");
		return -ENOMEM;
	}

	hdr = genlmsg_put(msg, 0, 0, genl_ctrl_resolve(ctx->nlsk, "nlctrl"), 0,
			0, CTRL_CMD_GETFAMILY, 0);
	if (!hdr) {
		printdbg("Null header on genlmsg_put()");
		rc = -EINVAL;
		goto nla_put_failure;
	}

	NLA_PUT_STRING(msg, CTRL_ATTR_FAMILY_NAME, family);

	hdl_data.group = group;

	rc = send_and_recv_msgs(ctx, msg, get_multicast_id_handler, &hdl_data);
	if (rc)
		goto nla_put_failure;

	rc = hdl_data.id;

nla_put_failure:
	nlmsg_free(msg);
	return rc;
}

int nfcctl_init(struct nfcctl *ctx)
{
	int id;
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

	id = get_multicast_id(ctx, NFC_GENL_NAME,
					NFC_GENL_MCAST_EVENT_NAME);
	if (id <= 0) {
		rc = id;
		goto free_nlsk;
	}

	rc = nl_socket_add_membership(ctx->nlsk, id);
	if (rc) {
		printdbg("Error adding nl socket to membership");
		rc = -nlerr2syserr(rc);
		goto free_nlsk;
	}

	ctx->target_fd = -1;

	return 0;

free_nlsk:
	nl_socket_free(ctx->nlsk);
	return rc;
}

void nfcctl_deinit(struct nfcctl *ctx)
{
	printdbg("IN");

	if (ctx->target_fd > -1) {
		close(ctx->target_fd);
		ctx->target_fd = -1;
	}

	if (ctx->nlsk)
		nl_socket_free(ctx->nlsk);
		ctx->nlsk = NULL;
}
