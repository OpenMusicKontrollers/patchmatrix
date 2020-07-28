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

#include <patchmatrix_jack.h>
#include <patchmatrix_db.h>
#include <patchmatrix_nk.h>

static cJSON *
_create_session(app_t *app)
{
	cJSON *root = cJSON_CreateObject();
	if(root)
	{
		cJSON *clients_node = cJSON_CreateArray();
		if(clients_node)
		{
			HASH_FOREACH(&app->clients, client_itr)
			{
				client_t *client = *client_itr;

				cJSON *client_node = cJSON_CreateObject();
				if(client_node)
				{
					char uuid_str [JACK_UUID_STRING_SIZE];
					jack_uuid_unparse(client->uuid, uuid_str);

					cJSON_AddStringToObject(client_node, "client_name", client->name);
					cJSON_AddStringToObject(client_node, "client_uuid", uuid_str);
					cJSON_AddNumberToObject(client_node, "xpos", client->pos.x);
					cJSON_AddNumberToObject(client_node, "ypos", client->pos.y);
					cJSON_AddBoolToObject(client_node, "sinks", client->flags & JackPortIsInput);
					cJSON_AddBoolToObject(client_node, "sources", client->flags & JackPortIsOutput);

					cJSON_AddItemToArray(clients_node, client_node);
				}
			}

			cJSON_AddItemToObject(root, "clients", clients_node);
		}

		cJSON *conns_node = cJSON_CreateArray();
		if(conns_node)
		{
			HASH_FOREACH(&app->conns, client_conn_itr)
			{
				client_conn_t *client_conn = *client_conn_itr;

				cJSON *conn_node = cJSON_CreateObject();
				if(conn_node)
				{
					char source_uuid_str [JACK_UUID_STRING_SIZE];
					char sink_uuid_str [JACK_UUID_STRING_SIZE];
					jack_uuid_unparse(client_conn->source_client->uuid, source_uuid_str);
					jack_uuid_unparse(client_conn->sink_client->uuid, sink_uuid_str);

					cJSON_AddStringToObject(conn_node, "source_name", client_conn->source_client->name);
					cJSON_AddStringToObject(conn_node, "sink_name", client_conn->sink_client->name);
					cJSON_AddStringToObject(conn_node, "source_uuid", source_uuid_str);
					cJSON_AddStringToObject(conn_node, "sink_uuid", sink_uuid_str);
					cJSON_AddNumberToObject(conn_node, "xpos", client_conn->pos.x);
					cJSON_AddNumberToObject(conn_node, "ypos", client_conn->pos.y);

					cJSON_AddItemToArray(conns_node, conn_node);
				}
			}

			cJSON_AddItemToObject(root, "conns", conns_node);
		}
	}

	return root;
}

