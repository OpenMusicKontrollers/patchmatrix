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

typedef struct _mixer_app_t mixer_app_t;

struct _mixer_app_t {
	jack_client_t *client;
	jack_port_t *jautom;
	jack_port_t *jsinks [PORT_MAX];
	jack_port_t *jsources[PORT_MAX];
	port_type_t type;

	int16_t nrpn [0x10];
	int16_t data [0x10];

	mixer_shm_t *shm;	
	nsmc_t *nsm;
};

static atomic_bool closed = ATOMIC_VAR_INIT(false);

static void
_close(mixer_shm_t *shm)
{
	atomic_store_explicit(&shm->closing, true, memory_order_relaxed);
	sem_post(&shm->done);
}

static void
_jack_on_info_shutdown_cb(jack_status_t code, const char *reason, void *arg)
{
	mixer_app_t *mixer = arg;
	mixer_shm_t *shm = mixer->shm;

	_close(shm);
}

static inline void
_midi_handle(mixer_app_t *mixer, jack_midi_event_t *ev)
{
	const uint8_t cmd = ev->buffer[0] & 0xf0;

	if(cmd != 0xb0)
	{
		return;
	}

	const uint8_t chn = ev->buffer[0] & 0x0f;
	const uint8_t ctr = ev->buffer[1];
	const uint8_t val = ev->buffer[2];

	switch(ctr)
	{
		case 0x62: // NRPN_LSB
		{
			mixer->nrpn[chn] &= 0x3f80;
			mixer->nrpn[chn] |= val;
		} break;
		case 0x63: // NRPN_MSB
		{
			mixer->nrpn[chn] &= 0x7f;
			mixer->nrpn[chn] |= (val << 7);
		} break;
		case 0x26: // DATA_LSB
		{
			mixer->data[chn] &= 0x3f80;
			mixer->data[chn] |= val;
		} break;
		case 0x06: // DATA_MSB
		{
			mixer->data[chn] &= 0x7f;
			mixer->data[chn] |= (val << 7);

			const uint8_t nrpn_msb = mixer->nrpn[chn] >> 7;
			const uint8_t nrpn_lsb = mixer->nrpn[chn] & 0x7f;
			mixer_shm_t *shm = mixer->shm;

			if( (nrpn_msb < shm->nsources) && (nrpn_lsb < shm->nsinks) )
			{
				const int32_t mBFS = (float)(mixer->data[chn] - 0x1fff)/0x2000 * 3600.f;

				atomic_store_explicit(&shm->jgains[nrpn_msb][nrpn_lsb], mBFS, memory_order_relaxed);
			}
		} break;
	}
}

#include <osc.lv2/reader.h>

static inline void
_osc_message_handle(mixer_app_t *mixer, LV2_OSC_Reader *reader)
{
	const char *path = NULL;
	const char *type= NULL;

	lv2_osc_reader_get_string(reader, &path);
	if(!path || strcmp(path, "/patchmatrix/mixer"))
		return;

	lv2_osc_reader_get_string(reader, &type);
	if(!type || strcmp(type, ",iif"))
		return;

	int32_t nsink = 0;
	int32_t nsource = 0;
	float mBFS = 0.f;

	lv2_osc_reader_get_int32(reader, &nsink);
	lv2_osc_reader_get_int32(reader, &nsource);
	lv2_osc_reader_get_float(reader, &mBFS);

	mixer_shm_t *shm = mixer->shm;
	atomic_store_explicit(&shm->jgains[nsource][nsink], mBFS, memory_order_relaxed);
}

static inline void
_osc_packet_handle(mixer_app_t *mixer, const uint8_t *body, size_t size)
{
	LV2_OSC_Reader reader;
	lv2_osc_reader_initialize(&reader, body, size);

	if(lv2_osc_reader_is_bundle(&reader))
	{
		OSC_READER_BUNDLE_FOREACH(&reader, itm, size)
		{
			_osc_packet_handle(mixer, itm->body, itm->size);
		}
	}
	else if(lv2_osc_reader_is_message(&reader))
	{
		_osc_message_handle(mixer, &reader);
	}
}

static inline void
_autom_handle(mixer_app_t *mixer, jack_midi_event_t *ev)
{
	const uint8_t first = ev->buffer[0];

	if(first & 0x80)
	{
		_midi_handle(mixer, ev);
	}
	else
	{
		_osc_packet_handle(mixer, ev->buffer, ev->size);
	}
}

