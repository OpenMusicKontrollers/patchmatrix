/*
 * Copyright (c) 2016-2020 Hanspeter Portner (dev@open-music-kontrollers.ch)
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the Artistic License 2.0 as published by
 * The Perl Foundation.
 *
 * This sink is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the iapplied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Artistic License 2.0 for more details.
 *
 * You should have received a copy of the Artistic License 2.0
 * along the sink as a COPYING file. If not, obtain it from
 * http://www.perlfoundation.org/artistic_license_2_0.
 */

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#define NSMC_IMPLEMENTATION
#include <nsmc/nsmc.h>

#include <patchmatrix.h>

typedef struct _monitor_app_t monitor_app_t;

struct _monitor_app_t {
	jack_client_t *client;
	jack_port_t *jsinks [PORT_MAX];
	float sample_rate_1;
	union {
		struct {
			float dBFSs [PORT_MAX];
		} audio;
		struct {
			float vels [PORT_MAX];
		} midi;
	};
	port_type_t type;

	const char *server_name;
	monitor_shm_t *shm;	
	nsmc_t *nsm;
	char *path;
};

static atomic_bool done = ATOMIC_VAR_INIT(false);
static atomic_bool closed = ATOMIC_VAR_INIT(false);

static void
_sig(int sig)
{
	atomic_store_explicit(&done, true, memory_order_relaxed);
}

static void
_close(monitor_shm_t *shm)
{
	atomic_store_explicit(&shm->closing, true, memory_order_relaxed);
	sem_post(&shm->done);
}

static void
_jack_on_info_shutdown_cb(jack_status_t code, const char *reason, void *arg)
{
	monitor_app_t *monitor = arg;
	monitor_shm_t *shm = monitor->shm;

	_close(shm);
}

static int
_audio_monitor_process(jack_nframes_t nframes, void *arg)
{
	monitor_app_t *monitor = arg;
	monitor_shm_t *shm = monitor->shm;
	const unsigned nsinks = atomic_load_explicit(&shm->nsinks, memory_order_acquire);

	if(  atomic_load_explicit(&closed, memory_order_relaxed)
		|| atomic_load_explicit(&shm->closing, memory_order_relaxed) )
	{
		return 0;
	}

	const float *psinks [PORT_MAX];

	for(unsigned i = 0; i < nsinks; i++)
	{
		jack_port_t *jsink = monitor->jsinks[i];
		psinks[i] = jack_port_get_buffer(jsink, nframes);

		float peak = 0.f;
		for(unsigned k = 0; k < nframes; k++)
		{
			const float sample = fabsf(psinks[i][k]);
			if(sample > peak)
			{
				peak = sample;
			}
		}

		// go to zero in 1/2 s
		if(monitor->audio.dBFSs[i] > -64.f)
		{
			monitor->audio.dBFSs[i] -= nframes * 70.f * 2.f * monitor->sample_rate_1;
		}

		const float dBFS = (peak > 0.f)
			? 6.f + 20.f*log10f(peak / 2.f) // dBFS+6
			: -64.f;

		if(dBFS > monitor->audio.dBFSs[i])
		{
			monitor->audio.dBFSs[i] = dBFS;
		}

		const int32_t mBFS = rintf(monitor->audio.dBFSs[i] * 100.f);
		atomic_store_explicit(&shm->jgains[i], mBFS, memory_order_relaxed);
	}

	return 0;
}

