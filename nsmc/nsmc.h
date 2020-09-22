/*
 * Copyright (c) 2015-2016 Hanspeter Portner (dev@open-music-kontrollers.ch)
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the Artistic License 2.0 as published by
 * The Perl Foundation.
 *
 * This source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Artistic License 2.0 for more details.
 *
 * You should have received a copy of the Artistic License 2.0
 * along the source as a COPYING file. If not, obtain it from
 * http://www.perlfoundation.org/artistic_license_2_0.
 */

#ifndef _NSMC_H
#define _NSMC_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NSMC_API
#	define NSMC_API static
#endif

#include <stdbool.h>
#include <stdarg.h>

typedef enum _nsmc_err_t {
	NSMC_ERR_GENERAL          = -1,
	NSMC_ERR_INCOMPATIBLE_API = -2,
	NSMC_ERR_BLACKLISTED      = -3,
	NSMC_ERR_LAUNCH_FAILED    = -4,
	NSMC_ERR_NO_SUCH_FILE     = -5,
	NSMC_ERR_NO_SESSION_OPEN  = -6,
	NSMC_ERR_UNSAVED_CHANGES  = -7,
	NSMC_ERR_NOT_NOW          = -8,
	NSMC_ERR_BAD_PROJECT      = -9,
	NSMC_ERR_CREATE_FAILED    = -10
} nsmc_err_t;

typedef enum _nsmc_capability_t {
	NSMC_CAPABILITY_NONE           = 0,

	// client only
	NSMC_CAPABILITY_SWITCH         = (1 << 0),
	NSMC_CAPABILITY_DIRTY          = (1 << 1),
	NSMC_CAPABILITY_PROGRESS       = (1 << 2),
	NSMC_CAPABILITY_MESSAGE        = (1 << 3),

	// client + server
	NSMC_CAPABILITY_OPTIONAL_GUI   = (1 << 4),

	// server only
	NSMC_CAPABILITY_SERVER_CONTROL = (1 << 5),
	NSMC_CAPABILITY_BROADCAST      = (1 << 6),

	NSMC_CAPABILITY_MAX
} nsmc_capability_t;

typedef enum _nsmc_event_type_t {
	NSMC_EVENT_TYPE_NONE = 0,

	NSMC_EVENT_TYPE_OPEN,
	NSMC_EVENT_TYPE_SAVE,
	NSMC_EVENT_TYPE_SHOW,
	NSMC_EVENT_TYPE_HIDE,
	NSMC_EVENT_TYPE_SESSION_IS_LOADED,

	NSMC_EVENT_TYPE_VISIBILITY,
	NSMC_EVENT_TYPE_CAPABILITY,

	NSMC_EVENT_TYPE_REPLY,
	NSMC_EVENT_TYPE_ERROR,

	NSMC_EVENT_TYPE_MAX
} nsmc_event_type_t;

typedef struct _nsmc_t nsmc_t;
typedef struct _nsmc_event_open_t nsmc_event_open_t;
typedef struct _nsmc_event_error_t nsmc_event_error_t;
typedef struct _nsmc_event_reply_t nsmc_event_reply_t;
typedef struct _nsmc_event_t nsmc_event_t;
	
typedef int (*nsmc_callback_t)(void *data, const nsmc_event_t *ev);

struct _nsmc_event_open_t {
	const char *path;
	const char *name;
	const char *id;
};

struct _nsmc_event_error_t {
	const char *request;
	nsmc_err_t code;
	const char *message;
};

struct _nsmc_event_reply_t {
	const char *request;
	const char *message;
};

struct _nsmc_event_t {
	nsmc_event_type_t type;
	union {
		nsmc_event_open_t open;
		nsmc_event_error_t error;
		nsmc_event_reply_t reply;
	};
};

NSMC_API nsmc_t *
nsmc_new(const char *call, const char *exe, const char *fallback_path,
	nsmc_callback_t callback, void *data);

NSMC_API void
nsmc_free(nsmc_t *nsm);

NSMC_API void
nsmc_pollin(nsmc_t *nsm, int timeout_ms);

NSMC_API int
nsmc_get_file_descriptors(nsmc_t *nsm, int fd[2]);