static inline void
_audio_mixer_process_internal(mixer_app_t *mixer,
	float *psources [PORT_MAX], const float *psinks [PORT_MAX],
	jack_nframes_t from, jack_nframes_t to)
{
	mixer_shm_t *shm = mixer->shm;

	if(from == to)
	{
		return; // shortcut
	}

	for(unsigned j = 0; j < shm->nsources; j++)
	{
		for(unsigned i = 0; i < shm->nsinks; i++)
		{
			const int32_t mBFS = atomic_load_explicit(&shm->jgains[j][i], memory_order_relaxed);
			const float dBFS = mBFS / 100.f;

			if(dBFS == 0.f) // just add
			{
				for(unsigned k = from; k < to; k++)
				{
					psources[j][k] += psinks[i][k];
				}
			}
			else if(dBFS > -36.f) // multiply-add
			{
				const float gain = exp10f(dBFS / 20.f); // jgain = 20.f*log10f(gain);

				for(unsigned k = from; k < to; k++)
				{
					psources[j][k] += gain * psinks[i][k];
				}
			}
			// else connection not to be mixed
		}
	}
}

static int
_audio_mixer_process(jack_nframes_t nframes, void *arg)
{
	mixer_app_t *mixer = arg;
	mixer_shm_t *shm = mixer->shm;

	if(  atomic_load_explicit(&closed, memory_order_relaxed)
		|| atomic_load_explicit(&shm->closing, memory_order_relaxed) )
	{
		return 0;
	}

	float *psources [PORT_MAX];
	const float *psinks [PORT_MAX];
	void *pautom;

	for(unsigned i = 0; i < shm->nsinks; i++)
	{
		jack_port_t *jsink = mixer->jsinks[i];
		psinks[i] = jack_port_get_buffer(jsink, nframes);
	}

	for(unsigned j = 0; j < shm->nsources; j++)
	{
		jack_port_t *jsource = mixer->jsources[j];
		psources[j] = jack_port_get_buffer(jsource, nframes);

		// clear
		for(unsigned k = 0; k < nframes; k++)
		{
			psources[j][k] = 0.f;
		}
	}

	pautom = jack_port_get_buffer(mixer->jautom, nframes);
	const unsigned count = jack_midi_get_event_count(pautom);
	jack_nframes_t from = 0;

	for(unsigned p = 0; p < count; p++)
	{
			jack_midi_event_t ev;
			jack_midi_event_get(&ev, pautom, p);

			_audio_mixer_process_internal(mixer, psources, psinks, from, ev.time);
			_autom_handle(mixer, &ev);

			from = ev.time;
	}

	_audio_mixer_process_internal(mixer, psources, psinks, from, nframes);

	return 0;
}

static int
_midi_mixer_process(jack_nframes_t nframes, void *arg)
{
	mixer_app_t *mixer = arg;
	mixer_shm_t *shm = mixer->shm;

	if(  atomic_load_explicit(&closed, memory_order_relaxed)
		|| atomic_load_explicit(&shm->closing, memory_order_relaxed) )
	{
		return 0;
	}

	void *psources [PORT_MAX];
	void *psinks [PORT_MAX + 1];

	unsigned count [PORT_MAX + 1];
	unsigned pos [PORT_MAX + 1];

	for(unsigned i = 0; i < shm->nsinks; i++)
	{
		jack_port_t *jsink = mixer->jsinks[i];
		psinks[i] = jack_port_get_buffer(jsink, nframes);

		count[i] = jack_midi_get_event_count(psinks[i]);
		pos[i] = 0;
	}

	{
		psinks[shm->nsinks] = jack_port_get_buffer(mixer->jautom, nframes);
		count[shm->nsinks] = jack_midi_get_event_count(psinks[shm->nsinks]);
		pos[shm->nsinks] = 0;
	}

	for(unsigned j = 0; j < shm->nsources; j++)
	{
		jack_port_t *jsource = mixer->jsources[j];
		psources[j] = jack_port_get_buffer(jsource, nframes);

		// clear
		jack_midi_clear_buffer(psources[j]);
	}

	while(true)
	{
		uint32_t T = UINT32_MAX;
		int I = -1;

		for(unsigned i = 0; i < shm->nsinks + 1; i++)
		{
			if(pos[i] >= count[i]) // no more events to process on this sink
				continue;

			jack_midi_event_t ev;
			jack_midi_event_get(&ev, psinks[i], pos[i]);

			if(ev.time <= T)
			{
				T = ev.time;
				I = i;
			}
		}

		if(I == -1) // no more events to process from all sinks
			break;

		jack_midi_event_t ev;
		jack_midi_event_get(&ev, psinks[I], pos[I]);

		if(I == shm->nsinks) // automation port
		{
			_autom_handle(mixer, &ev);
		}
		else
		{
			for(unsigned j = 0; j < shm->nsources; j++)
			{
				const int32_t mBFS = atomic_load_explicit(&shm->jgains[j][I], memory_order_relaxed);
				const float dBFS = mBFS / 100.f;

				if(dBFS > -36.f) // connection to be mixed
				{
					uint8_t *msg = jack_midi_event_reserve(psources[j], ev.time, ev.size);
					if(!msg)
						continue;

					memcpy(msg, ev.buffer, ev.size);

					if( (dBFS != 0.f) && (ev.size == 3) ) // multiply-add
					{
						const uint8_t cmd = msg[0] & 0xf0;
						if( (cmd == 0x90) || (cmd == 0x80) ) // noteOn or noteOff
						{
							const float gain = exp10f(dBFS / 20.f); // jgain = 20*log10(gain/1);

							const float vel = msg[2] * gain; // velocity
							msg[2] = vel < 0 ? 0 : (vel > 0x7f ? 0x7f : vel);
						}
					}
				}
				// else connection not to be mixed
			}
		}

		pos[I] += 1; // advance event pointer from this sink
	}

	return 0;
}