bool
_jack_anim(app_t *app)
{
	if(!app->client)
		return true;

	bool realize = false;
	bool quit = false;

	const event_t *ev;
	size_t len;
	while((ev = varchunk_read_request(app->from_jack, &len)))
	{
		switch(ev->type)
		{
			case EVENT_CLIENT_REGISTER:
			{
				if(ev->client_register.state)
				{
					// we create clients upon first port registering
				}
				else
				{
					client_t *client;
					while((client = _client_find_by_name(app, ev->client_register.name,
						JackPortIsInput | JackPortIsOutput)))
					{
						_client_remove(app, client);
						_client_free(app, client);
					}
				}

				if(ev->client_register.name)
					free(ev->client_register.name); // strdup

				realize = true;
			} break;

			case EVENT_PORT_REGISTER:
			{
				jack_port_t *jport = jack_port_by_id(app->client, ev->port_register.id);
				if(jport)
				{
					if(ev->port_register.state)
					{
						port_t *port = _port_find_by_body(app, jport);
						if(!port)
							port = _port_add(app, jport);
					}
					else
					{
						port_t *port = _port_find_by_body(app, jport);
						if(port)
						{
							_port_remove(app, port);
							_port_free(port);
						}
					}
				}

				realize = true;
			} break;

			case EVENT_PORT_CONNECT:
			{
				jack_port_t *source_jport = jack_port_by_id(app->client, ev->port_connect.id_source);
				jack_port_t *sink_jport = jack_port_by_id(app->client, ev->port_connect.id_sink);
				if(source_jport && sink_jport)
				{
					port_t *source_port = _port_find_by_body(app, source_jport);
					port_t *sink_port = _port_find_by_body(app, sink_jport);
					if(source_port && sink_port)
					{
						client_conn_t *client_conn = _client_conn_find_or_add(app, source_port->client, sink_port->client);
						if(client_conn)
						{
							if(ev->port_connect.state)
								_port_conn_add(client_conn, source_port, sink_port);
							else
								_port_conn_remove(app, client_conn, source_port, sink_port);
						}
					}
				}

				realize = true;
			} break;

#ifdef JACK_HAS_METADATA_API
			case EVENT_PROPERTY_CHANGE:
			{
				switch(ev->property_change.state)
				{
					case PropertyCreated:
					{
						// fall-through
					}
					case PropertyChanged:
					{
						char *value = NULL;
						char *type = NULL;
						if(!jack_uuid_empty(ev->property_change.uuid) && ev->property_change.key)
						{
							jack_get_property(ev->property_change.uuid,
								ev->property_change.key, &value, &type);

							if(value)
							{
								if(!strcmp(ev->property_change.key, JACK_METADATA_PRETTY_NAME))
								{
									port_t *port = NULL;
									client_t *client = NULL;
									if((port = _port_find_by_uuid(app, ev->property_change.uuid)))
									{
										free(port->pretty_name);
										port->pretty_name = strdup(value);
									}
									else if((client = _client_find_by_uuid(app, ev->property_change.uuid,
										JackPortIsInput | JackPortIsOutput)))
									{
										free(client->pretty_name);
										client->pretty_name = strdup(value);
									}
								}
								else if(!strcmp(ev->property_change.key, JACKEY_EVENT_TYPES))
								{
									port_t *port = _port_find_by_uuid(app, ev->property_change.uuid);
									if(port)
									{
										port->type = TYPE_NONE;
										if(strcasestr(value, port_labels[TYPE_MIDI]))
											port->type |= TYPE_MIDI;
										if(strcasestr(value, port_labels[TYPE_OSC]))
											port->type |= TYPE_OSC;
										if(port->type == TYPE_NONE)
											port->type |= TYPE_MIDI; // fallback, if none defined
										_client_refresh_type(port->client);
										HASH_FOREACH(&app->conns, client_conn_itr)
										{
											client_conn_t *client_conn = *client_conn_itr;

											_client_conn_refresh_type(client_conn);
										}
									}
								}
								else if(!strcmp(ev->property_change.key, JACKEY_SIGNAL_TYPE))
								{
									port_t *port = _port_find_by_uuid(app, ev->property_change.uuid);
									if(port)
									{
										port->type = !strcasecmp(value, port_labels[TYPE_CV]) ? TYPE_CV : TYPE_AUDIO;
										_client_refresh_type(port->client);
										HASH_FOREACH(&app->conns, client_conn_itr)
										{
											client_conn_t *client_conn = *client_conn_itr;

											_client_conn_refresh_type(client_conn);
										}
									}
								}
								else if(!strcmp(ev->property_change.key, JACKEY_ORDER))
								{
									port_t *port = _port_find_by_uuid(app, ev->property_change.uuid);
									if(port)
									{
										port->order = atoi(value);
										_client_sort(port->client);
									}
								}
								else if(!strcmp(ev->property_change.key, JACKEY_DESIGNATION))
								{
									port_t *port = _port_find_by_uuid(app, ev->property_change.uuid);
									if(port)
									{
										port->designation = _designation_get(value);
										//FIXME do something?
									}
								}
								else if(!strcmp(ev->property_change.key, PATCHMATRIX__mainPositionX))
								{
									client_t *client = _client_find_by_uuid(app, ev->property_change.uuid,
										JackPortIsInput | JackPortIsOutput);
									if(client)
										client->pos.x = atof(value);
								}
								else if(!strcmp(ev->property_change.key, PATCHMATRIX__mainPositionY))
								{
									client_t *client = _client_find_by_uuid(app, ev->property_change.uuid,
										JackPortIsInput | JackPortIsOutput);
									if(client)
										client->pos.y = atof(value);
								}
								else if(!strcmp(ev->property_change.key, PATCHMATRIX__sourcePositionX))
								{
									client_t *client = _client_find_by_uuid(app, ev->property_change.uuid,
										JackPortIsOutput);
									if(client)
										client->pos.x = atof(value);
								}
								else if(!strcmp(ev->property_change.key, PATCHMATRIX__sourcePositionY))
								{
									client_t *client = _client_find_by_uuid(app, ev->property_change.uuid,
										JackPortIsOutput);
									if(client)
										client->pos.y = atof(value);
								}
								else if(!strcmp(ev->property_change.key, PATCHMATRIX__sinkPositionX))
								{
									client_t *client = _client_find_by_uuid(app, ev->property_change.uuid,
										JackPortIsInput);
									if(client)
										client->pos.x = atof(value);
								}
								else if(!strcmp(ev->property_change.key, PATCHMATRIX__sinkPositionY))
								{
									client_t *client = _client_find_by_uuid(app, ev->property_change.uuid,
										JackPortIsInput);
									if(client)
										client->pos.y = atof(value);
								}

								free(value);
							}

							if(type)
								free(type);
						}

						break;
					}
					case PropertyDeleted:
					{
						if(!jack_uuid_empty(ev->property_change.uuid))
						{
							port_t *port = NULL;
							client_t *client = NULL;

							if((port = _port_find_by_uuid(app, ev->property_change.uuid)))
							{
								bool needs_port_update = false;
								bool needs_pretty_update = false;
								bool needs_position_update = false;
								bool needs_designation_update = false;

								if(  ev->property_change.key
									&& ( !strcmp(ev->property_change.key, JACKEY_SIGNAL_TYPE)
										|| !strcmp(ev->property_change.key, JACKEY_EVENT_TYPES) ) )
								{
									needs_port_update = true;
								}
								else if(ev->property_change.key
									&& !strcmp(ev->property_change.key, JACKEY_ORDER))
								{
									needs_position_update = true;
								}
								else if(ev->property_change.key
									&& !strcmp(ev->property_change.key, JACKEY_DESIGNATION))
								{
									needs_designation_update = true;
								}
								else if(ev->property_change.key
									&& !strcmp(ev->property_change.key, JACK_METADATA_PRETTY_NAME))
								{
									needs_pretty_update = true;
								}
								else // all keys removed
								{
									needs_port_update = true;
									needs_pretty_update = true;
									needs_position_update = true;
									needs_designation_update = true;
								}

								if(needs_port_update)
								{
									jack_port_t *jport = jack_port_by_name(app->client, port->name);
									bool midi = 0;

									if(jport)
										midi = !strcmp(jack_port_type(jport), JACK_DEFAULT_MIDI_TYPE) ? true : false;

									port->type = midi ? TYPE_MIDI : TYPE_AUDIO;

									_client_refresh_type(port->client);
									HASH_FOREACH(&app->conns, client_conn_itr)
									{
										client_conn_t *client_conn = *client_conn_itr;

										_client_conn_refresh_type(client_conn);
									}
								}

								if(needs_pretty_update)
								{
									free(port->pretty_name);
									port->pretty_name = NULL;
								}

								if(needs_position_update)
								{
									port->order = 0;

									client_t *client2 = port->client;
									_client_sort(client2);
								}

								if(needs_designation_update)
								{
									port->designation = DESIGNATION_NONE;
									//FIXME do something?
								}
							}
							else if((client = _client_find_by_uuid(app, ev->property_change.uuid,
								JackPortIsInput | JackPortIsOutput)))
							{
								bool needs_pretty_update = false;

								if(ev->property_change.key
									&& !strcmp(ev->property_change.key, JACK_METADATA_PRETTY_NAME))
								{
									needs_pretty_update = true;
								}
								else // all keys removed
								{
									needs_pretty_update = true;
								}

								if(needs_pretty_update)
								{
									free(client->pretty_name);
									client->pretty_name = NULL;
								}
							}
						}
						else
						{
							fprintf(stderr, "all properties in current JACK session deleted\n");
							//TODO
						}

						break;
					}
				}

				if(ev->property_change.key)
					free(ev->property_change.key); // strdup

				realize = true;
			} break;
#endif

			case EVENT_ON_INFO_SHUTDOWN:
			{
				app->client = NULL; // JACK has shut down, hasn't it?

			} break;

			case EVENT_GRAPH_ORDER:
			{
				//FIXME
			} break;

			case EVENT_SESSION:
			{
				jack_session_event_t *jev = ev->session.event;

				asprintf(&jev->command_line, "patchmatrix -u %s -d ${SESSION_DIR}",
					jev->client_uuid);

				switch(jev->type)
				{
					case JackSessionSave:
					case JackSessionSaveAndQuit:
					{
						cJSON *root = _create_session(app);
						if(root)
						{
							_save_session(root, jev->session_dir);
							cJSON_Delete(root);
						}

						if(jev->type == JackSessionSaveAndQuit)
							quit = true;
					} break;
					case JackSessionSaveTemplate:
					{
						// nothing
					} break;
				}

				jack_session_reply(app->client, jev);
				jack_session_event_free(jev);
			} break;

			case EVENT_FREEWHEEL:
			{
				app->freewheel = ev->freewheel.starting;

				realize = true;
			} break;

			case EVENT_BUFFER_SIZE:
			{
				app->buffer_size = ev->buffer_size.nframes;

				realize = true;
			} break;

			case EVENT_SAMPLE_RATE:
			{
				app->sample_rate = ev->sample_rate.nframes;

				realize = true;
			} break;

			case EVENT_XRUN:
			{
				app->xruns += 1;

				realize = true;
			} break;

#ifdef JACK_HAS_PORT_RENAME_CALLBACK
			case EVENT_PORT_RENAME:
			{
				port_t *port = _port_find_by_name(app, ev->port_rename.old_name);
				if(port)
				{
					free(port->name);
					free(port->short_name);

					const char *name = ev->port_rename.new_name;
					char *sep = strchr(name, ':');
					const char *short_name = sep + 1;

					port->name = strdup(name);
					port->short_name = strdup(short_name);
					_client_sort(port->client);
				}

				if(ev->port_rename.old_name)
					free(ev->port_rename.old_name);
				if(ev->port_rename.new_name)
					free(ev->port_rename.new_name);

				realize = true;
			} break;
#endif
		};

		varchunk_read_advance(app->from_jack);
	}

	if(realize)
		nk_pugl_post_redisplay(&app->win);

	return quit;
}