NSMC_API void
nsmc_run(nsmc_t *nsm);

NSMC_API int
nsmc_opened(nsmc_t *nsm, int status);

NSMC_API int
nsmc_shown(nsmc_t *nsm);

NSMC_API int
nsmc_hidden(nsmc_t *nsm);

NSMC_API int
nsmc_saved(nsmc_t *nsm, int status);

NSMC_API bool
nsmc_managed();

NSMC_API int
nsmc_progress(nsmc_t *nsm, float progress);

NSMC_API int
nsmc_dirty(nsmc_t *nsm);

NSMC_API int
nsmc_clean(nsmc_t *nsm);

NSMC_API int
nsmc_message(nsmc_t *nsm, int priority, const char *message);

NSMC_API int
nsmc_server_add(nsmc_t *nsm, const char *exe);

NSMC_API int
nsmc_server_save(nsmc_t *nsm);

NSMC_API int
nsmc_server_load(nsmc_t *nsm, const char *porj_name);

NSMC_API int
nsmc_server_new(nsmc_t *nsm, const char *porj_name);

NSMC_API int
nsmc_server_duplicate(nsmc_t *nsm, const char *porj_name);

NSMC_API int
nsmc_server_close(nsmc_t *nsm);

NSMC_API int
nsmc_server_abort(nsmc_t *nsm);

NSMC_API int
nsmc_server_quit(nsmc_t *nsm);

NSMC_API int
nsmc_server_list(nsmc_t *nsm);

NSMC_API int
nsmc_server_broadcast_varlist(nsmc_t *nsm, const char *fmt, va_list args);

NSMC_API int
nsmc_server_broadcast_vararg(nsmc_t *nsm, const char *fmt, ...);

#ifdef NSMC_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h> // getpid

#include <osc.lv2/writer.h>
#include <osc.lv2/reader.h>
#include <osc.lv2/stream.h>

#include <varchunk.h>

struct _nsmc_t {
	bool connected;
	bool connectionless;

	char *url;
	char *call;
	char *exe;

	nsmc_capability_t client_capability;
	nsmc_callback_t callback;
	void *data;

	LV2_OSC_Stream stream;

	varchunk_t *tx;
	varchunk_t *rx;

	nsmc_capability_t host_capability;

	bool again;
};

static const char *nsmc_capability_labels [NSMC_CAPABILITY_MAX] = {
	[NSMC_CAPABILITY_SWITCH] = "switch",
	[NSMC_CAPABILITY_DIRTY] = "dirty",
	[NSMC_CAPABILITY_PROGRESS] = "progress",
	[NSMC_CAPABILITY_MESSAGE] = "message",
	[NSMC_CAPABILITY_OPTIONAL_GUI] = "optional-gui",
	[NSMC_CAPABILITY_SERVER_CONTROL] = "server-control",
	[NSMC_CAPABILITY_BROADCAST] = "broadcast"
};

static int
_nsmc_message_varlist(nsmc_t *nsm, const char *path, const char *fmt, va_list args)
{
	if(!nsmc_managed())
	{
		return 0;
	}

	if(!nsm)
	{
		return 1;
	}

	size_t max = 0;
	uint8_t *tx = varchunk_write_request_max(nsm->tx, 1024, &max);
	if(!tx)
	{
		return 1;
	}

	LV2_OSC_Writer writer;
	lv2_osc_writer_initialize(&writer, tx, max);

	if(lv2_osc_writer_message_varlist(&writer, path, fmt, args) == false)
	{
		return 1;
	}

	size_t written;
	if(lv2_osc_writer_finalize(&writer, &written) == NULL)
	{
		return 1;
	}

	varchunk_write_advance(nsm->tx, written);

	nsm->again = true;

	return 0;
}

static int
_nsmc_message_vararg(nsmc_t *nsm, const char *path, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

	const int ret = _nsmc_message_varlist(nsm, path, fmt, args);

	va_end(args);

	return ret;
}