static cJSON *
_create_session(mixer_app_t *mixer)
{
	mixer_shm_t *shm = mixer->shm;

	cJSON *root = cJSON_CreateObject();
	if(root)
	{
		cJSON_AddStringToObject(root, "type", _port_type_to_string(mixer->type));
		cJSON_AddNumberToObject(root, "nsinks", shm->nsinks);
		cJSON_AddNumberToObject(root, "nsources", shm->nsources);
		cJSON *arr1 = cJSON_CreateArray();
		if(arr1)
		{
			for(unsigned j = 0; j < shm->nsources; j++)
			{
				cJSON *arr2 = cJSON_CreateArray();
				if(arr2)
				{
					for(unsigned i = 0; i < shm->nsinks; i++)
					{
						const int32_t gain = atomic_load_explicit(&shm->jgains[j][i], memory_order_relaxed);
						cJSON_AddItemToArray(arr2, cJSON_CreateNumber(gain));
					}
					cJSON_AddItemToArray(arr1, arr2);
				}
			}
			cJSON_AddItemToObject(root, "gains", arr1);
		}

		return root;
	}

	return NULL;
}

static void
_jack_session_cb(jack_session_event_t *jev, void *arg)
{
	mixer_app_t *mixer= arg;
	mixer_shm_t *shm = mixer->shm;

	asprintf(&jev->command_line, "patchmatrix_mixer -u %s -d ${SESSION_DIR}",
		jev->client_uuid);

	switch(jev->type)
	{
		case JackSessionSave:
		case JackSessionSaveAndQuit:
		{
			cJSON *root = _create_session(mixer);
			if(root)
			{
				_save_session(root, jev->session_dir);
				cJSON_Delete(root);
			}

			if(jev->type == JackSessionSaveAndQuit)
				_close(shm);
		}	break;
		case JackSessionSaveTemplate:
		{
			// nothing
		} break;
	}

	jack_session_reply(mixer->client, jev);
	jack_session_event_free(jev);
}

static int
_nsm_callback(void *data, const nsmc_event_t *ev)
{
	app_t *mixer = data;
	(void)mixer; //FIXME

	switch(ev->type)
	{
		//FIXME
		default:
		{
			// nothing to do
		} break;
	}

	return 0;
}

