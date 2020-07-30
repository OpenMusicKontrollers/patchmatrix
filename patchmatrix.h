/*
 * Copyright (c) 2016-2020 Hanspeter Portner (dev@open-music-kontrollers.ch)
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the Artistic License 2.0 as published by
 * The Perl Foundation.
 *
 * This source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the iapplied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Artistic License 2.0 for more details.
 *
 * You should have received a copy of the Artistic License 2.0
 * along the source as a COPYING file. If not, obtain it from
 * http://www.perlfoundation.org/artistic_license_2_0.
 */

#ifndef _PATCHMATRIX_H
#define _PATCHMATRIX_H

#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <math.h>
#include <sys/stat.h> // mkdir
#include <errno.h> // mkdir
#include <signal.h>
#include <string.h>
#include <semaphore.h>

#include <jack/jack.h>
#include <jack/session.h>
#include <jack/midiport.h>
#ifdef JACK_HAS_METADATA_API
#	include <jack/uuid.h>
#	include <jack/metadata.h>
#	include <jackey.h>
#endif

#include <cJSON/cJSON.h>

#include <lv2/lv2plug.in/ns/ext/port-groups/port-groups.h>
#include <lv2/lv2plug.in/ns/extensions/ui/ui.h>

#include <varchunk.h>

#include <nsmc/nsmc.h>

#define NK_PUGL_API
#include <nk_pugl/nk_pugl.h>

#define PATCHMATRIX_URI              "http://open-music-kontrollers.ch/patchmatrix"
#define PATCHMATRIX_PREFIX           PATCHMATRIX_URI"#"
#define PATCHMATRIX__mainPositionX   PATCHMATRIX_PREFIX"mainPositionX"
#define PATCHMATRIX__mainPositionY   PATCHMATRIX_PREFIX"mainPositionY"
#define PATCHMATRIX__sourcePositionX PATCHMATRIX_PREFIX"sourcePositionX"
#define PATCHMATRIX__sourcePositionY PATCHMATRIX_PREFIX"sourcePositionY"
#define PATCHMATRIX__sinkPositionX   PATCHMATRIX_PREFIX"sinkPositionX"
#define PATCHMATRIX__sinkPositionY   PATCHMATRIX_PREFIX"sinkPositionY"

#define XSD_URI                      "http://www.w3.org/2001/XMLSchema"
#define XSD_PREFIX                   XSD_URI"#"
#define XSD__integer                 XSD_PREFIX"integer"
#define XSD__float                   XSD_PREFIX"float"

#define PATCHMATRIX_MIXER            "patchmatrix_mixer"
#define PATCHMATRIX_MONITOR          "patchmatrix_monitor"

#define PATCHMATRIX_MIXER_ID          "/"PATCHMATRIX_MIXER
#define PATCHMATRIX_MONITOR_ID        "/"PATCHMATRIX_MONITOR

#define PORT_MAX 128

typedef enum _event_type_t event_type_t;
typedef enum _port_type_t port_type_t;
typedef enum _port_designation_t port_designation_t;

typedef struct _hash_t hash_t;
typedef struct _port_conn_t port_conn_t;
typedef struct _client_conn_t client_conn_t;
typedef struct _port_t port_t;
typedef struct _mixer_shm_t mixer_shm_t;
typedef struct _monitor_shm_t monitor_shm_t;
typedef struct _client_t client_t;
typedef struct _app_t app_t;
typedef struct _event_t event_t;

enum _event_type_t {
	EVENT_CLIENT_REGISTER,
	EVENT_PORT_REGISTER,
	EVENT_PORT_CONNECT,
	EVENT_ON_INFO_SHUTDOWN,
	EVENT_GRAPH_ORDER,
	EVENT_SESSION,
	EVENT_FREEWHEEL,
	EVENT_BUFFER_SIZE,
	EVENT_SAMPLE_RATE,
	EVENT_XRUN,
#ifdef JACK_HAS_PORT_RENAME_CALLBACK
	EVENT_PORT_RENAME,
#endif
#ifdef JACK_HAS_METADATA_API
	EVENT_PROPERTY_CHANGE,
#endif
};