static const char *
_arg_to_string(LV2_OSC_Reader *reader, LV2_OSC_Arg **arg)
{
	if(lv2_osc_reader_arg_is_end(reader, *arg))
	{
		return NULL;
	}

	if((*arg)->type[0] != LV2_OSC_STRING)
	{
		return NULL;
	}

	const char *s = (*arg)->s;

	*arg = lv2_osc_reader_arg_next(reader, *arg);

	return s;
}

static int32_t
_arg_to_int32(LV2_OSC_Reader *reader, LV2_OSC_Arg **arg)
{
	if(lv2_osc_reader_arg_is_end(reader, *arg))
	{
		return 0;
	}

	if((*arg)->type[0] != LV2_OSC_INT32)
	{
		return 0;
	}

	const int32_t i = (*arg)->i;

	*arg = lv2_osc_reader_arg_next(reader, *arg);

	return i;
}

static void
_reply_server_announce(LV2_OSC_Reader *reader, LV2_OSC_Arg *arg,
	const LV2_OSC_Tree *tree, nsmc_t *nsm)
{
	const char *message = _arg_to_string(reader, &arg);
	const char *manager = _arg_to_string(reader, &arg);
	const char *capabilities = _arg_to_string(reader, &arg);

	(void)message; //FIXME
	(void)manager; //FIXME

	char *caps = alloca(strlen(capabilities) + 1);
	strcpy(caps, capabilities);

	char *tok;
	while( (tok = strsep(&caps, ":")) )
	{
		for(nsmc_capability_t cap = NSMC_CAPABILITY_NONE;
			cap < NSMC_CAPABILITY_MAX;
			cap++)
		{
			if(  nsmc_capability_labels[cap]
				&& !strcasecmp(nsmc_capability_labels[cap], tok) )
			{
				nsm->host_capability |= cap;
			}
		}
	}
}

static void
_reply(LV2_OSC_Reader *reader, LV2_OSC_Arg *arg, const LV2_OSC_Tree *tree,
	void *data)
{
	nsmc_t *nsm = data;
	const char *target = _arg_to_string(reader, &arg);

	if(!target)
	{
		return;
	}

	if(!strcasecmp(target, "/nsm/server/announce"))
	{
		_reply_server_announce(reader, arg, tree, nsm);
	}

	const nsmc_event_t ev_reply = {
		.type = NSMC_EVENT_TYPE_REPLY,
		.reply = {
			.request = target
		}
	};

	nsm->callback(nsm->data, &ev_reply);
}

static void
_error(LV2_OSC_Reader *reader, LV2_OSC_Arg *arg, const LV2_OSC_Tree *tree,
	void *data)
{
	nsmc_t *nsm = data;

	const char *target = _arg_to_string(reader, &arg);
	if(!target)
	{
		return;
	}

	const nsmc_err_t code = _arg_to_int32(reader, &arg);
	if(!code)
	{
		return;
	}

	const char *err = _arg_to_string(reader, &arg);
	if(!err)
	{
		return;
	}

	const nsmc_event_t ev_error = {
		.type = NSMC_EVENT_TYPE_ERROR,
		.error = {
			.request = target,
			.code = code,
			.message = err
		}
	};

	nsm->callback(nsm->data, &ev_error);
}

static void
_client_open(LV2_OSC_Reader *reader, LV2_OSC_Arg *arg, const LV2_OSC_Tree *tree,
	void *data)
{
	nsmc_t *nsm = data;
	const char *dir = _arg_to_string(reader, &arg);
	const char *name = _arg_to_string(reader, &arg);
	const char *id = _arg_to_string(reader, &arg);

	char tmp [PATH_MAX];
	const char *resolvedpath = realpath(dir, tmp);
	if(!resolvedpath)
	{
		resolvedpath = dir;
	}

	const nsmc_event_t ev_open = {
		.type = NSMC_EVENT_TYPE_OPEN,
		.open = {
			.path = resolvedpath,
			.name = name,
			.id = id
		}
	};

	if(nsm->callback(nsm->data, &ev_open) != 0)
	{
		fprintf(stderr, "NSM load failed: '%s'\n", dir);
	}

	// return if client does not support optional gui
	if(!(nsm->client_capability & NSMC_CAPABILITY_OPTIONAL_GUI))
	{
		return;
	}

	const nsmc_event_t ev_show = {
		.type = NSMC_EVENT_TYPE_SHOW
	};

	// always show gui if server does not support optional gui
	if(!(nsm->host_capability & NSMC_CAPABILITY_OPTIONAL_GUI))
	{
		if(nsm->callback(nsm->data, &ev_show) != 0)
		{
			//FIXME report error
		}

		return;
	}

	const nsmc_event_t ev_visibility = {
		.type = NSMC_EVENT_TYPE_VISIBILITY
	};

	// put gui visibility into last known state
	const int visibility = nsm->callback(nsm->data, &ev_visibility);

	if(visibility && (nsm->callback(nsm->data, &ev_show) == 0) )
	{
		_nsmc_message_vararg(nsm, "/nsm/client/gui_is_shown", "");
	}
	else
	{
		_nsmc_message_vararg(nsm, "/nsm/client/gui_is_hidden", "");
	}
}

