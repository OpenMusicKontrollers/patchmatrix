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
#include <pthread.h>
#include <sys/stat.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <d2tk/frontend_pugl.h>
#include <d2tk/hash.h>
#include "subprojects/d2tk/src/base_internal.h" //FIXME

#define NSMC_IMPLEMENTATION
#include <nsmc/nsmc.h>

#define MAX_MIXERS   512
#define MAX_MONITORS 512

typedef enum _node_t {
	NODE_NONE   = 0x000,

	NODE_PLUG   = 0x100,

	NODE_CONN   = 0x200,

	NODE_ARCS   = 0x400,

	NODE_ARCS_A = 0x001,
	NODE_ARCS_B = 0x002,
	NODE_ARCS_C = 0x004,
	NODE_ARCS_D = 0x008,

	NODE_ARCS_E = 0x010,
	NODE_ARCS_F = 0x020,
	NODE_ARCS_G = 0x040,
	NODE_ARCS_H = 0x080
} node_t;

typedef struct _app_config_t app_config_t;
typedef struct _app_session_t app_session_t;
typedef struct _app_mixer_t app_mixer_t;
typedef struct _app_monitor_t app_monitor_t;
typedef struct _app_t app_t;

struct _app_config_t {
	const char *foo;
};

struct _app_session_t {
	bool visibility;
	uint32_t id_offset;
};

struct _app_mixer_t {
	uint32_t id;
};

struct _app_monitor_t {
	uint32_t id;
};

struct _app_t {
	nsmc_t *nsm;
	lua_State *L;
	app_config_t config;
	app_session_t session;
	app_mixer_t *mixers [MAX_MIXERS];
	app_monitor_t *monitors [MAX_MONITORS];
	char *path;

	atomic_bool gui_visible;
	float scale;
	int32_t header_height;
	d2tk_frontend_t *dpugl;
	pthread_t ui_thread;
};

static atomic_bool done = ATOMIC_VAR_INIT(false);

static void
_sig_interrupt(int signum)
{
	atomic_store_explicit(&done, true, memory_order_release);
}

static const char *
_bool_serialize(bool val)
{
	static const char *true_str = "true";
	static const char *false_str = "false";

	return val ? true_str : false_str;
}

static int
_config_deserialize(lua_State *L, app_config_t *config)
{
	if(!lua_istable(L, 1))
	{
		return -1;
	}

	lua_getfield(L, 1, "foo");
	config->foo = luaL_optstring(L, -1, "unknown");
	lua_pop(L, 1);

	return 0;
}

static int
_lpatchmatrix_config(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));
	app_config_t *config = &app->config;

	if(_config_deserialize(L, config) != 0)
	{
		luaL_error(L, "[%s] config deserialize failed", __func__);
	}

	fprintf(stderr, "foo -> %s\n", config->foo);

	return 0;
}

static int
_session_deserialize(lua_State *L, app_session_t *session)
{
	if(!lua_istable(L, 1))
	{
		return -1;
	}

	lua_getfield(L, 1, "visibility");
	session->visibility = lua_toboolean(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "id_offset");
	session->id_offset = luaL_optinteger(L, -1, 0);
	lua_pop(L, 1);

	return 0;
}

static int
_session_serialize(FILE *fout, app_session_t *session)
{
	fprintf(fout,
		"patchmatrix.session {\n"
		"\tvisibility = %s,\n"
		"\tid_offset = %"PRIu32",\n"
		"}\n\n",
		_bool_serialize(session->visibility),
		session->id_offset);

	return 0;
}

static int
_lpatchmatrix_session(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));
	app_session_t *session = &app->session;

	if(_session_deserialize(L, session) != 0)
	{
		luaL_error(L, "[%s] session deserialization failed", __func__);
	}

	return 0;
}

static int
_mixer_deserialize(lua_State *L, app_mixer_t *mixer)
{
	if(!lua_istable(L, 1))
	{
		return -1;
	}

	lua_getfield(L, 1, "id");
	mixer->id = luaL_optinteger(L, -1, 0);
	lua_pop(L, 1);

	return 0;
}

static int
_mixer_serialize(FILE *fout, app_mixer_t *mixer)
{
	fprintf(fout,
		"patchmatrix.mixer {\n"
		"\tid = %"PRIu32",\n"
		"}\n\n",
		mixer->id);

	return 0;
}