int
main(int argc, char **argv)
{
	static mixer_app_t mixer;
	const size_t total_size = sizeof(mixer_shm_t);

	cJSON *root = NULL;
	cJSON *gains_node = NULL;
	const char *server_name = NULL;
	const char *session_id = NULL;
	unsigned nsinks = 1;
	unsigned nsources = 1;
	mixer.type = TYPE_AUDIO;

	fprintf(stderr,
		"%s "PATCHMATRIX_VERSION"\n"
		"Copyright (c) 2016-2020 Hanspeter Portner (dev@open-music-kontrollers.ch)\n"
		"Released under Artistic License 2.0 by Open Music Kontrollers\n", argv[0]);

	int c;
	while((c = getopt(argc, argv, "vhn:u:t:i:o:d:")) != -1)
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
					"   [-o] output-num      port output number (1-%i)\n"
					"   [-n] server-name     connect to named JACK daemon\n"
					"   [-u] client-uuid     client UUID for JACK session management\n"
					"   [-d] session-dir     directory for JACK session management\n\n"
					, argv[0], PORT_MAX, PORT_MAX);
				return 0;
			case 'n':
				server_name = optarg;
				break;
			case 'u':
				session_id = optarg;
				break;
			case 'd':
				root = _load_session(optarg);
				break;
			case 't':
				mixer.type = _port_type_from_string(optarg);
				break;
			case 'i':
				nsinks = atoi(optarg);
				if(nsinks > PORT_MAX)
					nsinks = PORT_MAX;
				break;
			case 'o':
				nsources = atoi(optarg);
				if(nsources > PORT_MAX)
					nsources = PORT_MAX;
				break;
			case '?':
				if( (optopt == 'n') || (optopt == 'u') || (optopt == 't')
						|| (optopt == 'i') || (optopt == 'o') || (optopt == 'd') )
					fprintf(stderr, "Option `-%c' requires an argument.\n", optopt);
				else if(isprint(optopt))
					fprintf(stderr, "Unknown option `-%c'.\n", optopt);
				else
					fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
				return -1;
			default:
				return -1;
		}
	}

	const char *path = NULL; //FIXME
	mixer.nsm = nsmc_new(argv[0], "PATCHMATRIX-MONITOR", path,
		_nsm_callback, &mixer);

	//FIXME

	nsmc_free(mixer.nsm);

	if(root)
	{
		cJSON *type_node = cJSON_GetObjectItem(root, "type");
		if(type_node && cJSON_IsString(type_node))
		{
			const char *port_type = type_node->valuestring;

			mixer.type = _port_type_from_string(port_type);
		}

		cJSON *nsinks_node = cJSON_GetObjectItem(root, "nsinks");
		if(nsinks_node && cJSON_IsNumber(nsinks_node))
		{
			nsinks = nsinks_node->valueint;
		}

		cJSON *nsources_node = cJSON_GetObjectItem(root, "nsources");
		if(nsources_node && cJSON_IsNumber(nsources_node))
		{
			nsources = nsources_node->valueint;
		}

		gains_node = cJSON_GetObjectItem(root, "gains");
	}

	jack_options_t opts = JackNullOption | JackNoStartServer;
	if(server_name)
		opts |= JackServerName;
	if(session_id)
		opts |= JackSessionID;

	jack_status_t status;
	mixer.client = jack_client_open(PATCHMATRIX_MIXER_ID, opts, &status,
		server_name ? server_name : session_id,
		server_name ? session_id : NULL);
	if(!mixer.client)
		return -1;

	unsigned i;
	for(i = 0; i < nsinks; i++)
	{
		char buf [32];
		snprintf(buf, 32, "sink_%02u", i + 1);

		jack_port_t *jsink = jack_port_register(mixer.client, buf,
			mixer.type == TYPE_AUDIO ? JACK_DEFAULT_AUDIO_TYPE : JACK_DEFAULT_MIDI_TYPE,
			JackPortIsInput, 0);

#ifdef JACK_HAS_METADATA_API
		jack_uuid_t uuid = jack_port_uuid(jsink);

		snprintf(buf, 32, "%u", i);
		jack_set_property(mixer.client, uuid, JACKEY_ORDER, buf, XSD__integer);

		if(mixer.type == TYPE_MIDI)
			jack_set_property(mixer.client, uuid, JACKEY_EVENT_TYPES, "MIDI", "text/plain");

		snprintf(buf, 32, "Sink %u", i + 1);
		jack_set_property(mixer.client, uuid, JACK_METADATA_PRETTY_NAME, buf, "text/plain");
#endif

		mixer.jsinks[i] = jsink;
	}

	{
		char buf [32];
		snprintf(buf, 32, "automation");

		jack_port_t *jautom = jack_port_register(mixer.client, buf,
			JACK_DEFAULT_MIDI_TYPE,
			JackPortIsInput, 0);

#ifdef JACK_HAS_METADATA_API
		jack_uuid_t uuid = jack_port_uuid(jautom);

		snprintf(buf, 32, "%u", i);
		jack_set_property(mixer.client, uuid, JACKEY_ORDER, buf, XSD__integer);

		jack_set_property(mixer.client, uuid, JACKEY_EVENT_TYPES, "MIDI,OSC", "text/plain");

		jack_set_property(mixer.client, uuid, JACK_METADATA_PRETTY_NAME, "Automation", "text/plain");
#endif

		mixer.jautom = jautom;
	}

	for(unsigned j = 0; j < nsources; j++)
	{
		char buf [32];
		snprintf(buf, 32, "source_%02u", j + 1);

		jack_port_t *jsource = jack_port_register(mixer.client, buf,
			mixer.type == TYPE_AUDIO ? JACK_DEFAULT_AUDIO_TYPE : JACK_DEFAULT_MIDI_TYPE,
			JackPortIsOutput, 0);

#ifdef JACK_HAS_METADATA_API
		jack_uuid_t uuid = jack_port_uuid(jsource);

		snprintf(buf, 32, "%u", j);
		jack_set_property(mixer.client, uuid, JACKEY_ORDER, buf, XSD__integer);

		if(mixer.type == TYPE_MIDI)
			jack_set_property(mixer.client, uuid, JACKEY_EVENT_TYPES, "MIDI", "text/plain");

		snprintf(buf, 32, "Source %u", j + 1);
		jack_set_property(mixer.client, uuid, JACK_METADATA_PRETTY_NAME, buf, "text/plain");
#endif

		mixer.jsources[j] = jsource;
	}

	const char *client_name = jack_get_client_name(mixer.client);
	const int fd = shm_open(client_name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if(fd != -1)
	{
		if(ftruncate(fd, total_size) != -1)
		{
			if((mixer.shm = mmap(NULL, total_size, PROT_READ | PROT_WRITE,
				MAP_SHARED, fd, 0)) != MAP_FAILED)
			{
				mixer.shm->nsinks = nsinks;
				mixer.shm->nsources = nsources;

				atomic_init(&mixer.shm->closing, false);

				for(unsigned j = 0; j < nsources; j++)
				{
					for(unsigned i = 0; i < nsinks; i++)
					{
						if(j == i)
							atomic_init(&mixer.shm->jgains[j][i], 0);
						else
							atomic_init(&mixer.shm->jgains[j][i], -3600);

						if(gains_node)
						{
							cJSON *row = cJSON_GetArrayItem(gains_node, j);
							if(row)
							{
								cJSON *col = cJSON_GetArrayItem(row, i);
								if(col && cJSON_IsNumber(col))
									atomic_init(&mixer.shm->jgains[j][i], col->valueint);
							}
						}
					}
				}

				if(sem_init(&mixer.shm->done, 1, 0) != -1)
				{
					jack_on_info_shutdown(mixer.client, _jack_on_info_shutdown_cb, &mixer);
					jack_set_process_callback(mixer.client,
						mixer.type == TYPE_AUDIO ? _audio_mixer_process : _midi_mixer_process,
						&mixer);
					//TODO CV
					jack_set_session_callback(mixer.client, _jack_session_cb, &mixer);

					jack_activate(mixer.client);

					sem_wait(&mixer.shm->done);
					atomic_store_explicit(&mixer.shm->closing, true, memory_order_relaxed);

					jack_deactivate(mixer.client);

					sem_destroy(&mixer.shm->done);
				}

				atomic_store_explicit(&closed, true, memory_order_relaxed);
				munmap(mixer.shm, total_size);
			}
		}

		close(fd);
		shm_unlink(client_name);
	}

	for(unsigned i = 0; i < nsinks; i++)
	{
#ifdef JACK_HAS_METADATA_API
		jack_uuid_t uuid = jack_port_uuid(mixer.jsinks[i]);
		jack_remove_properties(mixer.client, uuid);
#endif
		jack_port_unregister(mixer.client, mixer.jsinks[i]);
	}

	{
#ifdef JACK_HAS_METADATA_API
		jack_uuid_t uuid = jack_port_uuid(mixer.jautom);
		jack_remove_properties(mixer.client, uuid);
#endif
		jack_port_unregister(mixer.client, mixer.jautom);
	}

	for(unsigned j = 0; j < nsources; j++)
	{
#ifdef JACK_HAS_METADATA_API
		jack_uuid_t uuid = jack_port_uuid(mixer.jsources[j]);
		jack_remove_properties(mixer.client, uuid);
#endif
		jack_port_unregister(mixer.client, mixer.jsources[j]);
	}

	jack_client_close(mixer.client);

	if(root)
		cJSON_Delete(root);

	return 0;
}