static int
_midi_monitor_process(jack_nframes_t nframes, void *arg)
{
	monitor_app_t *monitor = arg;
	monitor_shm_t *shm = monitor->shm;
	const unsigned nsinks = atomic_load_explicit(&shm->nsinks, memory_order_acquire);

	if(  atomic_load_explicit(&closed, memory_order_relaxed)
		|| atomic_load_explicit(&shm->closing, memory_order_relaxed) )
	{
		return 0;
	}

	void *psinks [PORT_MAX];

	for(unsigned i = 0; i < nsinks; i++)
	{
		jack_port_t *jsink = monitor->jsinks[i];
		psinks[i] = jack_port_get_buffer(jsink, nframes);

		float vel = 0.f;
		const uint32_t count = jack_midi_get_event_count(psinks[i]);
		for(unsigned k = 0; k < count; k++)
		{
			jack_midi_event_t ev;
			jack_midi_event_get(&ev, psinks[i], k);

			if(ev.size != 3)
			{
				continue;
			}

			if( (ev.buffer[0] & 0xf0) == 0x90)
			{
				if(ev.buffer[2] > vel)
				{
					vel = ev.buffer[2];
				}
			}
		}

		// go to zero in 1/2 s
		if(monitor->midi.vels[i] > 0.f)
		{
			monitor->midi.vels[i] -= nframes * 127.f * 2.f * monitor->sample_rate_1;
		}

		if(vel > monitor->midi.vels[i])
		{
			monitor->midi.vels[i] = vel;
		}

		const int32_t cvel = rintf(monitor->midi.vels[i] * 100.f);
		atomic_store_explicit(&shm->jgains[i], cvel, memory_order_relaxed);
	}

	return 0;
}

static cJSON *
_create_session(monitor_app_t *monitor)
{
	monitor_shm_t *shm = monitor->shm;
	const unsigned nsinks = atomic_load_explicit(&shm->nsinks, memory_order_acquire);

	cJSON *root = cJSON_CreateObject();
	if(root)
	{
		cJSON_AddStringToObject(root, "type", _port_type_to_string(monitor->type));
		cJSON_AddNumberToObject(root, "nsinks", nsinks);

		return root;
	}

	return NULL;
}

static int
_jack_deinit(monitor_app_t *monitor, unsigned nsinks)
{
	jack_deactivate(monitor->client);

	for(unsigned i = 0; i < nsinks; i++)
	{
#ifdef JACK_HAS_METADATA_API
		jack_uuid_t uuid = jack_port_uuid(monitor->jsinks[i]);
		jack_remove_properties(monitor->client, uuid);
#endif
		jack_port_unregister(monitor->client, monitor->jsinks[i]);
	}

	jack_client_close(monitor->client);
	monitor->client = NULL;

	return 0;
}

static int
_jack_init(monitor_app_t *monitor, const char *id, unsigned nsinks)
{
	jack_status_t status;
	jack_options_t opts = JackNullOption | JackNoStartServer;

	if(monitor->server_name)
	{
		opts |= JackServerName;
	}

	monitor->client = jack_client_open(id, opts, &status,
		monitor->server_name ? monitor->server_name : NULL);

	if(!monitor->client)
	{
		fprintf(stderr, "[%s] jack_client_open failed\n", __func__);
		return -1;
	}

	monitor->sample_rate_1 = 1.f / jack_get_sample_rate(monitor->client);

	for(unsigned i = 0; i < nsinks; i++)
	{
		char buf [32];
		snprintf(buf, 32, "sink_%02u", i + 1);

		jack_port_t *jsink = jack_port_register(monitor->client, buf,
			monitor->type == TYPE_AUDIO ? JACK_DEFAULT_AUDIO_TYPE : JACK_DEFAULT_MIDI_TYPE,
			JackPortIsInput | JackPortIsTerminal, 0);

#ifdef JACK_HAS_METADATA_API
		jack_uuid_t uuid = jack_port_uuid(jsink);

		snprintf(buf, 32, "%u", i);
		jack_set_property(monitor->client, uuid, JACKEY_ORDER, buf, XSD__integer);

		snprintf(buf, 32, "Sink %u", i + 1);
		jack_set_property(monitor->client, uuid, JACK_METADATA_PRETTY_NAME, buf, "text/plain");
#endif

		if(monitor->type == TYPE_AUDIO)
		{
			monitor->audio.dBFSs[i] = -64.f;
		}
		else if(monitor->type == TYPE_MIDI)
		{
			monitor->midi.vels[i] = 0.f;
		}

		monitor->jsinks[i] = jsink;
	}
	
	jack_on_info_shutdown(monitor->client, _jack_on_info_shutdown_cb, monitor);
	jack_set_process_callback(monitor->client,
		monitor->type == TYPE_AUDIO ? _audio_monitor_process : _midi_monitor_process,
		monitor);
	//TODO CV

	jack_activate(monitor->client);

	return 0;
}