static int
_lpatchmatrix_mixer(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));
	app_session_t *session = &app->session;

	app_mixer_t *mixer = calloc(1, sizeof(app_mixer_t));

	if(_mixer_deserialize(L, mixer) != 0)
	{
		luaL_error(L, "[%s] mixer deserialize failed", __func__);
	}

	if(mixer->id == 0)
	{
		mixer->id = session->id_offset++;
	}

	app->mixers[0] = mixer; //FIXME

	return 0;
}

static int
_monitor_deserialize(lua_State *L, app_monitor_t *monitor)
{
	if(!lua_istable(L, 1))
	{
		return -1;
	}

	lua_getfield(L, 1, "id");
	monitor->id = luaL_optinteger(L, -1, 0);
	lua_pop(L, 1);

	return 0;
}

static int
_monitor_serialize(FILE *fout, app_monitor_t *monitor)
{
	fprintf(fout,
		"patchmatrix.monitor {\n"
		"\tid = %"PRIu32",\n"
		"}\n\n",
		monitor->id);

	return 0;
}

static int
_lpatchmatrix_monitor(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));
	app_session_t *session = &app->session;

	app_monitor_t *monitor = calloc(1, sizeof(app_monitor_t));

	if(_monitor_deserialize(L, monitor) != 0)
	{
		luaL_error(L, "[%s] monitor deserialize failed", __func__);
	}

	if(monitor->id == 0)
	{
		monitor->id = session->id_offset++;
	}

	app->monitors[0] = monitor; //FIXME

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
		return nsmc_opened(app->nsm, 0);
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

	FILE *fout = fopen(session_path, "wb");

	if(!fout)
	{
		nsmc_saved(app->nsm, -1); //FIXME
	}

	const int res = _session_serialize(fout, &app->session);

	fclose(fout);

	return nsmc_saved(app->nsm, res);
}

static inline void
_expose_header(app_t *app, const d2tk_rect_t *rect)
{
	d2tk_frontend_t *dpugl = app->dpugl;
	d2tk_base_t *base = d2tk_frontend_get_base(dpugl);

	const d2tk_coord_t frac [3] = { 1, 1, 1 };
	D2TK_BASE_LAYOUT(rect, 3, frac, D2TK_FLAG_LAYOUT_X_REL, lay)
	{
		const unsigned k = d2tk_layout_get_index(lay);
		const d2tk_rect_t *lrect = d2tk_layout_get_rect(lay);

		switch(k)
		{
			case 0:
			{
				d2tk_base_label(base, -1, "Open•Music•Kontrollers", 0.5f, lrect,
					D2TK_ALIGN_LEFT | D2TK_ALIGN_TOP);
			} break;
			case 1:
			{
				d2tk_base_label(base, -1, "Patch•Matrix", 1.f, lrect,
					D2TK_ALIGN_CENTER | D2TK_ALIGN_TOP);
			} break;
			case 2:
			{
				d2tk_base_label(base, -1, "Version "PATCHMATRIX_VERSION, 0.5f, lrect,
					D2TK_ALIGN_RIGHT | D2TK_ALIGN_TOP);
			} break;
		}
	}
}

static inline void
_expose_node(app_t *app, unsigned k, const d2tk_rect_t *rect)
{
	d2tk_frontend_t *dpugl = app->dpugl;
	d2tk_base_t *base = d2tk_frontend_get_base(dpugl);

	char lbl [16];
	const size_t lbl_len = snprintf(lbl, sizeof(lbl), "Node-%02x", k);

	d2tk_rect_t bnd;
	d2tk_rect_shrink(&bnd, rect, -rect->h/4);

	D2TK_BASE_FRAME(base, &bnd, lbl_len, lbl, frm)
	{
		const d2tk_rect_t *frect = d2tk_frame_get_rect(frm);

		const d2tk_coord_t frac [4] = { 1, 1, 1, 1 };
		D2TK_BASE_LAYOUT(frect, 4, frac, D2TK_FLAG_LAYOUT_Y_REL, lay)
		{
			const d2tk_rect_t *lrect = d2tk_layout_get_rect(lay);
			const unsigned l = (k << 8) | d2tk_layout_get_index(lay);

			int32_t val = -48 + k/4;
			d2tk_base_meter(base, D2TK_ID_IDX(l), lrect, &val);
		}
	}
}

