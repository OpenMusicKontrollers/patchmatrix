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

#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#define NSMC_IMPLEMENTATION
#include <nsmc/nsmc.h>

typedef struct _app_config_t app_config_t;
typedef struct _app_session_t app_session_t;
typedef struct _app_t app_t;

struct _app_config_t {
	const char *foo;
};

struct _app_session_t {
	bool visibility;
};

struct _app_t {
	nsmc_t *nsm;
	lua_State *L;
	app_config_t config;
	app_session_t session;
	char *path;
};

static atomic_bool done = ATOMIC_VAR_INIT(false);

static void
_sig_interrupt(int signum)
{
	atomic_store_explicit(&done, true, memory_order_release);
}

static int
_lpatchmatrix_config(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));
	app_config_t *config = &app->config;

	if(!lua_istable(L, 1))
	{
		return 0;
	}

	lua_getfield(L, 1, "foo");
	config->foo = luaL_optstring(L, -1, "unknown");
	lua_pop(L, 1);

	fprintf(stderr, "foo -> %s\n", config->foo);

	return 0;
}

static int
_lpatchmatrix_session(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));
	app_session_t *session = &app->session;

	if(!lua_istable(L, 1))
	{
		return 0;
	}

	lua_getfield(L, 1, "visibility");
	session->visibility = lua_toboolean(L, -1);
	lua_pop(L, 1);

	fprintf(stderr, "visibility -> %i\n", session->visibility);

	return 0;
}

static int
_lpatchmatrix_mixer(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));

	(void)app;
	//FIXME
	return 0;
}

static int
_lpatchmatrix_monitor(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));

	(void)app;
	//FIXME
	return 0;
}

static luaL_Reg lpatchmatrix [] = {
	{
		.name = "config",
		.func = _lpatchmatrix_config
	},
	{
		.name = "session",
		.func = _lpatchmatrix_session
	},
	{
		.name = "mixer",
		.func = _lpatchmatrix_mixer
	},
	{
		.name = "monitor",
		.func = _lpatchmatrix_monitor
	},
	{
		.name = NULL,
		.func = NULL
	}
};

static int
_file_exists(const char *path)
{
	return access(path, F_OK);
}

static int
_file_write(const char *path, const uint8_t *body, size_t body_len)
{
		FILE *fout = fopen(path, "wb");

		if(!fout)
		{
			fprintf(stderr, "[%s] fopen: %s\n", __func__, strerror(errno));
			return 1;
		}

		const size_t written_len = fwrite(body, body_len, 1, fout);

		fclose(fout);

		if(written_len != body_len)
		{
			//FIXME
			return -1;
		}

		return 0;
}

static int
_config_load(app_t *app)
{
	char config_path [PATH_MAX];
	const char *home = getenv("HOME");

	snprintf(config_path, sizeof(config_path),
		"%s/.config/patchmatrix", home);

	if(_file_exists(config_path) != 0)
	{
		mkdir(config_path, 0755);
	}

	snprintf(config_path, sizeof(config_path),
		"%s/.config/patchmatrix/config.lua", home);

	if(_file_exists(config_path) != 0)
	{
		static const uint8_t default_config [] =
			"patchmatrix.config {\n"
			"	foo = 'bar'\n"
			"}";

		_file_write(config_path, default_config, sizeof(default_config) - 1);
	}

	return luaL_dofile(app->L, config_path);
}

static int
_open(app_t *app, const char *path, const char *name, const char *id)
{
	char session_path [PATH_MAX];

	if(app->path)
	{
		free(app->path);
	}
	app->path = strdup(path);

	snprintf(session_path, sizeof(session_path),
		"%s/session.lua", path);

	if(_file_exists(session_path) != 0)
	{
		return nsmc_opened(app->nsm, 0);;
	}

	luaL_dofile(app->L, session_path); //FIXME check

	return nsmc_opened(app->nsm, 0);;
}

