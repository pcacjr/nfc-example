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
#include <errno.h>
#include <sys/socket.h>

#include <gst/gst.h>
#include <glib.h>

#include "nfcctl.h"
#include "tag_mifare.h"
#include "linux/nfc.h"
#include "misc.h"

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
	CMD_READ_TAG,
	CMD_WRITE_TAG,
	CMD_RUN_TEST,
};

int cmd;

const struct option lops[] = {
	{ "verbose", no_argument, &verbose, 1 },
	{ "list-devices", no_argument, &cmd, CMD_LIST_DEVICES },
	{ "list-targets", no_argument, &cmd, CMD_LIST_TARGETS },
	{ "read-tag", no_argument, &cmd, CMD_READ_TAG },
	{ "write-tag", required_argument, &cmd, CMD_WRITE_TAG },
	{ "protocol", required_argument, NULL, 'p' },
	{ "run-test", no_argument, &cmd, CMD_RUN_TEST },
	{ 0, 0, 0, 0 },
};

const char *sound_files_path = "/home/pcacjr/vol0/devel/nfc-example/sounds/";
const char *sound_file_suffix = ".mp3";

/* Sound File Indexes */
enum {
	SOUND_FILE_CIGARETTE,
	SOUND_FILE_PEN,
	SOUND_FILE_BOOK,
	SOUND_FILE_CELL_PHONE,
	SOUND_FILE_MOUSE,
	SOUND_FILE_CHAIR,
	SOUND_FILE_TABLE,
	SOUND_FILE_SERGIO_MURILO,
	SOUND_FILE_KEY,
	SOUND_FILE_LIGHTER,
	SOUND_FILE_BATTERY,
	SOUND_FILE_CABLE,
	SOUND_FILE_LAPTOP,

	/* You might want to add other objects here... */

	SOUND_FILE_MAX_SIZE,
};