enum _port_type_t {
	TYPE_NONE   = (0 << 0),
	TYPE_AUDIO	= (1 << 0),
	TYPE_MIDI		= (1 << 1),
#ifdef JACK_HAS_METADATA_API
	TYPE_OSC		= (1 << 2),
	TYPE_CV			= (1 << 3)
#endif
};

enum _port_designation_t {
	DESIGNATION_NONE	= 0,
	DESIGNATION_LEFT,
	DESIGNATION_RIGHT,
	DESIGNATION_CENTER,
	DESIGNATION_SIDE,
	DESIGNATION_CENTER_LEFT,
	DESIGNATION_CENTER_RIGHT,
	DESIGNATION_SIDE_LEFT,
	DESIGNATION_SIDE_RIGHT,
	DESIGNATION_REAR_LEFT,
	DESIGNATION_REAR_RIGHT,
	DESIGNATION_REAR_CENTER,
	DESIGNATION_LOW_FREQUENCY_EFFECTS,

	DESIGNATION_MAX
};

struct _hash_t {
	void **nodes;
	unsigned size;
};

struct _port_conn_t {
	port_t *source_port;
	port_t *sink_port;
};

struct _client_conn_t {
	client_t *source_client;
	client_t *sink_client;
	hash_t conns;
	port_type_t type;

	struct nk_vec2 pos;
	bool moving;
};

struct _mixer_shm_t {
	sem_t done;
	atomic_bool closing;
	unsigned nsinks;
	unsigned nsources;
	atomic_int jgains [PORT_MAX][PORT_MAX];
};

struct _monitor_shm_t {
	sem_t done;
	atomic_bool closing;
	atomic_uint nsinks;
	atomic_int jgains [PORT_MAX];
};

struct _port_t {
	jack_port_t *body;
	client_t *client;
	jack_uuid_t uuid;
	char *name;
	char *short_name;
	char *pretty_name;
	int order;
	port_type_t type;
	port_designation_t designation;
};

struct node_linking {
	client_t *source_client;
	bool active;
};

struct node_editor {
	struct nk_rect bounds;
	struct node *selected;
	struct nk_vec2 scrolling;
	struct node_linking linking;
};

struct _client_t {
	jack_uuid_t uuid;
	char *name;
	char *pretty_name;
	hash_t ports;
	hash_t sources;
	hash_t sinks;

	int flags;
	struct nk_vec2 pos;
	struct nk_vec2 dim;
	bool moving;
	bool hilighted;
	bool hovered;

	mixer_shm_t *mixer_shm;
	monitor_shm_t *monitor_shm;
	port_type_t sink_type;
	port_type_t source_type;
};

struct _event_t {
	event_type_t type;

	union {
		struct {
			char *name;
			int state;
		} client_register;

		struct {
			jack_port_id_t id;
			int state;
		} port_register;

		struct {
			jack_port_id_t id_source;
			jack_port_id_t id_sink;
			int state;
		} port_connect;

#ifdef JACK_HAS_METADATA_API
		struct {
			jack_uuid_t uuid;
			char *key;
			jack_property_change_t state;
		} property_change;
#endif

		struct {
			jack_status_t code;
			char *reason;
		} on_info_shutdown;

		struct {
			jack_session_event_t *event;
		} session;

		struct {
			int starting;
		} freewheel;

		struct {
			jack_nframes_t nframes;
		} buffer_size;

		struct {
			jack_nframes_t nframes;
		} sample_rate;

		struct {
			char *old_name;
			char *new_name;
		} port_rename;
	};
};

struct _app_t {
	// UI
	port_type_t type;
	port_designation_t designation;
	bool freewheel;
	bool realtime;
	int32_t buffer_size;
	int32_t sample_rate;
	int32_t xruns;

	// JACK
	jack_client_t *client;
#ifdef JACK_HAS_METADATA_API
	jack_uuid_t uuid;
#endif

	// varchunk
	varchunk_t *from_jack;

	const char *server_name;

	nk_pugl_window_t win;

	float scale;
	float dy;