static inline void
_expose_conn(app_t *app, const d2tk_rect_t *rect)
{
	d2tk_frontend_t *dpugl = app->dpugl;
	d2tk_base_t *base = d2tk_frontend_get_base(dpugl);

	const d2tk_style_t *style = d2tk_base_get_style(base);

	const d2tk_hash_dict_t dict [] = {
		{ rect, sizeof(d2tk_rect_t) },
		{ style, sizeof(d2tk_style_t) },
		{ NULL, 0 }
	};
	const uint64_t hash = d2tk_hash_dict(dict);

	d2tk_core_t *core = base->core;

	D2TK_CORE_WIDGET(core, hash, widget)
	{
		d2tk_rect_t bnd;
		d2tk_rect_shrink(&bnd, rect, style->padding);

		const d2tk_triple_t triple = D2TK_TRIPLE_ACTIVE_HOT_FOCUS;

		const size_t ref = d2tk_core_bbox_push(core, true, rect);

		const d2tk_coord_t x1 = rect->x;
		const d2tk_coord_t y1 = rect->y + rect->h/2;

		const d2tk_coord_t x2 = rect->x + rect->w/2;
		const d2tk_coord_t y2 = rect->y + rect->h;

		const d2tk_coord_t x3 = rect->x + rect->w;
		const d2tk_coord_t y3 = rect->y + rect->h/2;

		const d2tk_coord_t x4 = rect->x + rect->w/2;
		const d2tk_coord_t y4 = rect->y;

		d2tk_core_begin_path(core);
		d2tk_core_move_to(core, x1, y1);
		d2tk_core_line_to(core, x2, y2);
		d2tk_core_line_to(core, x3, y3);
		d2tk_core_line_to(core, x4, y4);
		d2tk_core_close_path(core);
		d2tk_core_color(core, style->stroke_color[triple]);
		d2tk_core_stroke_width(core, style->border_width);
		d2tk_core_stroke(core);

		d2tk_core_bbox_pop(core, ref);
	}
}