static void
_client_save(LV2_OSC_Reader *reader, LV2_OSC_Arg *arg, const LV2_OSC_Tree *tree,
	void *data)
{
	nsmc_t *nsm = data;
	const nsmc_event_t ev_save = {
		.type = NSMC_EVENT_TYPE_SAVE
	};

	// save app
	if(nsm->callback(nsm->data, &ev_save) != 0)
	{
		fprintf(stderr, "NSM save failed:\n");
	}
}

static void
_client_session_is_loaded(LV2_OSC_Reader *reader, LV2_OSC_Arg *arg, const LV2_OSC_Tree *tree,
	void *data)
{
	nsmc_t *nsm = data;
	const nsmc_event_t ev_session_is_loaded = {
		.type = NSMC_EVENT_TYPE_SESSION_IS_LOADED
	};

	// save app
	if(nsm->callback(nsm->data, &ev_session_is_loaded) != 0)
	{
		fprintf(stderr, "NSM session_is_loaded failed:\n");
	}
}

static void
_client_show_optional_gui(LV2_OSC_Reader *reader, LV2_OSC_Arg *arg,
	const LV2_OSC_Tree *tree, void *data)
{
	nsmc_t *nsm = data;
	const nsmc_event_t ev_show = {
		.type = NSMC_EVENT_TYPE_SHOW
	};

	// show gui
	if(  (nsm->client_capability & NSMC_CAPABILITY_OPTIONAL_GUI)
		&& (nsm->callback(nsm->data, &ev_show) == 0) )
	{
		nsmc_shown(nsm);
	}
}

static void
_client_hide_optional_gui(LV2_OSC_Reader *reader, LV2_OSC_Arg *arg,
	const LV2_OSC_Tree *tree, void *data)
{
	nsmc_t *nsm = data;
	const nsmc_event_t ev_hide = {
		.type = NSMC_EVENT_TYPE_HIDE
	};

	// hide gui
	if(  (nsm->client_capability & NSMC_CAPABILITY_OPTIONAL_GUI)
		&& (nsm->callback(nsm->data, &ev_hide) == 0) )
	{
		nsmc_hidden(nsm);
	}
}

static int
_announce(nsmc_t *nsm)
{
	char capabilities [128] = "";

	for(nsmc_capability_t cap = NSMC_CAPABILITY_NONE;
		cap < NSMC_CAPABILITY_MAX;
		cap++)
	{
		if(  (nsm->client_capability & cap)
			&& nsmc_capability_labels[cap] )
		{
			strcat(capabilities, ":");
			strcat(capabilities, nsmc_capability_labels[cap]);
		}
	}

	if(strlen(capabilities) > 0)
	{
		strcat(capabilities, ":");
	}

	// send announce message
	pid_t pid = getpid();

	return _nsmc_message_vararg(nsm, "/nsm/server/announce", "sssiii",
		nsm->call, capabilities, nsm->exe, 1, 2, pid);
}