static void
_jack_on_info_shutdown_cb(jack_status_t code, const char *reason, void *arg)
{
	app_t *app = arg;

	event_t *ev;
	if((ev = varchunk_write_request(app->from_jack, sizeof(event_t))))
	{
		ev->type = EVENT_ON_INFO_SHUTDOWN;
		ev->on_info_shutdown.code = code;
		ev->on_info_shutdown.reason = strdup(reason);

		varchunk_write_advance(app->from_jack, sizeof(event_t));
		_ui_signal(app);
	}
}

static void
_jack_freewheel_cb(int starting, void *arg)
{
	app_t *app = arg;

	event_t *ev;
	if((ev = varchunk_write_request(app->from_jack, sizeof(event_t))))
	{
		ev->type = EVENT_FREEWHEEL;
		ev->freewheel.starting = starting;

		varchunk_write_advance(app->from_jack, sizeof(event_t));
		_ui_signal(app);
	}
}

static int
_jack_buffer_size_cb(jack_nframes_t nframes, void *arg)
{
	app_t *app = arg;

	event_t *ev;
	if((ev = varchunk_write_request(app->from_jack, sizeof(event_t))))
	{
		ev->type = EVENT_BUFFER_SIZE;
		ev->buffer_size.nframes = nframes;

		varchunk_write_advance(app->from_jack, sizeof(event_t));
		_ui_signal(app);
	}

	return 0;
}