static inline void
_expose_arcs(app_t *app, node_t node, const d2tk_rect_t *rect)
{
	d2tk_frontend_t *dpugl = app->dpugl;
	d2tk_base_t *base = d2tk_frontend_get_base(dpugl);

	const d2tk_style_t *style = d2tk_base_get_style(base);

	const d2tk_hash_dict_t dict [] = {
		{ rect, sizeof(d2tk_rect_t) },
		{ style, sizeof(d2tk_style_t) },
		{ &node, sizeof(node_t) },
		{ NULL, 0 }
	};
	const uint64_t hash = d2tk_hash_dict(dict);

	d2tk_core_t *core = base->core;

	D2TK_CORE_WIDGET(core, hash, widget)
	{
		d2tk_rect_t bnd;
		d2tk_rect_shrink(&bnd, rect, style->padding);

		const d2tk_triple_t triple = D2TK_TRIPLE_ACTIVE_HOT_FOCUS;

		const size_t ref = d2tk_core_bbox_push(core, true, rect);

		d2tk_core_color(core, style->stroke_color[triple]);
		d2tk_core_stroke_width(core, style->border_width);

		if(node & NODE_ARCS_A)
		{
			const d2tk_coord_t r = rect->h/2;
			const d2tk_coord_t x = rect->x;
			const d2tk_coord_t y = rect->y;

			d2tk_core_begin_path(core);
			d2tk_core_arc(core, x, y, r, 0 + 90, 0, false);
			d2tk_core_stroke(core);
		}
		if(node & NODE_ARCS_B)
		{
			const d2tk_coord_t r = rect->h/2;
			const d2tk_coord_t x = rect->x;
			const d2tk_coord_t y = rect->y + rect->h;

			d2tk_core_begin_path(core);
			d2tk_core_arc(core, x, y, r, 270 + 90, 270, false);
			d2tk_core_stroke(core);
		}
		if(node & NODE_ARCS_C)
		{
			const d2tk_coord_t r = rect->h/2;
			const d2tk_coord_t x = rect->x + rect->w;
			const d2tk_coord_t y = rect->y + rect->h;

			d2tk_core_begin_path(core);
			d2tk_core_arc(core, x, y, r, 180 + 90, 180, false);
			d2tk_core_stroke(core);
		}
		if(node & NODE_ARCS_D)
		{
			const d2tk_coord_t r = rect->h/2;
			const d2tk_coord_t x = rect->x + rect->w;
			const d2tk_coord_t y = rect->y;

			d2tk_core_begin_path(core);
			d2tk_core_arc(core, x, y, r, 90 + 90, 90, false);
			d2tk_core_stroke(core);
		}

		if(node & NODE_ARCS_E)
		{
			const d2tk_coord_t x0 = rect->x;
			const d2tk_coord_t y0 = rect->y + rect->h/2;
			const d2tk_coord_t x1 = x0 + rect->w/2;
			const d2tk_coord_t y1 = y0;

			d2tk_core_begin_path(core);
			d2tk_core_move_to(core, x0, y0);
			d2tk_core_line_to(core, x1, y1);
			d2tk_core_stroke(core);
		}
		if(node & NODE_ARCS_F)
		{
			const d2tk_coord_t x0 = rect->x + rect->w/2;;
			const d2tk_coord_t y0 = rect->y + rect->h/2;
			const d2tk_coord_t x1 = x0;
			const d2tk_coord_t y1 = y0 + rect->h/2;

			d2tk_core_begin_path(core);
			d2tk_core_move_to(core, x0, y0);
			d2tk_core_line_to(core, x1, y1);
			d2tk_core_stroke(core);
		}
		if(node & NODE_ARCS_G)
		{
			const d2tk_coord_t x0 = rect->x + rect->w/2;
			const d2tk_coord_t y0 = rect->y + rect->h/2;
			const d2tk_coord_t x1 = x0 + rect->w/2;
			const d2tk_coord_t y1 = y0;

			d2tk_core_begin_path(core);
			d2tk_core_move_to(core, x0, y0);
			d2tk_core_line_to(core, x1, y1);
			d2tk_core_stroke(core);
		}
		if(node & NODE_ARCS_H)
		{
			const d2tk_coord_t x0 = rect->x + rect->w/2;;
			const d2tk_coord_t y0 = rect->y;
			const d2tk_coord_t x1 = x0;
			const d2tk_coord_t y1 = y0 + rect->h/2;

			d2tk_core_begin_path(core);
			d2tk_core_move_to(core, x0, y0);
			d2tk_core_line_to(core, x1, y1);
			d2tk_core_stroke(core);
		}

		d2tk_core_bbox_pop(core, ref);
	}
}

static inline void
_expose_body(app_t *app, const d2tk_rect_t *rect)
{
#define M 12
#define N 12
	static const node_t grid [M][N] = {
		{ 0x404, 0x450, 0x450, 0x450, 0x450, 0x450, 0x450, 0x450, 0x402, 0x000, 0x000, 0x000 },
		{ 0x408, 0x200, 0x450, 0x100, 0x452, 0x200, 0x450, 0x100, 0x401, 0x000, 0x000, 0x000 },
		{ 0x000, 0x000, 0x000, 0x000, 0x4a0, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000 },
		{ 0x000, 0x000, 0x000, 0x000, 0x408, 0x200, 0x402, 0x000, 0x000, 0x000, 0x000, 0x000 },
		{ 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x4a0, 0x000, 0x000, 0x000, 0x000, 0x000 },
		{ 0x404, 0x200, 0x450, 0x100, 0x450, 0x200, 0x45c, 0x100, 0x000, 0x000, 0x000, 0x000 },
		{ 0x4a0, 0x000, 0x000, 0x000, 0x000, 0x000, 0x4a0, 0x000, 0x000, 0x000, 0x000, 0x000 },
		{ 0x4a0, 0x000, 0x000, 0x000, 0x404, 0x200, 0x401, 0x000, 0x000, 0x000, 0x000, 0x000 },
		{ 0x408, 0x450, 0x450, 0x450, 0x4a2, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000, 0x000 },
		{ 0x000, 0x000, 0x000, 0x100, 0x453, 0x200, 0x450, 0x100, 0x450, 0x200, 0x454, 0x100 },
		{ 0x000, 0x000, 0x000, 0x000, 0x4a0, 0x000, 0x000, 0x000, 0x000, 0x000, 0x4a0, 0x000 },
		{ 0x000, 0x000, 0x000, 0x000, 0x408, 0x450, 0x450, 0x450, 0x450, 0x200, 0x401, 0x000 }
	};

	const d2tk_coord_t S = 80 * app->scale; //FIXME do only once
	D2TK_BASE_TABLE(rect, S, S, D2TK_FLAG_TABLE_ABS, tab)
	{
		const d2tk_rect_t *trect = d2tk_table_get_rect(tab);
		const unsigned n = d2tk_table_get_index_x(tab);
		const unsigned m = d2tk_table_get_index_y(tab);
		const unsigned k = d2tk_table_get_index(tab);

		if(m >= M)
		{
			break;
		}

		if(n >= N)
		{
			continue;
		}

		node_t node = grid[m][n];

		if(node & NODE_PLUG)
		{
			_expose_node(app, k, trect);
		}
		else if(node & NODE_CONN)
		{
			_expose_conn(app, trect);
		}
		else if(node & NODE_ARCS)
		{
			_expose_arcs(app, node, trect);
		}
	}
#undef N
#undef M
}