static const LV2_OSC_Tree tree_client [] = {
	{ .name = "open",              .trees = NULL, .branch = _client_open },
	{ .name = "save",              .trees = NULL, .branch = _client_save },
	{ .name = "session_is_loaded", .trees = NULL, .branch = _client_session_is_loaded},
	{ .name = "show_optional_gui", .trees = NULL, .branch = _client_show_optional_gui },
	{ .name = "hide_optional_gui", .trees = NULL, .branch = _client_hide_optional_gui },
	{ .name = NULL,                .trees = NULL, .branch = NULL } // sentinel
};

static const LV2_OSC_Tree tree_nsm [] = {
	{ .name = "client", .trees = tree_client, .branch = NULL },
	{ .name = NULL,     .trees = NULL,        .branch = NULL } // sentinel
};

static const LV2_OSC_Tree tree_root [] = {
	{ .name = "reply", .trees = NULL,     .branch = _reply },
	{ .name = "error", .trees = NULL,     .branch = _error },
	{ .name = "nsm",   .trees = tree_nsm, .branch = NULL },

	{ .name = NULL,    .trees = NULL,     .branch = NULL } // sentinel
};

static void *
_recv_req(void *data, size_t size, size_t *max)
{
	nsmc_t *nsm = data;

	return varchunk_write_request_max(nsm->rx, size, max);
}

static void
_recv_adv(void *data, size_t written)
{
	nsmc_t *nsm = data;

	varchunk_write_advance(nsm->rx, written);
}

static const void *
_send_req(void *data, size_t *len)
{
	nsmc_t *nsm = data;

	return varchunk_read_request(nsm->tx, len);
}

static void
_send_adv(void *data)
{
	nsmc_t *nsm = data;

	varchunk_read_advance(nsm->tx);
}

static const LV2_OSC_Driver driver = {
	.write_req = _recv_req,
	.write_adv = _recv_adv,
	.read_req = _send_req,
	.read_adv = _send_adv
};

NSMC_API nsmc_t *
nsmc_new(const char *call, const char *exe, const char *fallback_path,
	nsmc_callback_t callback, void *data)
{
	if(!callback)
	{
		return NULL;
	}

	nsmc_t *nsm = calloc(1, sizeof(nsmc_t));
	if(!nsm)
	{
		return NULL;
	}

	nsm->callback = callback;
	nsm->data = data;

	const nsmc_event_t ev_capability = {
		.type = NSMC_EVENT_TYPE_CAPABILITY,
	};

	nsm->client_capability = nsm->callback(nsm->data, &ev_capability);

	nsm->call = call ? strdup(call) : NULL;
	nsm->exe = exe ? strdup(exe) : NULL;

	char *nsm_url = getenv("NSM_URL");
	if(nsm_url)
	{
		nsm->connectionless = !strncmp(nsm_url, "osc.udp", 7) ? true : false;

		nsm->url = strdup(nsm_url);
		if(!nsm->url)
		{
			goto fail;
		}

		// remove trailing slash
		if(!isdigit(nsm->url[strlen(nsm->url)-1]))
		{
			nsm->url[strlen(nsm->url)-1] = '\0';
		}

		nsm->tx = varchunk_new(8192, false);
		if(!nsm->tx)
		{
			goto fail;
		}

		nsm->rx = varchunk_new(8192, false);
		if(!nsm->rx)
		{
			goto fail;
		}

		if(lv2_osc_stream_init(&nsm->stream, nsm->url, &driver, nsm) != 0)
		{
			goto fail;
		}
	}
	else if(fallback_path)
	{
		char tmp [PATH_MAX];
		const char *resolvedfallback_path = realpath(fallback_path, tmp);
		if(!resolvedfallback_path)
		{
			resolvedfallback_path = fallback_path;
		}

		const nsmc_event_t ev_open = {
			.type = NSMC_EVENT_TYPE_OPEN,
			.open = {
				.path = resolvedfallback_path,
				.name = "unmanaged",
				.id = nsm->call
			}
		};

		if(nsm->callback(nsm->data, &ev_open) != 0)
		{
			fprintf(stderr, "NSM load failed: '%s'\n", fallback_path);
			goto fail;
		}

		const nsmc_event_t ev_show = {
			.type = NSMC_EVENT_TYPE_SHOW
		};

		if(nsm->callback(nsm->data, &ev_show) != 0)
		{
			fprintf(stderr, "NSM show failed: '%s'\n", fallback_path);
			goto fail;
		}
	}

	return nsm;

fail:
	nsmc_free(nsm);

	return NULL;
}

