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
#include <sys/types.h>
#include <sys/socket.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <poll.h>

#include "tag_mifare.h"

struct mifare_cmd {
	uint8_t cmd;
	uint8_t block;
	uint8_t data[];
} __attribute__((packed));

#define DATA_BLOCK_START 4
#define DATA_BLOCK_END 15

#define BLK_SIZE 4
#define BLK_TO_B(x) (x * BLK_SIZE)

#define CMD_READ 0x30

#define CMD_READ_BLK_COUNT 4

#define NFC_HEADER_SIZE 1

extern int verbose;

#define printdbg(s, ...)						\
	do {								\
		if (verbose)						\
			fprintf(stderr, "%s:%d %s: " s "\n",		\
					__FILE__, __LINE__,		\
					__func__, ##__VA_ARGS__);	\
	} while (0)

static int send_command(int fd, struct mifare_cmd *cmd, size_t cmd_size)
{
	struct pollfd fds;
	int rc;

	fds.fd = fd;
	fds.events = POLLOUT;
	fds.revents = 0;

	rc = poll(&fds, 1, -1);
	printdbg("poll(%p, 1, 1) = %d", &fds, rc);
	if (rc == -1) {
		printdbg("poll error: %s", strerror(errno));
		return rc;
	}
	if (fds.revents != POLLOUT) {
		printdbg("poll error revent=0x%x", fds.revents);
		errno = EIO;
		return -1;
	}

	rc = send(fd, cmd, cmd_size, 0);
	printdbg("send(%d, %p, %lu, 0) = %d", fd, cmd, cmd_size, rc);
	if (rc == -1)
		printdbg("send error: %s", strerror(errno));

	return rc;
}

static int recv_command_reply(int fd, void *buf, size_t count)
{
	struct pollfd fds;
	int rc;

	fds.fd = fd;
	fds.events = POLLIN;
	fds.revents = 0;

	rc = poll(&fds, 1, -1);
	printdbg("poll(%p, 1, 1) = %d", &fds, rc);
	if (rc == -1) {
		printdbg("poll error: %s", strerror(errno));
		return rc;
	}
	if (fds.revents != POLLIN) {
		printdbg("poll error revent=0x%x", fds.revents);
		errno = EIO;
		return -1;
	}

	rc = recv(fd, buf, count, 0);
	printdbg("recv(%d, %p, %lu, 0) = %d", fd, buf, count, rc);
	if (rc == -1)
		printdbg("recv error: %s", strerror(errno));
	if (rc != count) {
		errno = EIO;
		rc = -1;
	}

	return rc;
}

int tag_mifare_read(int fd, void *buf, size_t count)
{
	size_t read_size = BLK_TO_B(CMD_READ_BLK_COUNT);
	struct mifare_cmd cmd;
	size_t recv_size = NFC_HEADER_SIZE + read_size;
	uint8_t recv_buf[recv_size];
	size_t bytes_count;
	int rc;

	printdbg("IN");

	if (count > TAG_MIFARE_MAX_SIZE) {
		errno = EINVAL;
		return -1;
	}

	cmd.cmd = CMD_READ;
	cmd.block = DATA_BLOCK_START;

	bytes_count = 0;

	while (bytes_count < count) {

		rc = send_command(fd, &cmd, sizeof(cmd));
		if (rc == -1)
			return rc;

		cmd.block += CMD_READ_BLK_COUNT;
		bytes_count += read_size;
	}

	bytes_count = 0;

	while (bytes_count < count) {
		uint8_t bytes_to_read = read_size;

		if (bytes_count + bytes_to_read > count)
			bytes_to_read = count - bytes_count;

		rc = recv_command_reply(fd, recv_buf, recv_size);
		if (rc == -1)
			return bytes_count;

		if (recv_buf[0] != 0) {
			errno = EIO;
			return bytes_count;
		}

		memcpy(buf + bytes_count, recv_buf + NFC_HEADER_SIZE,
								bytes_to_read);

		bytes_count += bytes_to_read;
	}

	return bytes_count;
}
