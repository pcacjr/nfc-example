/*
 * Copyright (C) 2011 Instituto Nokia de Tecnologia
 *
 * Author:
 *    Lauro Ramos Venancio <lauro.venancio@openbossa.org>
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
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __LINUX_NFC_H
#define __LINUX_NFC_H

#include <linux/types.h>
#include <linux/socket.h>

#define NFC_GENL_NAME "nfc"
#define NFC_GENL_VERSION 1

#define NFC_GENL_MCAST_EVENT_NAME "events"

/**
 * enum nfc_commands - supported nfc commands
 *
 * @NFC_CMD_UNSPEC: unspecified command
 *
 * @NFC_CMD_GET_DEVICE: request information about a device (requires
 *	%NFC_ATTR_DEVICE_INDEX) or dump request to get a list of all nfc devices
 * @NFC_CMD_START_POLL: start polling for targets using the given protocols
 *	(requires %NFC_ATTR_DEVICE_INDEX and %NFC_ATTR_PROTOCOLS)
 * @NFC_CMD_STOP_POLL: stop polling for targets (requires
 *	%NFC_ATTR_DEVICE_INDEX)
 * @NFC_CMD_RESET_DEVICE: stop polling and deactivate all targets (requires
 *	%NFC_ATTR_DEVICE_INDEX)
 * @NFC_EVENT_TARGETS_FOUND: event emitted when a new target is found
 *	(it sends %NFC_ATTR_DEVICE_INDEX and %NFC_ATTR_TARGETS)
 */
enum nfc_commands {
	NFC_CMD_UNSPEC,
	NFC_CMD_GET_DEVICE,
	NFC_CMD_START_POLL,
	NFC_CMD_STOP_POLL,
	NFC_CMD_RESET_DEVICE,
	NFC_EVENT_TARGETS_FOUND,
/* private: internal use only */
	__NFC_CMD_AFTER_LAST
};
#define NFC_CMD_MAX (__NFC_CMD_AFTER_LAST - 1)

/**
 * enum nfc_attrs - supported nfc attributes
 *
 * @NFC_ATTR_UNSPEC: unspecified attribute
 *
 * @NFC_ATTR_DEVICE_INDEX: index of nfc device
 * @NFC_ATTR_DEVICE_NAME: device name, max 8 chars
 * @NFC_ATTR_PROTOCOLS: nfc protocols - bitwise or-ed combination from
 *	NFC_PROTO_* constants
 * @NFC_ATTR_TARGETS: array of targets (see enum nfc_target_attr)
 */
enum nfc_attrs {
	NFC_ATTR_UNSPEC,
	NFC_ATTR_DEVICE_INDEX,
	NFC_ATTR_DEVICE_NAME,
	NFC_ATTR_PROTOCOLS,
	NFC_ATTR_TARGETS,
/* private: internal use only */
	__NFC_ATTR_AFTER_LAST
};
#define NFC_ATTR_MAX (__NFC_ATTR_AFTER_LAST - 1)

#define NFC_DEVICE_NAME_MAXSIZE 8

/* NFC protocols */
#define NFC_PROTO_JEWEL		0
#define NFC_PROTO_MIFARE	1
#define NFC_PROTO_FELICA	2
#define NFC_PROTO_ISO14443	3
#define NFC_PROTO_NFC_DEP	4

#define NFC_PROTO_MAX		5

/* NFC protocols masks used in bitsets */
#define NFC_PROTO_JEWEL_MASK	(1 << NFC_PROTO_JEWEL)
#define NFC_PROTO_MIFARE_MASK	(1 << NFC_PROTO_MIFARE)
#define NFC_PROTO_FELICA_MASK	(1 << NFC_PROTO_FELICA)
#define NFC_PROTO_ISO14443_MASK	(1 << NFC_PROTO_ISO14443)
#define NFC_PROTO_NFC_DEP_MASK	(1 << NFC_PROTO_NFC_DEP)

/**
 * enum nfc_target_attr - attributes for nfc targets
 *
 * @NFC_TARGET_ATTR_UNSPEC: unspecified attribute
 * @NFC_TARGET_ATTR_TARGET_INDEX: target index
 * @NFC_TARGET_ATTR_SUPPORTED_PROTOCOLS: protocols supported by the target
 *	(bitwise or-ed combination from NFC_PROTO_* constants)
 */
enum nfc_target_attr {
	NFC_TARGET_ATTR_UNSPEC,
	NFC_TARGET_ATTR_TARGET_INDEX,
	NFC_TARGET_ATTR_SUPPORTED_PROTOCOLS,
/* private: internal use only */
	__NFC_TARGET_ATTR_AFTER_LAST
};
#define NFC_TARGET_ATTR_MAX (__NFC_TARGET_ATTR_AFTER_LAST - 1)

struct sockaddr_nfc {
	sa_family_t sa_family;
	__u32 dev_idx;
	__u32 target_idx;
	__u32 nfc_protocol;
};

/* NFC socket protocols */
#define NFC_SOCKPROTO_RAW	0
#define NFC_SOCKPROTO_MAX	1

#endif /*__LINUX_NFC_H */