NSMC_API void
nsmc_free(nsmc_t *nsm)
{
	if(nsm)
	{
		if(nsm->call)
		{
			free(nsm->call);
		}

		if(nsm->exe)
		{
			free(nsm->exe);
		}

		if(nsm->url)
		{
			if(nsm->rx)
			{
				varchunk_free(nsm->rx);
			}

			if(nsm->tx)
			{
				varchunk_free(nsm->tx);
			}

			lv2_osc_stream_deinit(&nsm->stream);

			free(nsm->url);
		}

		free(nsm);
	}
}

NSMC_API void
nsmc_pollin(nsmc_t *nsm, int timeout_ms)
{
	if(!nsm || !nsm->rx)
	{
		return;
	}

	nsm->again = true;

	while(nsm->again)
	{
		nsm->again = false;

		const LV2_OSC_Enum ev = lv2_osc_stream_pollin(&nsm->stream, timeout_ms);

		if(ev & LV2_OSC_ERR)
		{
			nsm->connected = false;
		}

		if(nsm->connectionless || (ev & LV2_OSC_CONN) )
		{
			if(!nsm->connected)
			{
				_announce(nsm); // initial announcement
				nsm->connected = true;
			}
		}

		if(ev & LV2_OSC_SEND)
		{
			nsm->again = true;
		}

		if(ev & LV2_OSC_RECV)
		{
			const uint8_t *rx;
			size_t size;
			while( (rx = varchunk_read_request(nsm->rx, &size)) )
			{
				LV2_OSC_Reader reader;

				lv2_osc_reader_initialize(&reader, rx, size);
				lv2_osc_reader_match(&reader, size, tree_root, nsm);

				varchunk_read_advance(nsm->rx);
			}
		}
	}
}

NSMC_API int
nsmc_get_file_descriptors(nsmc_t *nsm, int fd[2])
{
	return lv2_osc_stream_get_file_descriptors(&nsm->stream, fd);
}

NSMC_API void
nsmc_run(nsmc_t *nsm)
{
	return nsmc_pollin(nsm, 0);
}

NSMC_API int
nsmc_opened(nsmc_t *nsm, int status)
{
	if(status == 0)
	{
		return _nsmc_message_vararg(nsm, "/reply", "ss",
					"/nsm/client/open", "opened");
	}

	return _nsmc_message_vararg(nsm, "/error", "sis",
		"/nsm/client/open", NSMC_ERR_GENERAL, "opening failed");
}

NSMC_API int
nsmc_shown(nsmc_t *nsm)
{
	return _nsmc_message_vararg(nsm, "/nsm/client/gui_is_shown", "");
}

NSMC_API int
nsmc_hidden(nsmc_t *nsm)
{
	return _nsmc_message_vararg(nsm, "/nsm/client/gui_is_hidden", "");
}

NSMC_API int
nsmc_saved(nsmc_t *nsm, int status)
{
	if(status == 0)
	{
		return _nsmc_message_vararg(nsm, "/reply", "ss",
			"/nsm/client/save", "saved");
	}

	return _nsmc_message_vararg(nsm, "/error", "sis",
		"/nsm/client/save", NSMC_ERR_GENERAL, "save failed");
}

NSMC_API bool
nsmc_managed()
{
	if(getenv("NSM_URL"))
	{
		return true;
	}

	return false;
}

NSMC_API int
nsmc_progress(nsmc_t *nsm, float progress)
{
	if(!nsm || !(nsm->client_capability & NSMC_CAPABILITY_PROGRESS))
	{
		return 1;
	}

	return _nsmc_message_vararg(nsm, "/nsm/client/progress", "f", progress);
}

NSMC_API int
nsmc_dirty(nsmc_t *nsm)
{
	if(!nsm || !(nsm->client_capability & NSMC_CAPABILITY_DIRTY))
	{
		return 1;
	}

	return _nsmc_message_vararg(nsm, "/nsm/client/is_dirty", "");
}