static int
_save(app_t *app, const char *path)
{
	char session_path [PATH_MAX];

	snprintf(session_path, sizeof(session_path),
		"%s", path);

	if(_file_exists(session_path) != 0)
	{
		mkdir(session_path, 0755);
	}

	snprintf(session_path, sizeof(session_path),
		"%s/session.lua", path);

	static const uint8_t default_session [] =
		"patchmatrix.session{\n"
		"	visibility = true\n"
		"}";

	const int res = _file_write(session_path,
		default_session, sizeof(default_session) - 1);

	return nsmc_saved(app->nsm, res);
}

static int
_show(app_t *app)
{
	//FIXME
	
	app->session.visibility = true;
	
	return nsmc_shown(app->nsm);
}

static int
_hide(app_t *app)
{
	//FIXME
	
	app->session.visibility = false;
	
	return nsmc_hidden(app->nsm);
}

static int
_nsm_callback(void *data, const nsmc_event_t *ev)
{
	app_t *app = data;
	app_session_t *session = &app->session;

	switch(ev->type)
	{
		case NSMC_EVENT_TYPE_OPEN:
			return _open(app, ev->open.path, ev->open.name, ev->open.id);
		case NSMC_EVENT_TYPE_SAVE:
			return _save(app, app->path);
		case NSMC_EVENT_TYPE_SHOW:
			return _show(app);
		case NSMC_EVENT_TYPE_HIDE:
			return _hide(app);
		case NSMC_EVENT_TYPE_SESSION_IS_LOADED:
			return 0;

		case NSMC_EVENT_TYPE_VISIBILITY:
			return session->visibility;
		case NSMC_EVENT_TYPE_CAPABILITY:
			return NSMC_CAPABILITY_MESSAGE
				| NSMC_CAPABILITY_OPTIONAL_GUI
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
	static app_t app;

	fprintf(stderr,
		"%s "PATCHMATRIX_VERSION"\n"
		"Copyright (c) 2016-2020 Hanspeter Portner (dev@open-music-kontrollers.ch)\n"
		"Released under Artistic License 2.0 by Open Music Kontrollers\n", argv[0]);

	int c;
	while( (c = getopt(argc, argv, "vh")) != -1)
	{
		switch(c)
		{
			case 'v':
			{
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
			}	return 0;
			case 'h':
			{
				fprintf(stderr,
					"--------------------------------------------------------------------\n"
					"USAGE\n"
					"   %s [OPTIONS]\n"
					"\n"
					"OPTIONS\n"
					"   [-v]                 print version and full license information\n"
					"   [-h]                 print usage information\n\n"
					, argv[0]);
			}	return 0;
			case '?':
			{
				if(isprint(optopt))
				{
					fprintf(stderr, "Unknown option `-%c'.\n", optopt);
				}
				else
				{
					fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
				}
			} return -1;
			default:
			{
				return -1;
			}
		}
	}

	signal(SIGINT, _sig_interrupt);

	app.L = luaL_newstate();

	luaL_requiref(app.L, "base", luaopen_base, 0);

	luaL_requiref(app.L, "coroutine", luaopen_coroutine, 1);
	luaL_requiref(app.L, "table", luaopen_table, 1);
	luaL_requiref(app.L, "string", luaopen_string, 1);
	luaL_requiref(app.L, "math", luaopen_math, 1);
	luaL_requiref(app.L, "utf8", luaopen_utf8, 1);

	luaL_newlibtable(app.L, lpatchmatrix);
	lua_pushlightuserdata(app.L, &app);
  luaL_setfuncs(app.L, lpatchmatrix, 1);
	lua_setglobal(app.L, "patchmatrix");

	_config_load(&app);

	const char *exe = strrchr(argv[0], '/');
	exe = exe ? exe + 1 : argv[0];
	app.nsm = nsmc_new("PATCHMATRIX", exe, argv[optind], _nsm_callback, &app);

	if(!app.nsm)
	{
		fprintf(stderr, "[%s] nsmc_new failed\n", __func__);
		return 1;
	}

	while(!atomic_load_explicit(&done, memory_order_acquire))
	{
		if(nsmc_managed())
		{
			nsmc_pollin(app.nsm, 1000);
		}
		else
		{
			usleep(1000);
		}
	}

	nsmc_free(app.nsm);

	lua_close(app.L);

	free(app.path);

	return 0;
}