static int
_open(monitor_app_t *monitor, const char *path, const char *name, const char *id)
{
	monitor_shm_t *shm = monitor->shm;
	unsigned nsinks = atomic_load_explicit(&shm->nsinks, memory_order_acquire);
	const bool switch_over = monitor->client ? true : false;

	if(monitor->path)
	{
		free(monitor->path);
	}

	monitor->path = strdup(path);

	if(switch_over)
	{
		if(_jack_deinit(monitor, nsinks) != 0)
		{
			return nsmc_opened(monitor->nsm, -1);
		}
	}

	cJSON *root = _load_session(path);

	if(root)
	{
		cJSON *type_node = cJSON_GetObjectItem(root, "type");
		if(type_node && cJSON_IsString(type_node))
		{
			const char *port_type = type_node->valuestring;

			monitor->type = _port_type_from_string(port_type);
		}

		cJSON *nsinks_node = cJSON_GetObjectItem(root, "nsinks");
		if(nsinks_node && cJSON_IsNumber(nsinks_node))
		{
			nsinks = nsinks_node->valueint;
		}

		cJSON_Delete(root);
	}

	if(_jack_init(monitor, id, nsinks) != 0)
	{
		return nsmc_opened(monitor->nsm, -1);
	}

	atomic_store_explicit(&shm->nsinks, nsinks, memory_order_release);

	return nsmc_opened(monitor->nsm, 0);
}

static int
_save(monitor_app_t *monitor)
{
	cJSON *root = _create_session(monitor);

	if(!root)
	{
		return nsmc_saved(monitor->nsm, -1);
	}

	_save_session(root, monitor->path);
	cJSON_Delete(root);

	return nsmc_saved(monitor->nsm, 0);
}

static int
_nsm_callback(void *data, const nsmc_event_t *ev)
{
	monitor_app_t *monitor = data;

	switch(ev->type)
	{
		case NSMC_EVENT_TYPE_OPEN:
			return _open(monitor, ev->open.path, ev->open.name, ev->open.id);
		case NSMC_EVENT_TYPE_SAVE:
			return _save(monitor);
		case NSMC_EVENT_TYPE_SHOW:
			return 1; // not supported
		case NSMC_EVENT_TYPE_HIDE:
			return 1; // not supported
		case NSMC_EVENT_TYPE_SESSION_IS_LOADED:
			return 0;

		case NSMC_EVENT_TYPE_VISIBILITY:
			return 0; // not supported
		case NSMC_EVENT_TYPE_CAPABILITY:
			return NSMC_CAPABILITY_MESSAGE
				| NSMC_CAPABILITY_SWITCH ;

		case NSMC_EVENT_TYPE_ERROR:
			fprintf(stderr, "err: %s: (%i) %s", ev->error.request,
				ev->error.code, ev->error.message);
			return 0;
		case NSMC_EVENT_TYPE_REPLY:
			fprintf(stderr, "reply: %s", ev->reply.request);
			return 0;

		case NSMC_EVENT_TYPE_NONE:
			// fall-through
		case NSMC_EVENT_TYPE_MAX:
			// fall-through
		default:
			return 1;
	}

	return 0;
}

