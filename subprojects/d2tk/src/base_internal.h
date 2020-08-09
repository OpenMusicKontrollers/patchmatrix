/*
 * Copyright (c) 2018-2019 Hanspeter Portner (dev@open-music-kontrollers.ch)
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

#include <stdatomic.h>

#include <d2tk/base.h>
#include <d2tk/hash.h>
#include "core_internal.h"

#define _D2TK_MAX_ATOM 0x1000
#define _D2TK_MASK_ATOMS (_D2TK_MAX_ATOM - 1)

typedef enum _d2tk_atom_type_t {
	D2TK_ATOM_NONE,
	D2TK_ATOM_SCROLL,
	D2TK_ATOM_PANE,
	D2TK_ATOM_FLOW,
	D2TK_ATOM_FLOW_NODE,
	D2TK_ATOM_FLOW_ARC,
#if D2TK_PTY
	D2TK_ATOM_PTY,
#endif
#if D2TK_EVDEV
	D2TK_ATOM_VKB,
#endif
} d2tk_atom_type_t;

typedef enum _d2tk_atom_event_type_t {
	D2TK_ATOM_EVENT_NONE,
	D2TK_ATOM_EVENT_PROBE,
	D2TK_ATOM_EVENT_DEINIT
} d2tk_atom_event_type_t;

typedef struct _d2tk_flip_t d2tk_flip_t;
typedef struct _d2tk_atom_t d2tk_atom_t;
typedef int (*d2tk_atom_event_t)(d2tk_atom_event_type_t event, void *data);

struct _d2tk_flip_t {
	d2tk_id_t old;
	d2tk_id_t cur;
};

struct _d2tk_atom_t {
	d2tk_id_t id;
	d2tk_atom_type_t type;
	void *body;
	d2tk_atom_event_t event;
};

struct _d2tk_base_t {
	d2tk_flip_t hotitem;
	d2tk_flip_t activeitem;
	d2tk_flip_t focusitem;
	d2tk_id_t lastitem;

	bool not_first_time;

	struct {
		d2tk_coord_t x;
		d2tk_coord_t y;
		d2tk_coord_t ox;
		d2tk_coord_t oy;
		d2tk_coord_t dx;
		d2tk_coord_t dy;
		d2tk_butmask_t mask;
		d2tk_butmask_t mask_prev;
	} mouse;

	struct {
		int32_t odx;
		int32_t ody;
		int32_t dx;
		int32_t dy;
	} scroll;

	struct {
		size_t nchars;
		utf8_int32_t chars [32];
		unsigned keymod;
		d2tk_keymask_t mask;
		d2tk_keymask_t mask_prev;
		d2tk_modmask_t mod;
	} keys;

	struct {
		char text_in [1024];
		char text_out [1024];
	} edit;

	const d2tk_style_t *style;

	atomic_bool again;
	bool clear_focus;
	bool focused;

	d2tk_core_t *core;

	d2tk_atom_t atoms [_D2TK_MAX_ATOM];
};

extern const size_t d2tk_atom_body_flow_sz;
extern const size_t d2tk_atom_body_pane_sz;
extern const size_t d2tk_atom_body_scroll_sz;
#if D2TK_PTY
extern const size_t d2tk_atom_body_pty_sz;
#endif
#if D2TK_EVDEV
extern const size_t d2tk_atom_body_vkb_sz;
#endif

void *
_d2tk_base_get_atom(d2tk_base_t *base, d2tk_id_t id, d2tk_atom_type_t type,
	d2tk_atom_event_t event);

d2tk_state_t
_d2tk_base_is_active_hot_vertical_scroll(d2tk_base_t *base);

d2tk_state_t
_d2tk_base_is_active_hot_horizontal_scroll(d2tk_base_t *base);

void
_d2tk_base_clear_chars(d2tk_base_t *base);