	float nxt_source;
	float nxt_sink;
	float nxt_default;
	hash_t clients;
	hash_t conns;

	struct node_editor nodedit;

	struct {
		struct nk_image audio;
		struct nk_image midi;
#ifdef JACK_HAS_METADATA_API
		struct nk_image cv;
		struct nk_image osc;
#endif
	} icons;

	atomic_bool done;
	bool animating;
	struct nk_rect contextbounds;

	cJSON *root;
	nsmc_t *nsm;
};

#define HASH_FOREACH(hash, itr) \
	for(void **(itr) = (hash)->nodes; (itr) - (hash)->nodes < (hash)->size; (itr)++)

#define HASH_FREE(hash, ptr) \
	for(void *(ptr) = _hash_pop((hash)); (ptr); (ptr) = _hash_pop((hash)))

static bool
_hash_empty(hash_t *hash)
{
	return hash->size == 0;
}

static size_t
_hash_size(hash_t *hash)
{
	return hash->size;
}

static void
_hash_add(hash_t *hash, void *node)
{
	hash->nodes = realloc(hash->nodes, (hash->size + 1)*sizeof(void *));
	if(hash->nodes)
	{
		hash->nodes[hash->size] = node;
		hash->size++;
	}
}

static void
_hash_remove(hash_t *hash, void *node)
{
	void **nodes = NULL;
	size_t size = 0;

	HASH_FOREACH(hash, node_itr)
	{
		void *node_ptr = *node_itr;

		if(node_ptr != node)
		{
			nodes = realloc(nodes, (size + 1)*sizeof(void *));
			if(nodes)
			{
				nodes[size] = node_ptr;
				size++;
			}
		}
	}

	free(hash->nodes);
	hash->nodes = nodes;
	hash->size = size;
}

static void
_hash_remove_cb(hash_t *hash, bool (*cb)(void *node, void *data), void *data)
{
	void **nodes = NULL;
	size_t size = 0;

	HASH_FOREACH(hash, node_itr)
	{
		void *node_ptr = *node_itr;

		if(cb(node_ptr, data))
		{
			nodes = realloc(nodes, (size + 1)*sizeof(void *));
			if(nodes)
			{
				nodes[size] = node_ptr;
				size++;
			}
		}
	}

	free(hash->nodes);
	hash->nodes = nodes;
	hash->size = size;
}

static void
_hash_free(hash_t *hash)
{
	free(hash->nodes);
	hash->nodes = NULL;
	hash->size = 0;
}

static void *
_hash_pop(hash_t *hash)
{
	if(hash->size)
	{
		void *node = hash->nodes[--hash->size];

		if(!hash->size)
			_hash_free(hash);

		return node;
	}

	return NULL;
}

static void
_hash_sort(hash_t *hash, int (*cmp)(const void *a, const void *b))
{
	if(hash->size)
		qsort(hash->nodes, hash->size, sizeof(void *), cmp);
}

static void
_hash_sort_r(hash_t *hash, int (*cmp)(const void *a, const void *b, void *data),
	void *data)
{
	if(hash->size)
		qsort_r(hash->nodes, hash->size, sizeof(void *), cmp, data);
}

#if defined(_WIN32)
static inline char *
strsep(char **sp, char *sep)
{
	char *p, *s;
	if(sp == NULL || *sp == NULL || **sp == '\0')
		return(NULL);
	s = *sp;
	p = s + strcspn(s, sep);
	if(*p != '\0')
		*p++ = '\0';
	*sp = p;
	return(s);
}
#endif

static int
_mkdirp(const char* path, mode_t mode)
{
	int ret = 0;

	// const cast for hack
	char *p = strdup(path);
	if(!p)
		return -1;

	char cwd [1024];
	getcwd(cwd, 1024);

	chdir("/");

	const char *pattern = "/";
	for(char *pp = p, *sub = strsep(&pp, pattern);
		sub;
		sub = strsep(&pp, pattern))
	{
		mkdir(sub, mode);
		chdir(sub);
	}

	chdir(cwd);

	free(p);

	return ret;
}