static int
_expose(void *data, d2tk_coord_t w, d2tk_coord_t h)
{
	app_t *app = data;
	const d2tk_rect_t rect = D2TK_RECT(0, 0, w, h);

	const d2tk_coord_t frac [2] = { app->header_height, 0 };
	D2TK_BASE_LAYOUT(&rect, 2, frac, D2TK_FLAG_LAYOUT_Y_ABS, lay)
	{
		const unsigned k = d2tk_layout_get_index(lay);
		const d2tk_rect_t *lrect = d2tk_layout_get_rect(lay);

		switch(k)
		{
			case 0:
			{
				_expose_header(app, lrect);
			} break;
			case 1:
			{
				_expose_body(app, lrect);
			} break;
		}
	}

	return 0;
}

static void *
_ui_thread(void *data)
{
	app_t *app = data;

	d2tk_pugl_config_t config;
	uintptr_t widget;

	const d2tk_coord_t w = 800;
	const d2tk_coord_t h = 800;

	memset(&config, 0x0, sizeof(config));
	config.bundle_path = "./"; //FIXME
	config.min_w = w/2;
	config.min_h = h/2;
	config.w = w;
	config.h = h;
	config.fixed_size = false;
	config.fixed_aspect = false;
	config.expose = _expose;
	config.data = app;

	app->dpugl = d2tk_pugl_new(&config, &widget);
	if(!app->dpugl)
	{	
		return NULL;
	}

	app->scale = d2tk_frontend_get_scale(app->dpugl);

	app->header_height = 32 * app->scale;

	while(atomic_load_explicit(&app->gui_visible, memory_order_acquire))
	{
		if(d2tk_frontend_poll(app->dpugl, 0.1) != 0)
		{
			atomic_store_explicit(&app->gui_visible, false, memory_order_release);
		}
	}

	d2tk_frontend_free(app->dpugl);

	return NULL;
}

static int
_show(app_t *app)
{
	if(atomic_exchange(&app->gui_visible, true) == true)
	{
		return 0;
	}

	app->session.visibility = true;
	return pthread_create(&app->ui_thread, NULL, _ui_thread, app);
}

static int
_hide(app_t *app)
{
	if(atomic_exchange(&app->gui_visible, false) == false)
	{
		return 0;
	}

	app->session.visibility = false;
	pthread_join(app->ui_thread, NULL);

	return 0;
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
				| NSMC_CAPABILITY_SWITCH;

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

	atomic_init(&app.gui_visible, false);

	const char *exe = strrchr(argv[0], '/');
	exe = exe ? exe + 1 : argv[0];
	const char *fallback_path = argv[optind]
		? argv[optind]
		: "/tmp/patchmatrix"; //FIXME
	app.nsm = nsmc_new("PatchMatrix", exe, fallback_path, _nsm_callback, &app);

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

		// check if user closed the gui
		const bool old_visibility = app.session.visibility;
		app.session.visibility = atomic_load_explicit(&app.gui_visible, memory_order_acquire);
		if(old_visibility && !app.session.visibility)
		{
			_hide(&app);
			nsmc_hidden(app.nsm);
		}
	}

	nsmc_free(app.nsm);

	lua_close(app.L);

	free(app.path);

	return 0;
}