static int
_jack_sample_rate_cb(jack_nframes_t nframes, void *arg)
{
	app_t *app = arg;

	event_t *ev;
	if((ev = varchunk_write_request(app->from_jack, sizeof(event_t))))
	{
		ev->type = EVENT_SAMPLE_RATE;
		ev->sample_rate.nframes = nframes;

		varchunk_write_advance(app->from_jack, sizeof(event_t));
		_ui_signal(app);
	}

	return 0;
}

static void
_jack_client_registration_cb(const char *name, int state, void *arg)
{
	app_t *app = arg;

	event_t *ev;
	if((ev = varchunk_write_request(app->from_jack, sizeof(event_t))))
	{
		ev->type = EVENT_CLIENT_REGISTER;
		ev->client_register.name = strdup(name);
		ev->client_register.state = state;

		varchunk_write_advance(app->from_jack, sizeof(event_t));
		_ui_signal(app);
	}
}

static void
_jack_port_registration_cb(jack_port_id_t id, int state, void *arg)
{
	app_t *app = arg;

	event_t *ev;
	if((ev = varchunk_write_request(app->from_jack, sizeof(event_t))))
	{
		ev->type = EVENT_PORT_REGISTER;
		ev->port_register.id = id;
		ev->port_register.state = state;

		varchunk_write_advance(app->from_jack, sizeof(event_t));
		_ui_signal(app);
	}
}

