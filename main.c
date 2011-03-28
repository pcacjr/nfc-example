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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/socket.h>

#include "nfcctl.h"
#include "linux/nfc.h"

#define NFC_DEV_MAX 4

extern int verbose;

#define printerr(s, ...)					\
	fprintf(stderr, "%s:%d %s: " s "\n",  __FILE__,		\
			__LINE__, __func__, ##__VA_ARGS__);	\

#define printdbg(s, ...)				\
	do {						\
		if (verbose)				\
			printerr(s, ##__VA_ARGS__);	\
	} while (0)

enum {
	CMD_UNSPEC,
	CMD_LIST_DEVICES,
	CMD_LIST_TARGETS,
};

int cmd;

const struct option lops[] = {
	{ "verbose", no_argument, &verbose, 1 },
	{ "list-devices", no_argument, &cmd, CMD_LIST_DEVICES },
	{ "list-targets", no_argument, &cmd, CMD_LIST_TARGETS },
	{ "protocol", required_argument, NULL, 'p' },
	{ 0, 0, 0, 0 },
};

static void print_devices(const struct nfc_dev *devl, uint8_t devl_count)
{
	unsigned i;

	if (!devl || devl_count == 0)
		return;

	if (devl_count > NFC_DEV_MAX)
		devl_count = NFC_DEV_MAX;

	printf("NFC device list:\n"
		"Index:\tName:\tProtocols:\n");

	for (i = 0; i < devl_count; i++) {
		printf("%d\t%s\t0x%x\n", devl[i].idx, devl[i].name,
					devl[i].protocols);
	}
}

static int init_and_get_devices(struct nfcctl *ctx, struct nfc_dev *devl)
{
	int devl_count;
	int rc;

	rc = nfcctl_init(ctx);
	if (rc) {
		printdbg("%s", strerror(rc));
		return rc;
	}

	rc = nfcctl_get_devices(ctx, devl, NFC_DEV_MAX);
	if (rc < 0) {
		printdbg("%s", strerror(rc));
		return rc;
	}

	devl_count = rc;

	if (!devl_count)
		printf("Info: There isn't any attached NFC device\n");

	return devl_count;
}

static int list_devices(void)
{
	struct nfcctl ctx;
	struct nfc_dev devl[NFC_DEV_MAX];
	uint8_t devl_count;
	int rc;

	rc = init_and_get_devices(&ctx, devl);
	if (rc < 0) {
		printerr("%s", strerror(rc));
		goto out;
	}

	devl_count = rc;
	if (!devl_count)
		goto out;

	print_devices(devl, devl_count);

	rc = 0;
out:
	nfcctl_deinit(&ctx);
	return rc;
}

static int start_poll_all_devices(struct nfcctl *ctx, struct nfc_dev *devl,
				uint32_t devl_count, uint32_t protocols)
{
	int i;
	int rc;

	for (i = 0; i < devl_count; i++) {
		rc = nfcctl_start_poll(ctx, &devl[i], protocols);
		if (rc) {
			rc = nfcctl_stop_poll(ctx, &devl[i]);
			if (rc)
				return rc;

			rc = nfcctl_start_poll(ctx, &devl[i], protocols);
			if (rc)
				return rc;
		}
	}
	return 0;
}

struct print_target_hdl_data {
	uint32_t dev_idx;
	uint32_t tgt_count;
};

static int print_target_handler(void *arg, uint32_t dev_idx,
							struct nfc_target *tgt)
{
	struct print_target_hdl_data *params = arg;

	if (params->tgt_count == 0) {
		params->dev_idx = dev_idx;
		printf("Found NFC target(s):\n"
			"Device Index:\tTarget Index:\tSupported Protocols:\n");
	}

	printf("%d\t\t%d\t\t0x%x\n", dev_idx, tgt->idx, tgt->protocols);
	params->tgt_count++;

	return TARGET_FOUND_SKIP;
}

static int list_targets(uint32_t protocols)
{
	struct nfcctl ctx;
	struct nfc_dev devl[NFC_DEV_MAX];
	uint8_t devl_count;
	struct print_target_hdl_data params;
	int rc;

	rc = init_and_get_devices(&ctx, devl);
	if (rc < 0)
		goto error;

	devl_count = rc;
	if (!devl_count)
		goto out;

	rc = start_poll_all_devices(&ctx, devl, devl_count, protocols);
	if (rc)
		goto error;

	params.tgt_count = 0;

	for(;;) {
		rc = nfcctl_targets_found(&ctx, print_target_handler, &params);
		if (rc)
			goto error;

		rc = nfcctl_start_poll(&ctx, &devl[params.dev_idx], protocols);
		if (rc)
			goto error;
	}

error:
	printerr("%s", strerror(rc));
out:
	nfcctl_deinit(&ctx);
	return rc;
}

static void usage(const char *prog)
{
	printf("Usage: %s  [-v] [-p PROT] (-d|-t)\n"
		"Option:\t\t\t\tDescription:\n"
		"-v, --verbose\t\t\tEnable verbosity\n"
		"-p, --protocol\t\t\tRestrict to PROT protocol\n"
		"\t\t\t\tPROT = {mifare}\n"
		"-d, --list-devices\t\tList all attached NFC devices\n"
		"-t, --list-targets\t\tList all found NFC targets\n\n",
		prog);

	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	int opt, op_idx;
	int rc;
	uint32_t protocols;

	if (argc == 1)
		usage(*argv);

	cmd = CMD_UNSPEC;
	op_idx = 0;
	protocols = NFC_PROTO_JEWEL_MASK | NFC_PROTO_MIFARE_MASK |
			NFC_PROTO_FELICA_MASK | NFC_PROTO_ISO14443_MASK |
			NFC_PROTO_NFC_DEP_MASK;

	for (;;) {
		opt = getopt_long(argc, argv, "vdtp:", lops, &op_idx);
		if (opt < 0)
			break;

		switch (opt) {
		case 'v':
			verbose = 1;
			break;
		case 'd':
			cmd = CMD_LIST_DEVICES;
			break;
		case 't':
			cmd = CMD_LIST_TARGETS;
			break;
		case 'p':
			if (!strcasecmp(optarg, "mifare")) {
				protocols = NFC_PROTO_MIFARE_MASK;
			} else {
				printerr("%s is not a valid argument to -p\n",
									optarg);
				usage(*argv);
			}
			break;
		case 0:
			break;
		default:
			usage(*argv);
		}
	}

	if (cmd == CMD_UNSPEC)
		usage(*argv);

	switch (cmd) {
	case CMD_LIST_DEVICES:
		rc = list_devices();
		break;
	case CMD_LIST_TARGETS:
		rc = list_targets(protocols);
		break;
	default:
		usage(*argv);
	}

	return rc < 0 ? -rc : rc;
}