/* Sound File Names */
const char *sound_files[SOUND_FILE_MAX_SIZE] = {
	"cigarette", "pen", "book", "cell_phone", "mouse", "chair", "table",
	"sergio_murilo", "key", "lighter", "battery", "cable", "laptop",
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

static int list_targets(int protocol)
{
	struct nfcctl ctx;
	struct nfc_dev devl[NFC_DEV_MAX];
	uint8_t devl_count;
	uint32_t protocols;
	struct print_target_hdl_data params;
	int rc;

	rc = init_and_get_devices(&ctx, devl);
	if (rc < 0)
		goto error;

	devl_count = rc;
	if (!devl_count)
		goto out;

	if (protocol >= 0) {
		protocols = 1 << protocol;
	} else {
		protocols = NFC_PROTO_JEWEL_MASK | NFC_PROTO_MIFARE_MASK |
			NFC_PROTO_FELICA_MASK | NFC_PROTO_ISO14443_MASK |
			NFC_PROTO_NFC_DEP_MASK;
	}

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

struct save_target_hdl_data {
	uint32_t desired_protocol;
	uint32_t dev_idx;
	uint32_t tgt_idx;
};

static int save_target_handler(void *arg, uint32_t dev_idx,
							struct nfc_target *tgt)
{
	struct save_target_hdl_data *params = arg;

	if (tgt->protocols & (1 << params->desired_protocol)) {
		params->dev_idx = dev_idx;
		params->tgt_idx = tgt->idx;
		return TARGET_FOUND_STOP;
	}

	return TARGET_FOUND_SKIP;
}

static int __read_tag(uint32_t protocol, void *buf, size_t len)
{
	struct nfcctl ctx;
	struct nfc_dev devl[NFC_DEV_MAX];
	uint8_t devl_count;
	struct save_target_hdl_data params;
	int rc;

	if (protocol != NFC_PROTO_MIFARE) {
		printerr("Tag read support for protocol (%d) not"
						" implemented\n", protocol);
		return -ENOSYS;
	}

	if (len > TAG_MIFARE_MAX_SIZE + 1) {
		printerr("Length > TAG_MIFARE_MAX_SIZE + 1\n");
		return -EINVAL;
	}

	rc = init_and_get_devices(&ctx, devl);
	if (rc < 0)
		goto error;

	devl_count = rc;
	if (!devl_count)
		goto out;

	rc = start_poll_all_devices(&ctx, devl, devl_count, 1 << protocol);
	if (rc)
		goto error;

	params.desired_protocol = protocol;

	rc = nfcctl_targets_found(&ctx, save_target_handler, &params);
	if (rc)
		goto error;

	rc = nfcctl_target_init(&ctx, params.dev_idx, params.tgt_idx, protocol);
	if (rc)
		goto error;

	rc = tag_mifare_read(ctx.target_fd, buf, len - 1);
	if (rc == -1) {
		rc = errno;
		goto error;
	}

	rc = 0;
	goto out;

error:
	printerr("%s", strerror(rc));
out:
	nfcctl_deinit(&ctx);
	return rc;
}

static int read_tag(uint32_t protocol)
{
	struct nfcctl ctx;
	struct nfc_dev devl[NFC_DEV_MAX];
	uint8_t devl_count;
	uint8_t buf[TAG_MIFARE_MAX_SIZE + 1];
	struct save_target_hdl_data params;
	int rc;

	if (protocol != NFC_PROTO_MIFARE) {
		printerr("Tag read support for protocol (%d) not"
						" implemented\n", protocol);
		return -ENOSYS;
	}

	rc = init_and_get_devices(&ctx, devl);
	if (rc < 0)
		goto error;

	devl_count = rc;
	if (!devl_count)
		goto out;

	rc = start_poll_all_devices(&ctx, devl, devl_count, 1 << protocol);
	if (rc)
		goto error;

	params.desired_protocol = protocol;

	rc = nfcctl_targets_found(&ctx, save_target_handler, &params);
	if (rc)
		goto error;

	rc = nfcctl_target_init(&ctx, params.dev_idx, params.tgt_idx, protocol);
	if (rc)
		goto error;

	rc = tag_mifare_read(ctx.target_fd, buf, TAG_MIFARE_MAX_SIZE);
	if (rc == -1) {
		rc = errno;
		goto error;
	}

	buf[TAG_MIFARE_MAX_SIZE] = '\0';

	printf("%s\n", (char *) buf);

	rc = 0;
	goto out;

error:
	printerr("%s", strerror(rc));
out:
	nfcctl_deinit(&ctx);
	return rc;
}

static int __write_tag(uint32_t protocol, const void *buf, size_t len)
{
	struct nfcctl ctx;
	struct nfc_dev devl[NFC_DEV_MAX];
	uint8_t devl_count;
	struct save_target_hdl_data params;
	int rc;

	if (protocol != NFC_PROTO_MIFARE) {
		printerr("Tag write support for protocol (%d) not"
						" implemented\n", protocol);
		return -ENOSYS;
	}

	rc = init_and_get_devices(&ctx, devl);
	if (rc < 0)
		goto error;

	devl_count = rc;
	if (!devl_count)
		goto out;

	rc = start_poll_all_devices(&ctx, devl, devl_count, 1 << protocol);
	if (rc)
		goto error;

	params.desired_protocol = protocol;

	rc = nfcctl_targets_found(&ctx, save_target_handler, &params);
	if (rc)
		goto error;

	rc = nfcctl_target_init(&ctx, params.dev_idx, params.tgt_idx, protocol);
	if (rc)
		goto error;

	rc = tag_mifare_write(ctx.target_fd, buf, len);
	if (rc != len) {
		rc = errno;
		goto error;
	}

	rc = 0;
	goto out;

error:
	printerr("%s", strerror(rc));
out:
	nfcctl_deinit(&ctx);
	return rc;

}

static int write_tag(uint32_t protocol, char *string, size_t lenght)
{
	struct nfcctl ctx;
	struct nfc_dev devl[NFC_DEV_MAX];
	uint8_t devl_count;
	struct save_target_hdl_data params;
	int rc;

	if (protocol != NFC_PROTO_MIFARE) {
		printerr("Tag write support for protocol (%d) not"
						" implemented\n", protocol);
		return -ENOSYS;
	}

	rc = init_and_get_devices(&ctx, devl);
	if (rc < 0)
		goto error;

	devl_count = rc;
	if (!devl_count)
		goto out;

	rc = start_poll_all_devices(&ctx, devl, devl_count, 1 << protocol);
	if (rc)
		goto error;

	params.desired_protocol = protocol;

	rc = nfcctl_targets_found(&ctx, save_target_handler, &params);
	if (rc)
		goto error;

	rc = nfcctl_target_init(&ctx, params.dev_idx, params.tgt_idx, protocol);
	if (rc)
		goto error;

	rc = tag_mifare_write(ctx.target_fd, string, lenght);
	if (rc != lenght) {
		rc = errno;
		goto error;
	}

	rc = 0;
	goto out;

error:
	printerr("%s", strerror(rc));
out:
	nfcctl_deinit(&ctx);
	return rc;
}

/* Return the sound file name */
const char *get_sound_file(uint16_t flags)
{
	const char *file = NULL;

	if (flags & ~MISC_OBJ_MASK) {
		print_err("invalid flags");
		return NULL;
	}

	if (flags & MISC_OBJ_CIGARETTE)
		file = sound_files[SOUND_FILE_CIGARETTE];
	else if (flags & MISC_OBJ_PEN)
		file = sound_files[SOUND_FILE_PEN];
	else if (flags & MISC_OBJ_BOOK)
		file = sound_files[SOUND_FILE_BOOK];
	else if (flags & MISC_OBJ_CELL_PHONE)
		file = sound_files[SOUND_FILE_CELL_PHONE];
	else if (flags & MISC_OBJ_MOUSE)
		file = sound_files[SOUND_FILE_MOUSE];
	else if (flags & MISC_OBJ_CHAIR)
		file = sound_files[SOUND_FILE_CHAIR];
	else if (flags & MISC_OBJ_TABLE)
		file = sound_files[SOUND_FILE_TABLE];
	else if (flags & MISC_OBJ_SERGIO_MURILO)
		file = sound_files[SOUND_FILE_SERGIO_MURILO];
	else if (flags & MISC_OBJ_KEY)
		file = sound_files[SOUND_FILE_KEY];
	else if (flags & MISC_OBJ_LIGHTER)
		file = sound_files[SOUND_FILE_LIGHTER];
	else if (flags & MISC_OBJ_BATTERY)
		file = sound_files[SOUND_FILE_BATTERY];
	else if (flags & MISC_OBJ_CABLE)
		file = sound_files[SOUND_FILE_CABLE];
	else if (flags & MISC_OBJ_LAPTOP)
		file = sound_files[SOUND_FILE_LAPTOP];

	return file;
}

static int run_test(uint32_t protocol, int *argc, char ***argv)
{
	unsigned i;
	size_t len;
	int err;
	const char *s;

	/* Initialize GStreamer */
	gst_init(argc, argv);

	/* Objects to be detected in order */
	const uint16_t obj_list[9] = {
		MISC_OBJ_CIGARETTE, MISC_OBJ_PEN, MISC_OBJ_BOOK,
		MISC_OBJ_CELL_PHONE, MISC_OBJ_CHAIR, MISC_OBJ_TABLE,
		MISC_OBJ_KEY, MISC_OBJ_BATTERY, MISC_OBJ_SERGIO_MURILO,
	};

	i = 0;
	len = sizeof(obj_list[0]);

	/* Initially write the first object id */
	err = __write_tag(protocol, &obj_list[i++], len);
	if (!err)
		goto out;

	do {
		uint16_t flags;

		err = __read_tag(protocol, &flags, len);
		if (err)
			goto out;

		printdbg("Read data was 0x%x\n", flags);

		s = get_sound_file(obj_list[i]);
		if (!s) {
			err = -1;
			goto out;
		}

		printdbg("Found sound file: %s\n", s);

		err = __write_tag(protocol, &obj_list[i], len);
		if (err)
			goto out;

		printdbg("Written data was 0x%x\n", obj_list[i]);

		err = misc_play_sound_file(sound_files_path, s,
						sound_file_suffix);
		if (err)
			goto out;

	} while (++i < 9);

	return 0;

out:
	return err;
}

static void usage(const char *prog)
{
	printf("Usage: %s  [-v] [-p PROT] (-d|-t|-r|-w STR)\n"
		"Option:\t\t\t\tDescription:\n"
		"-v, --verbose\t\t\tEnable verbosity\n"
		"-p, --protocol\t\t\tRestrict to PROT protocol\n"
		"\t\t\t\tPROT = {mifare}\n"
		"-d, --list-devices\t\tList all attached NFC devices\n"
		"-t, --list-targets\t\tList all found NFC targets\n"
		"-r, --read-tag\t\t\tRead tag\n"
		"-w, --write-tag\t\t\tWrite STR to tag\n"
		"-s, --run-test\t\t\tRun test\n\n",
		prog);

	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	int opt, op_idx;
	int rc;
	int protocol;
	size_t write_str_max = TAG_MIFARE_MAX_SIZE;
	char write_str[write_str_max];
	size_t write_str_len;

	if (argc == 1)
		usage(*argv);

	cmd = CMD_UNSPEC;
	op_idx = 0;
	protocol = -1;

	for (;;) {
		opt = getopt_long(argc, argv, "vdtsrw:p:", lops, &op_idx);
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
		case 'r':
			cmd = CMD_READ_TAG;
			break;
		case 'w':
			cmd = CMD_WRITE_TAG;
			write_str_len = strlen(optarg);
			if (write_str_len > write_str_max) {
				printerr("Write up to %lu chars\n",
								write_str_max);
				usage(*argv);
			}
			if (write_str_len < write_str_max)
				write_str_len++; /* '\0' */
			strncpy(write_str, optarg, write_str_len);
			break;
		case 'p':
			if (!strcasecmp(optarg, "mifare")) {
				protocol = NFC_PROTO_MIFARE;
			} else {
				printerr("%s is not a valid argument to -p\n",
									optarg);
				usage(*argv);
			}
			break;
		case 's':
			cmd = CMD_RUN_TEST;
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
		rc = list_targets(protocol);
		break;
	case CMD_READ_TAG:
		if (protocol == -1) {
			printerr("-r command requires protocol choice");
			usage(*argv);
		}
		rc = read_tag(protocol);
		break;
	case CMD_WRITE_TAG:
		if (protocol == -1) {
			printerr("-w command requires protocol choice");
			usage(*argv);
		}
		rc = write_tag(protocol, write_str, write_str_len);
		break;
	case CMD_RUN_TEST:
		if (protocol == -1) {
			printerr("-s command requires protocol choice");
			usage(*argv);
		}

		rc = run_test(protocol, &argc, &argv);
		break;
	default:
		usage(*argv);
	}

	return rc < 0 ? -rc : rc;
}
