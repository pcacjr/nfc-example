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

#include "nfcctl.h"

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
};

int cmd;

const struct option lops[] = {
	{ "verbose", no_argument, &verbose, 1 },
	{ "list-devices", no_argument, &cmd, CMD_LIST_DEVICES },
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

static int list_devices(void)
{
	struct nfcctl ctx;
	struct nfc_dev devl[NFC_DEV_MAX];
	uint8_t devl_count;
	int rc;

	rc = nfcctl_init(&ctx);
	if (rc) {
		printdbg("%s", strerror(rc));
		return rc;
	}

	devl_count = nfcctl_get_devices(&ctx, devl, NFC_DEV_MAX);
	if (devl_count < 0) {
		rc = devl_count;
		printdbg("%s", strerror(rc));
		goto out;
	}

	if (!devl_count) {
		rc = 0;
		printf("Info: There isn't any attached NFC device\n");
		goto out;
	}

	print_devices(devl, devl_count);

out:
	nfcctl_deinit(&ctx);
	return rc;
}

static void usage(const char *prog)
{
	printf("Usage: %s  [-v] -d\n"
		"Option:\t\t\t\tDescription:\n"
		"-v, --verbose\t\t\tEnable verbosity\n"
		"-d, --list-devices\t\tList all attached NFC devices\n\n",
		prog);

	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	int opt, op_idx;
	int rc;

	if (argc == 1)
		usage(*argv);

	cmd = CMD_UNSPEC;
	op_idx = 0;

	for (;;) {
		opt = getopt_long(argc, argv, "vd", lops, &op_idx);
		if (opt < 0)
			break;

		switch (opt) {
		case 'v':
			verbose = 1;
			break;
		case 'd':
			cmd = CMD_LIST_DEVICES;
			break;
		case 0:
			break;
		default:
			usage(*argv);
		}
	}

	switch (cmd) {
	case CMD_LIST_DEVICES:
		rc = list_devices();
		break;
	default:
		usage(*argv);
	}

	return rc < 0 ? -rc : rc;
}