static cJSON *
_load_session(const char *session_dir)
{
	cJSON *root = NULL;

	// path may not exist yet
	_mkdirp(session_dir, S_IRWXU | S_IRGRP |  S_IXGRP | S_IROTH | S_IXOTH);

	char *session_file;
	if(asprintf(&session_file, "%s%s", session_dir, "state.json") != -1)
	{
		FILE *file = fopen(session_file, "r");
		if(file)
		{
			fseek(file, 0, SEEK_END);
			const size_t sz = ftell(file);
			fseek(file, 0, SEEK_SET);

			char *buf = malloc(sz + 1);
			if(buf)
			{
				if(fread(buf, sz, 1, file) == 1)
				{
					buf[sz] = '\0';
					root = cJSON_Parse(buf);
				}
				free(buf);
			}
			fclose(file);
		}
		free(session_file);
	}

	return root;
}

static void
_save_session(cJSON *root, const char *session_dir)
{
	// path may not exist yet
	_mkdirp(session_dir, S_IRWXU | S_IRGRP |  S_IXGRP | S_IROTH | S_IXOTH);

	char *buf = cJSON_Print(root);
	if(buf)
	{
		char *session_file;
		if(asprintf(&session_file, "%s%s", session_dir, "state.json") != -1)
		{
			FILE *file = fopen(session_file, "w");
			if(file)
			{
				fwrite(buf, strlen(buf), 1, file);
				fclose(file);
			}
			free(session_file);
		}
		free(buf);
	}
}

static const char *port_labels [] = {
	[TYPE_NONE] = NULL,
	[TYPE_AUDIO] = "AUDIO",
	[TYPE_MIDI] = "MIDI",
#ifdef JACK_HAS_METADATA_API
	[TYPE_CV] = "CV",
	[TYPE_OSC] = "OSC"
#endif
};

static port_type_t
_port_type_from_string(const char *str)
{
	if(!strcasecmp(str, port_labels[TYPE_AUDIO]))
		return TYPE_AUDIO;
	else if(!strcasecmp(str, port_labels[TYPE_MIDI]))
		return TYPE_MIDI;
#ifdef JACK_HAS_METADATA_API
	else if(!strcasecmp(str, port_labels[TYPE_CV]))
		return TYPE_CV;
	else if(!strcasecmp(str, port_labels[TYPE_OSC]))
		return TYPE_OSC;
#endif

	return TYPE_NONE;
}

static const char *
_port_type_to_string(port_type_t port_type)
{
	return port_labels[port_type];
}

static const char *designations [DESIGNATION_MAX] = {
	[DESIGNATION_NONE] = NULL,
	[DESIGNATION_LEFT] = LV2_PORT_GROUPS__left,
	[DESIGNATION_RIGHT] = LV2_PORT_GROUPS__right,
	[DESIGNATION_CENTER] = LV2_PORT_GROUPS__center,
	[DESIGNATION_SIDE] = LV2_PORT_GROUPS__side,
	[DESIGNATION_CENTER_LEFT] = LV2_PORT_GROUPS__centerLeft,
	[DESIGNATION_CENTER_RIGHT] = LV2_PORT_GROUPS__centerRight,
	[DESIGNATION_SIDE_LEFT] = LV2_PORT_GROUPS__sideLeft,
	[DESIGNATION_SIDE_RIGHT] = LV2_PORT_GROUPS__sideRight,
	[DESIGNATION_REAR_LEFT] = LV2_PORT_GROUPS__rearLeft,
	[DESIGNATION_REAR_RIGHT] = LV2_PORT_GROUPS__rearRight,
	[DESIGNATION_REAR_CENTER] = LV2_PORT_GROUPS__rearCenter,
	[DESIGNATION_LOW_FREQUENCY_EFFECTS] = LV2_PORT_GROUPS__lowFrequencyEffects
};

static int
_designation_get(const char *uri)
{
	for(int i=1; i<DESIGNATION_MAX; i++)
	{
		if(!strcmp(uri, designations[i]))
			return i; // found a match
	}

	return DESIGNATION_NONE; // found no match
}

#endif // _PATCHMATRIX_H