#ifdef JACK_HAS_PORT_RENAME_CALLBACK
static void
_jack_port_rename_cb(jack_port_id_t id, const char *old_name, const char *new_name, void *arg)
{
	app_t *app = arg;

	event_t *ev;
	if((ev = varchunk_write_request(app->from_jack, sizeof(event_t))))
	{
		ev->type = EVENT_PORT_RENAME;
		ev->port_rename.old_name = strdup(old_name);
		ev->port_rename.new_name = strdup(new_name);

		varchunk_write_advance(app->from_jack, sizeof(event_t));
		_ui_signal(app);
	}
}
#endif

static void
_jack_port_connect_cb(jack_port_id_t id_source, jack_port_id_t id_sink, int state, void *arg)
{
	app_t *app = arg;

	event_t *ev;
	if((ev = varchunk_write_request(app->from_jack, sizeof(event_t))))
	{
		ev->type = EVENT_PORT_CONNECT;
		ev->port_connect.id_source = id_source;
		ev->port_connect.id_sink = id_sink;
		ev->port_connect.state = state;

		varchunk_write_advance(app->from_jack, sizeof(event_t));
		_ui_signal(app);
	}
}

static int
_jack_xrun_cb(void *arg)
{
	app_t *app = arg;

	event_t *ev;
	if((ev = varchunk_write_request(app->from_jack, sizeof(event_t))))
	{
		ev->type = EVENT_XRUN;

		varchunk_write_advance(app->from_jack, sizeof(event_t));
		_ui_signal(app);
	}

	return 0;
}

static int
_jack_graph_order_cb(void *arg)
{
	app_t *app = arg;

	event_t *ev;
	if((ev = varchunk_write_request(app->from_jack, sizeof(event_t))))
	{
		ev->type = EVENT_GRAPH_ORDER;

		varchunk_write_advance(app->from_jack, sizeof(event_t));
		_ui_signal(app);
	}

	return 0;
}

#ifdef JACK_HAS_METADATA_API
static void
_jack_property_change_cb(jack_uuid_t uuid, const char *key, jack_property_change_t state, void *arg)
{
	app_t *app = arg;

	event_t *ev;
	if((ev = varchunk_write_request(app->from_jack, sizeof(event_t))))
	{
		ev->type = EVENT_PROPERTY_CHANGE;
		ev->property_change.uuid = uuid;
		ev->property_change.key = key ? strdup(key) : NULL;
		ev->property_change.state = state;

		varchunk_write_advance(app->from_jack, sizeof(event_t));
		_ui_signal(app);
	}
}
#endif

static void
_jack_session_cb(jack_session_event_t *jev, void *arg)
{
	app_t *app = arg;

	event_t *ev;
	if((ev = varchunk_write_request(app->from_jack, sizeof(event_t))))
	{
		ev->type = EVENT_SESSION;
		ev->session.event = jev;

		varchunk_write_advance(app->from_jack, sizeof(event_t));
		_ui_signal(app);
	}
}