NSMC_API int
nsmc_clean(nsmc_t *nsm)
{
	if(!nsm || !(nsm->client_capability & NSMC_CAPABILITY_DIRTY))
	{
		return 1;
	}

	return _nsmc_message_vararg(nsm, "/nsm/client/is_clean", "");
}

NSMC_API int
nsmc_message(nsmc_t *nsm, int priority, const char *message)
{
	if(!nsm || !(nsm->client_capability & NSMC_CAPABILITY_MESSAGE))
	{
		return 1;
	}

	return _nsmc_message_vararg(nsm, "/nsm/client/message", "is",
		priority, message);
}

NSMC_API int
nsmc_server_add(nsmc_t *nsm, const char *exe)
{
	if(!nsm || !(nsm->host_capability & NSMC_CAPABILITY_SERVER_CONTROL))
	{
		return 1;
	}

	return _nsmc_message_vararg(nsm, "/nsm/server/add", "s", exe);
}

NSMC_API int
nsmc_server_save(nsmc_t *nsm)
{
	if(!nsm || !(nsm->host_capability & NSMC_CAPABILITY_SERVER_CONTROL))
	{
		return 1;
	}

	return _nsmc_message_vararg(nsm, "/nsm/server/save", "");
}

NSMC_API int
nsmc_server_load(nsmc_t *nsm, const char *proj_name)
{
	if(!nsm || !(nsm->host_capability & NSMC_CAPABILITY_SERVER_CONTROL))
	{
		return 1;
	}

	return _nsmc_message_vararg(nsm, "/nsm/server/load", "s", proj_name);
}

NSMC_API int
nsmc_server_new(nsmc_t *nsm, const char *proj_name)
{
	if(!nsm || !(nsm->host_capability & NSMC_CAPABILITY_SERVER_CONTROL))
	{
		return 1;
	}

	return _nsmc_message_vararg(nsm, "/nsm/server/new", "s", proj_name);
}

NSMC_API int
nsmc_server_duplicate(nsmc_t *nsm, const char *proj_name)
{
	if(!nsm || !(nsm->host_capability & NSMC_CAPABILITY_SERVER_CONTROL))
	{
		return 1;
	}

	return _nsmc_message_vararg(nsm, "/nsm/server/duplicate", "s", proj_name);
}

NSMC_API int
nsmc_server_close(nsmc_t *nsm)
{
	if(!nsm || !(nsm->host_capability & NSMC_CAPABILITY_SERVER_CONTROL))
	{
		return 1;
	}

	return _nsmc_message_vararg(nsm, "/nsm/server/close", "");
}

NSMC_API int
nsmc_server_abort(nsmc_t *nsm)
{
	if(!nsm || !(nsm->host_capability & NSMC_CAPABILITY_SERVER_CONTROL))
	{
		return 1;
	}

	return _nsmc_message_vararg(nsm, "/nsm/server/abort", "");
}

NSMC_API int
nsmc_server_quit(nsmc_t *nsm)
{
	if(!nsm || !(nsm->host_capability & NSMC_CAPABILITY_SERVER_CONTROL))
	{
		return 1;
	}

	return _nsmc_message_vararg(nsm, "/nsm/server/quit", "");
}

NSMC_API int
nsmc_server_list(nsmc_t *nsm)
{
	if(!nsm || !(nsm->host_capability & NSMC_CAPABILITY_SERVER_CONTROL))
	{
		return 1;
	}

	return _nsmc_message_vararg(nsm, "/nsm/server/list", "");
}

NSMC_API int
nsmc_server_broadcast_varlist(nsmc_t *nsm, const char *fmt, va_list args)
{
	if(!nsm || !(nsm->host_capability & NSMC_CAPABILITY_BROADCAST))
	{
		return 1;
	}

	return _nsmc_message_varlist(nsm, "/nsm/server/broadcast", fmt, args);
}

NSMC_API int
nsmc_server_broadcast_vararg(nsmc_t *nsm, const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);

	const int ret = nsmc_server_broadcast_varlist(nsm, fmt, args);

	va_end(args);

	return ret;
}

#endif /* NSMC_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif

#endif /*_NSMC_H */