int
main(int argc, char **argv)
{
	static monitor_app_t monitor;
	const size_t total_size = sizeof(monitor_shm_t);
	unsigned nsinks = 1;

	monitor.server_name = NULL;
	monitor.type = TYPE_AUDIO;

	fprintf(stderr,
		"%s "PATCHMATRIX_VERSION"\n"
		"Copyright (c) 2016-2020 Hanspeter Portner (dev@open-music-kontrollers.ch)\n"
		"Released under Artistic License 2.0 by Open Music Kontrollers\n", argv[0]);

	int c;
	while((c = getopt(argc, argv, "vhn:t:i:d:")) != -1)
	{
		switch(c)
		{
			case 'v':
				fprintf(stderr,
					"--------------------------------------------------------------------\n"
					"This is free software: you can redistribute it and/or modify\n"
					"it under the terms of the Artistic License 2.0 as published by\n"
					"The Perl Foundation.\n"
					"\n"
					"This source is distributed in the hope that it will be useful,\n"
					"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
					"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the\n"
					"Artistic License 2.0 for more details.\n"
					"\n"
					"You should have received a copy of the Artistic License 2.0\n"
					"along the source as a COPYING file. If not, obtain it from\n"
					"http://www.perlfoundation.org/artistic_license_2_0.\n\n");
				return 0;
			case 'h':
				fprintf(stderr,
					"--------------------------------------------------------------------\n"
					"USAGE\n"
					"   %s [OPTIONS]\n"
					"\n"
					"OPTIONS\n"
					"   [-v]                 print version and full license information\n"
					"   [-h]                 print usage information\n"
					"   [-t] port-type       port type (audio, midi)\n"
					"   [-i] input-num       port input number (1-%i)\n"
					"   [-n] server-name     connect to named JACK daemon\n\n"
					, argv[0], PORT_MAX);
				return 0;
			case 'n':
				monitor.server_name = optarg;
				break;
			case 't':
				monitor.type = _port_type_from_string(optarg);
				break;
			case 'i':
				nsinks = atoi(optarg);
				if(nsinks > PORT_MAX)
				{
					nsinks = PORT_MAX;
				}
				break;
			case '?':
				if( (optopt == 'n') || (optopt == 't') || (optopt == 'i') )
				{
					fprintf(stderr, "Option `-%c' requires an argument.\n", optopt);
				}
				else if(isprint(optopt))
				{
					fprintf(stderr, "Unknown option `-%c'.\n", optopt);
				}
				else
				{
					fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
				}
				return -1;
			default:
				return -1;
		}
	}

	signal(SIGTERM, _sig);
	signal(SIGQUIT, _sig);
	signal(SIGINT, _sig);

	const char *exe = strrchr(argv[0], '/');
	exe = exe ? exe + 1 : argv[0];
	monitor.nsm = nsmc_new("PATCHMATRIX-MONITOR", exe, argv[optind], _nsm_callback, &monitor);

	if(!monitor.nsm)
	{
		fprintf(stderr, "[%s] nsmc_new failed\n", __func__);
		return 1;
	}

	const char *client_name = jack_get_client_name(monitor.client);
	const int fd = shm_open(client_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if(fd != -1)
	{
		if(ftruncate(fd, total_size) != -1)
		{
			if((monitor.shm = mmap(NULL, total_size, PROT_READ | PROT_WRITE,
				MAP_SHARED, fd, 0)) != MAP_FAILED)
			{
				atomic_store_explicit(&monitor.shm->nsinks, nsinks, memory_order_release);

				atomic_init(&monitor.shm->closing, false);

				for(unsigned i = 0; i < PORT_MAX; i++)
				{
					atomic_init(&monitor.shm->jgains[i], 0);
				}

				if(sem_init(&monitor.shm->done, 1, 0) != -1)
				{
					struct timespec t1;
					clock_gettime(CLOCK_REALTIME, &t1);

					while(!atomic_load_explicit(&done, memory_order_relaxed))
					{
						if(sem_timedwait(&monitor.shm->done, &t1) == -1)
						{
							if(errno == ETIMEDOUT)
							{
								t1.tv_sec += 1; // schedule next timeout
							}
						}
						else
						{
							atomic_store_explicit(&done, true, memory_order_relaxed);
						}

						nsmc_run(monitor.nsm);
					}

					atomic_store_explicit(&monitor.shm->closing, true, memory_order_relaxed);

					sem_destroy(&monitor.shm->done);
				}

				atomic_store_explicit(&closed, true, memory_order_relaxed);
				munmap(monitor.shm, total_size);
			}
		}

		close(fd);
		shm_unlink(client_name);
	}

	nsmc_free(monitor.nsm);

	if(monitor.path)
	{
		free(monitor.path);
	}

	return 0;
}
