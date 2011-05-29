/*
 * Copyright (c) 2011 Paulo Alcantara <pcacjr@gmail.com>
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

#ifndef _MISC_H_
#define _MISC_H_

#define print_err(s, ...) \
	do  { \
		fprintf(stderr, "misc: error: " s "\n", ##__VA_ARGS__); \
	} while (0)

#define print_info(s, ...) \
	do  { \
		if (verbose) \
			fprintf(stdout, "misc: info: " s "\n", ##__VA_ARGS__); \
	} while (0)

/* Object flags (16-bit) */
enum {
	MISC_OBJ_UNSPEC         = 0x0000,       /* unspecified object */
	MISC_OBJ_CIGARETTE      = 0x0001,
	MISC_OBJ_PEN            = 0x0002,
	MISC_OBJ_BOOK           = 0x0004,
	MISC_OBJ_CELL_PHONE     = 0x0008,
	MISC_OBJ_MOUSE          = 0x0010,
	MISC_OBJ_CHAIR          = 0x0020,
	MISC_OBJ_TABLE          = 0x0040,
	MISC_OBJ_SERGIO_MURILO  = 0x0080,       /* LOL */
	MISC_OBJ_KEY            = 0x0100,
	MISC_OBJ_LIGHTER        = 0x0200,
	MISC_OBJ_BATTERY        = 0x0400,
	MISC_OBJ_CABLE          = 0x0800,
	MISC_OBJ_LAPTOP         = 0x1000,
	MISC_OBJ_MASK           = 0x1FFF,
} __attribute__((__packed__));

int misc_play_sound_file(const char *path, const char *file,
				const char *suffix);

#endif /* _MISC_H_ */
