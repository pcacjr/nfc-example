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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <gst/gst.h>
#include <glib.h>

#include "misc.h"

extern int verbose;

static GMainLoop *loop;

static gboolean bus_call(GstBus *bus, GstMessage *msg, void *data)
{
	switch (GST_MESSAGE_TYPE(msg)) {
	case GST_MESSAGE_EOS:
		g_message("End of stream");
		g_main_loop_quit(loop);

		break;
	case GST_MESSAGE_ERROR:
	{
		GError *err;
		gst_message_parse_error(msg, &err, NULL);
		g_error("%s", err->message);
		g_error_free(err);

		g_main_loop_quit(loop);

		break;
	}
	}

	 return true;
}

/* Play a given sound file */
int misc_play_sound_file(const char *path, const char *file, const char *suffix)
{
	char *_file = NULL;
	size_t len;
	GstBus *bus;
	GstElement *pipeline;
	char *uri = NULL;

	/* Set up the pipeline */

	len = strlen(path) + strlen(file);
	len = path[strlen(path) - 1] == '/' ? len : len + 1;
	len = suffix ? len + strlen(suffix) : len;
	_file = (char *)malloc(len + 1);
	if (!file) {
		print_err("alloc char *");
		return -ENOMEM;
	}

	strncpy(_file, path, len);
	if (path[strlen(path) - 1] != '/')
		strncat(_file, "/", len);

	strncat(_file, file, len);
	if (suffix)
		strncat(_file, suffix, len);

	loop = g_main_loop_new(NULL, FALSE);

	pipeline = gst_element_factory_make("playbin", "player");

	len = strlen(_file) + 7 + 1;
	uri = (char *)malloc(len);
	if (!uri) {
		print_err("alloc char *");
		return -ENOMEM;
	}

	strncpy(uri, "file://", len);
	strncat(uri, _file, len);

	print_info("%s", uri);

	g_object_set(G_OBJECT(pipeline), "uri", uri, NULL);

	bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
	gst_bus_add_watch(bus, bus_call, NULL);
	gst_object_unref(bus);

	gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);

	g_main_loop_run(loop);

	gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_NULL);

	gst_object_unref(GST_OBJECT(pipeline));

	return 0;
}