static void
_jack_populate(app_t *app)
{
	const char **port_names = jack_get_ports(app->client, NULL, NULL, 0);
	if(!port_names)
		return;

	for(const char **itr = port_names; *itr; itr++)
	{
		const char *port_name = *itr;
		jack_port_t *jport = jack_port_by_name(app->client, port_name);

		if(jport)
			_port_add(app, jport);
	}
	jack_free(port_names);

	HASH_FOREACH(&app->clients, client_itr)
	{
		client_t *client = *client_itr;

		HASH_FOREACH(&client->sources, source_port_itr)
		{
			port_t *source_port = *source_port_itr;

			const char **connections = jack_port_get_all_connections(app->client, source_port->body);
			if(!connections)
				continue;

			for(const char **sink_name_ptr = connections; *sink_name_ptr; sink_name_ptr++)
			{
				const char *sink_name = *sink_name_ptr;

				port_t *sink_port = _port_find_by_name(app, sink_name);
				if(!sink_port)
					continue;

				client_conn_t *client_conn = _client_conn_find_or_add(app, source_port->client,  sink_port->client);
				if(client_conn)
					_port_conn_add(client_conn, source_port, sink_port);
			}
			jack_free(connections);
		}
	}
}

static void
_jack_depopulate(app_t *app)
{
	HASH_FREE(&app->conns, client_conn_ptr)
	{
		client_conn_t *client_conn = client_conn_ptr;

		_client_conn_free(client_conn);
	}

	HASH_FREE(&app->clients, client_ptr)
	{
		client_t *client = client_ptr;

		_client_free(app, client);
	}
}

int
_jack_init(app_t *app)
{
	jack_options_t opts = JackNullOption | JackNoStartServer;
	if(app->server_name)
		opts |= JackServerName;
	if(app->session_id)
		opts |= JackSessionID;

	jack_status_t status;
	app->client = jack_client_open("patchmatrix", opts, &status,
		app->server_name ? app->server_name : app->session_id,
		app->server_name ? app->session_id : NULL);
	if(!app->client)
		return -1;

#ifdef JACK_HAS_METADATA_API
	const char *client_name = jack_get_client_name(app->client);
	char *uuid_str = jack_get_uuid_for_client_name(app->client, client_name);
	if(uuid_str)
	{
		jack_uuid_parse(uuid_str, &app->uuid);
		free(uuid_str);
	}
	else
	{
		jack_uuid_clear(&app->uuid);
	}

	if(!jack_uuid_empty(app->uuid))
	{
		jack_set_property(app->client, app->uuid,
			JACK_METADATA_PRETTY_NAME, "PatchMatrix", "text/plain");
	}
#endif

	app->sample_rate = jack_get_sample_rate(app->client);
	app->buffer_size = jack_get_buffer_size(app->client);
	app->xruns = 0;
	app->freewheel = false;
	app->realtime = jack_is_realtime(app->client);

	jack_on_info_shutdown(app->client, _jack_on_info_shutdown_cb, app);

	jack_set_freewheel_callback(app->client, _jack_freewheel_cb, app);
	jack_set_buffer_size_callback(app->client, _jack_buffer_size_cb, app);
	jack_set_sample_rate_callback(app->client, _jack_sample_rate_cb, app);

	jack_set_client_registration_callback(app->client, _jack_client_registration_cb, app);
	jack_set_port_registration_callback(app->client, _jack_port_registration_cb, app);
	jack_set_port_connect_callback(app->client, _jack_port_connect_cb, app);
	jack_set_xrun_callback(app->client, _jack_xrun_cb, app);
	jack_set_graph_order_callback(app->client, _jack_graph_order_cb, app);
	jack_set_session_callback(app->client, _jack_session_cb, app);
#ifdef JACK_HAS_PORT_RENAME_CALLBACK
	jack_set_port_rename_callback(app->client, _jack_port_rename_cb, app);
#endif
#ifdef JACK_HAS_METADATA_API
	jack_set_property_change_callback(app->client, _jack_property_change_cb, app);
#endif

	jack_activate(app->client);

	_jack_populate(app);

	return 0;
}

void
_jack_deinit(app_t *app)
{
	if(!app->client)
		return;

	_jack_depopulate(app);

	jack_deactivate(app->client);

#ifdef JACK_HAS_METADATA_API
	if(!jack_uuid_empty(app->uuid))
		jack_remove_properties(app->client, app->uuid);
#endif

	jack_client_close(app->client);
	app->client = NULL;
}
