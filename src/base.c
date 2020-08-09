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

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <inttypes.h>

#include "base_internal.h"

static inline d2tk_id_t
_d2tk_flip_get_cur(d2tk_flip_t *flip)
{
	return flip->cur;
}

static inline d2tk_id_t
_d2tk_flip_get_old(d2tk_flip_t *flip)
{
	return flip->old;
}

static inline bool
_d2tk_flip_equal_cur(d2tk_flip_t *flip, d2tk_id_t id)
{
	return _d2tk_flip_get_cur(flip) == id;
}

static inline bool
_d2tk_flip_equal_old(d2tk_flip_t *flip, d2tk_id_t id)
{
	return _d2tk_flip_get_old(flip) == id;
}

static inline bool
_d2tk_flip_invalid(d2tk_flip_t *flip)
{
	return _d2tk_flip_equal_cur(flip, 0);
}

static inline bool
_d2tk_flip_invalid_old(d2tk_flip_t *flip)
{
	return _d2tk_flip_equal_old(flip, 0);
}

static inline void
_d2tk_flip_set_old(d2tk_flip_t *flip, d2tk_id_t old)
{
	flip->old = old;
}

static inline void
_d2tk_flip_set(d2tk_flip_t *flip, d2tk_id_t new)
{
	if(_d2tk_flip_invalid_old(flip))
	{
		_d2tk_flip_set_old(flip, flip->cur);
	}

	flip->cur = new;
}

static inline void
_d2tk_flip_clear(d2tk_flip_t *flip)
{
	_d2tk_flip_set(flip, 0);
}

static inline void
_d2tk_flip_clear_old(d2tk_flip_t *flip)
{
	_d2tk_flip_set_old(flip, 0);
}

void *
_d2tk_base_get_atom(d2tk_base_t *base, d2tk_id_t id, d2tk_atom_type_t type,
	d2tk_atom_event_t event)
{
	for(unsigned i = 0, idx = (id + i*i) & _D2TK_MASK_ATOMS;
		i < _D2TK_MAX_ATOM;
		i++, idx = (id + i*i) & _D2TK_MASK_ATOMS)
	{
		d2tk_atom_t *atom = &base->atoms[idx];

		if( (atom->id != 0) && (atom->id != id) )
		{
			continue;
		}

		if( (atom->id == 0) || (atom->type != type) || !atom->body) // new atom or changed type
		{
			atom->id = id;
			atom->type = type;
			atom->event = event;

			size_t len;
			switch(atom->type)
			{
				case D2TK_ATOM_SCROLL:
				{
					len = d2tk_atom_body_scroll_sz;
				} break;
				case D2TK_ATOM_PANE:
				{
					len = d2tk_atom_body_pane_sz;
				} break;
				case D2TK_ATOM_FLOW:
				{
					len = d2tk_atom_body_flow_sz;
				} break;
#if D2TK_PTY
				case D2TK_ATOM_PTY:
				{
					len = d2tk_atom_body_pty_sz;
				} break;
#endif
#if D2TK_EVDEV
				case D2TK_ATOM_VKB:
				{
					len = d2tk_atom_body_vkb_sz;
				} break;
#endif
				case D2TK_ATOM_FLOW_NODE:
					// fall-through
				case D2TK_ATOM_FLOW_ARC:
					// fall-through
				default:
				{
					len = 0;
				} break;
			}

			if(len == 0)
			{
				if(atom->event)
				{
					atom->event(D2TK_ATOM_EVENT_DEINIT, atom->body);
					atom->event = NULL;
				}
				free(atom->body);
				atom->body = 0;
			}
			else
			{
				void *body = realloc(atom->body, len);
				if(!body)
				{
					return NULL;
				}

				memset(body, 0x0, len);
				atom->body = body;
			}
		}

		return atom->body;
	}

	return NULL; // no space left
}

D2TK_API void
d2tk_clip_int32(int32_t min, int32_t *value, int32_t max)
{
	if(*value < min)
	{
		*value = min;
	}
	else if(*value > max)
	{
		*value = max;
	}
}

D2TK_API void
d2tk_clip_int64(int64_t min, int64_t *value, int64_t max)
{
	if(*value < min)
	{
		*value = min;
	}
	else if(*value > max)
	{
		*value = max;
	}
}

D2TK_API void
d2tk_clip_float(float min, float *value, float max)
{
	if(*value < min)
	{
		*value = min;
	}
	else if(*value > max)
	{
		*value = max;
	}
}

D2TK_API void
d2tk_clip_double(double min, double *value, double max)
{
	if(*value < min)
	{
		*value = min;
	}
	else if(*value > max)
	{
		*value = max;
	}
}

D2TK_API bool
d2tk_base_get_mod(d2tk_base_t *base)
{
	return base->keys.mod != D2TK_MODMASK_NONE;
}

D2TK_API const char *
d2tk_state_dump(d2tk_state_t state)
{
#define LEN 16
	static char buf [LEN + 1];

	for(unsigned i = 0; i < LEN; i++)
	{
		buf[LEN - 1 - i] = (1 << i) & state
			? '1'
			: '.';
	}

	buf[LEN] = '\0';

	return buf;
#undef LEN
}

D2TK_API bool
d2tk_state_is_down(d2tk_state_t state)
{
	return (state & D2TK_STATE_DOWN);
}

D2TK_API bool
d2tk_state_is_up(d2tk_state_t state)
{
	return (state & D2TK_STATE_UP);
}

D2TK_API bool
d2tk_state_is_scroll_up(d2tk_state_t state)
{
	return (state & D2TK_STATE_SCROLL_UP);
}

D2TK_API bool
d2tk_state_is_scroll_down(d2tk_state_t state)
{
	return (state & D2TK_STATE_SCROLL_DOWN);
}

D2TK_API bool
d2tk_state_is_motion(d2tk_state_t state)
{
	return (state & D2TK_STATE_MOTION);
}

D2TK_API bool
d2tk_state_is_scroll_left(d2tk_state_t state)
{
	return (state & D2TK_STATE_SCROLL_LEFT);
}

D2TK_API bool
d2tk_state_is_scroll_right(d2tk_state_t state)
{
	return (state & D2TK_STATE_SCROLL_RIGHT);
}

D2TK_API bool
d2tk_state_is_active(d2tk_state_t state)
{
	return (state & D2TK_STATE_ACTIVE);
}

D2TK_API bool
d2tk_state_is_hot(d2tk_state_t state)
{
	return (state & D2TK_STATE_HOT);
}

D2TK_API bool
d2tk_state_is_focused(d2tk_state_t state)
{
	return (state & D2TK_STATE_FOCUS);
}

D2TK_API bool
d2tk_state_is_focus_in(d2tk_state_t state)
{
	return (state & D2TK_STATE_FOCUS_IN);
}

D2TK_API bool
d2tk_state_is_focus_out(d2tk_state_t state)
{
	return (state & D2TK_STATE_FOCUS_OUT);
}

D2TK_API bool
d2tk_state_is_changed(d2tk_state_t state)
{
	return (state & D2TK_STATE_CHANGED);
}

D2TK_API bool
d2tk_state_is_enter(d2tk_state_t state)
{
	return (state & D2TK_STATE_ENTER);
}

D2TK_API bool
d2tk_state_is_over(d2tk_state_t state)
{
	return (state & D2TK_STATE_OVER);
}

D2TK_API bool
d2tk_state_is_close(d2tk_state_t state)
{
	return (state & D2TK_STATE_CLOSE);
}

D2TK_API bool
d2tk_state_is_bell(d2tk_state_t state)
{
	return (state & D2TK_STATE_BELL);
}

D2TK_API bool
d2tk_base_is_hit(d2tk_base_t *base, const d2tk_rect_t *rect)
{
	if(  (base->mouse.x < rect->x)
		|| (base->mouse.y < rect->y)
		|| (base->mouse.x >= rect->x + rect->w)
		|| (base->mouse.y >= rect->y + rect->h) )
	{
		return false;
	}

	return true;
}

static inline bool
_d2tk_base_set_focus(d2tk_base_t *base, d2tk_id_t id)
{
	_d2tk_flip_set(&base->focusitem, id);

	return true;
}

static inline void
_d2tk_base_change_focus(d2tk_base_t *base)
{
	// copy edit.text_in to edit.text_out
	strncpy(base->edit.text_out, base->edit.text_in, sizeof(base->edit.text_out));
}

void
_d2tk_base_clear_chars(d2tk_base_t *base)
{
	base->keys.nchars = 0;
}

D2TK_API void
d2tk_base_append_utf8(d2tk_base_t *base, utf8_int32_t utf8)
{
	if(base->keys.nchars < sizeof(base->keys.nchars))
	{
		base->keys.chars[base->keys.nchars++] = utf8;
	}
}

D2TK_API void
d2tk_base_get_utf8(d2tk_base_t *base, ssize_t *len, const utf8_int32_t **utf8)
{
	if(len)
	{
		*len = base->keys.nchars;
	}

	if(utf8)
	{
		*utf8 = base->keys.chars;
	}

	_d2tk_base_clear_chars(base);
}

d2tk_state_t
_d2tk_base_is_active_hot_vertical_scroll(d2tk_base_t *base)
{
	d2tk_state_t state = D2TK_STATE_NONE;

	// test for vertical scrolling
	if(base->scroll.dy != 0.f)
	{
		if(base->scroll.dy > 0.f)
		{
			state |= D2TK_STATE_SCROLL_UP;
		}
		else
		{
			state |= D2TK_STATE_SCROLL_DOWN;
		}

		base->scroll.ody = base->scroll.dy;
		base->scroll.dy = 0; // eat scrolling
	}

	return state;
}

d2tk_state_t
_d2tk_base_is_active_hot_horizontal_scroll(d2tk_base_t *base)
{
	d2tk_state_t state = D2TK_STATE_NONE;

	// test for horizontal scrolling
	if(base->scroll.dx != 0.f)
	{
		if(base->scroll.dx > 0.f)
		{
			state |= D2TK_STATE_SCROLL_RIGHT;
		}
		else
		{
			state |= D2TK_STATE_SCROLL_LEFT;
		}

		base->scroll.odx = base->scroll.dx;
		base->scroll.dx = 0; // eat scrolling
	}

	return state;
}

D2TK_API d2tk_state_t
d2tk_base_is_active_hot(d2tk_base_t *base, d2tk_id_t id,
	const d2tk_rect_t *rect, d2tk_flag_t flags)
{
	d2tk_state_t state = D2TK_STATE_NONE;
	bool is_active = _d2tk_flip_equal_cur(&base->activeitem, id);
	bool is_hot = false;
	bool is_over = false;
	bool curfocus = _d2tk_flip_equal_cur(&base->focusitem, id);
	bool newfocus = curfocus;
	const bool lastfocus = _d2tk_flip_equal_old(&base->focusitem, id);

	// test for mouse up
	if(  is_active
		&& !d2tk_base_get_butmask(base, D2TK_BUTMASK_LEFT, false) )
	{
		_d2tk_flip_clear(&base->activeitem);
		is_active = false;
		state |= D2TK_STATE_UP;
	}

	// handle forward focus
	if(curfocus)
	{
		if(d2tk_base_get_modmask(base, D2TK_MODMASK_CTRL, false))
		{
			if(d2tk_base_get_keymask(base, D2TK_KEYMASK_RIGHT, false))
			{
				newfocus = false; // do NOT change curfocus
				base->focused = false; // clear focused flag
			}
		}
		else
		{
			if(d2tk_base_get_keymask(base, D2TK_KEYMASK_LEFT, false))
			{
				state |= D2TK_STATE_SCROLL_LEFT;
				base->scroll.odx = -1;
			}

			if(d2tk_base_get_keymask(base, D2TK_KEYMASK_RIGHT, false))
			{
				state |= D2TK_STATE_SCROLL_RIGHT;
				base->scroll.odx = 1;
			}

			if(d2tk_base_get_keymask(base, D2TK_KEYMASK_UP, false))
			{
				state |= D2TK_STATE_SCROLL_UP;
				base->scroll.ody = 1;
			}

			if(d2tk_base_get_keymask(base, D2TK_KEYMASK_DOWN, false))
			{
				state |= D2TK_STATE_SCROLL_DOWN;
				base->scroll.ody = -1;
			}
		}

		if(d2tk_base_get_keymask_up(base, D2TK_KEYMASK_ENTER))
		{
			is_active = false;
		}
		else if(d2tk_base_get_keymask_down(base, D2TK_KEYMASK_ENTER))
		{
			is_active = true;
			state |= D2TK_STATE_ENTER;
		}
		else if(d2tk_base_get_keymask(base, D2TK_KEYMASK_ENTER, false))
		{
			is_active = true;
		}
	}
	else if(!base->focused)
	{
		curfocus = _d2tk_base_set_focus(base, id);
		newfocus = curfocus;
		base->focused = true; // set focused flag
	}

	// test for mouse over
	if(d2tk_base_is_hit(base, rect))
	{
		// test for mouse down
		if(  _d2tk_flip_invalid(&base->activeitem)
			&& d2tk_base_get_butmask(base, D2TK_BUTMASK_LEFT, false) )
		{
			_d2tk_flip_set(&base->activeitem, id);
			is_active = true;
			curfocus = _d2tk_base_set_focus(base, id);
			newfocus = curfocus;
			state |= D2TK_STATE_DOWN;
		}

		if(d2tk_base_get_butmask(base, D2TK_BUTMASK_LEFT, false)  && !is_active)
		{
			// another widget is active with mouse down, so don't be hot
		}
		else
		{
			_d2tk_flip_set(&base->hotitem, id);
			is_hot = true;
		}

		is_over = true;

		// test whether to handle scrolling
		if(flags & D2TK_FLAG_SCROLL_Y)
		{
			state |= _d2tk_base_is_active_hot_vertical_scroll(base);
		}

		if(flags & D2TK_FLAG_SCROLL_X)
		{
			state |= _d2tk_base_is_active_hot_horizontal_scroll(base);
		}
	}

	if(is_active)
	{
		if( (base->mouse.dx != 0) || (base->mouse.dy != 0) )
		{
			state |= D2TK_STATE_MOTION;
		}

		state |= D2TK_STATE_ACTIVE;
	}

	if(is_hot)
	{
		state |= D2TK_STATE_HOT;
	}

	if(is_over)
	{
		state |= D2TK_STATE_OVER;
	}

	if(newfocus)
	{
		state |= D2TK_STATE_FOCUS;
	}

	{
		if(lastfocus && !curfocus)
		{
			state |= D2TK_STATE_FOCUS_OUT;
			_d2tk_flip_clear_old(&base->focusitem); // clear previous focus
#ifdef D2TK_DEBUG
			fprintf(stderr, "\tfocus out 0x%016"PRIx64"\n", id);
#endif
		}
		else if(!lastfocus && curfocus)
		{
			if(_d2tk_flip_invalid_old(&base->focusitem) && base->not_first_time)
			{
				_d2tk_flip_set(&base->focusitem, _d2tk_flip_get_cur(&base->focusitem));
			}
			else
			{
				state |= D2TK_STATE_FOCUS_IN;
#ifdef D2TK_DEBUG
				fprintf(stderr, "\tfocus in 0x%016"PRIx64"\n", id);
#endif
				_d2tk_base_change_focus(base);
			}
		}
	}

	// handle backwards focus
	if(newfocus)
	{
		if(d2tk_base_get_modmask(base, D2TK_MODMASK_CTRL, false))
		{
			if(d2tk_base_get_keymask(base, D2TK_KEYMASK_LEFT, false))
			{
				_d2tk_base_set_focus(base, base->lastitem);
			}
		}
	}

	base->lastitem = id;

	base->not_first_time = true;

	return state;
}

#define nocol 0x0
#define light_grey 0x7f7f7fff
#define dark_grey 0x3f3f3fff
#define darker_grey 0x222222ff
#define black 0x000000ff
#define white 0xffffffff
#define light_orange 0xffcf00ff
#define dark_orange 0xcf9f00ff

#define FONT_SANS_BOLD "FiraSans:bold"

D2TK_API const d2tk_style_t *
d2tk_base_get_default_style()
{
	static const d2tk_style_t style = {
		.font_face                       = FONT_SANS_BOLD,
		.border_width                    = 1,
		.padding                         = 1,
		.rounding                        = 4,
		.bg_color                        = darker_grey,
		.fill_color = {
			[D2TK_TRIPLE_NONE]             = dark_grey,
			[D2TK_TRIPLE_HOT]              = light_grey,
			[D2TK_TRIPLE_ACTIVE]           = dark_orange,
			[D2TK_TRIPLE_ACTIVE_HOT]       = light_orange,
			[D2TK_TRIPLE_FOCUS]            = dark_grey,
			[D2TK_TRIPLE_HOT_FOCUS]        = light_grey,
			[D2TK_TRIPLE_ACTIVE_FOCUS]     = dark_orange,
			[D2TK_TRIPLE_ACTIVE_HOT_FOCUS] = light_orange,
		},
		.stroke_color = {
			[D2TK_TRIPLE_NONE]             = black,
			[D2TK_TRIPLE_HOT]              = black,
			[D2TK_TRIPLE_ACTIVE]           = black,
			[D2TK_TRIPLE_ACTIVE_HOT]       = black,
			[D2TK_TRIPLE_FOCUS]            = white,
			[D2TK_TRIPLE_HOT_FOCUS]        = white,
			[D2TK_TRIPLE_ACTIVE_FOCUS]     = white,
			[D2TK_TRIPLE_ACTIVE_HOT_FOCUS] = white,
		},
		.text_stroke_color = {
			[D2TK_TRIPLE_NONE]             = white,
			[D2TK_TRIPLE_HOT]              = light_orange,
			[D2TK_TRIPLE_ACTIVE]           = white,
			[D2TK_TRIPLE_ACTIVE_HOT]       = dark_grey,
			[D2TK_TRIPLE_FOCUS]            = white,
			[D2TK_TRIPLE_HOT_FOCUS]        = light_orange,
			[D2TK_TRIPLE_ACTIVE_FOCUS]     = white,
			[D2TK_TRIPLE_ACTIVE_HOT_FOCUS] = dark_grey
		},
		.text_fill_color = {
			[D2TK_TRIPLE_NONE]             = nocol,
			[D2TK_TRIPLE_HOT]              = nocol,
			[D2TK_TRIPLE_ACTIVE]           = nocol,
			[D2TK_TRIPLE_ACTIVE_HOT]       = nocol,
			[D2TK_TRIPLE_FOCUS]            = nocol,
			[D2TK_TRIPLE_HOT_FOCUS]        = nocol,
			[D2TK_TRIPLE_ACTIVE_FOCUS]     = nocol,
			[D2TK_TRIPLE_ACTIVE_HOT_FOCUS] = nocol
		}
	};

	return &style;
}

D2TK_API const d2tk_style_t *
d2tk_base_get_style(d2tk_base_t *base)
{
	return base->style ? base->style : d2tk_base_get_default_style();
}

D2TK_API void
d2tk_base_set_style(d2tk_base_t *base, const d2tk_style_t *style)
{
	base->style = style;
}

D2TK_API void
d2tk_base_set_default_style(d2tk_base_t *base)
{
	d2tk_base_set_style(base, NULL);
}

D2TK_API d2tk_base_t *
d2tk_base_new(const d2tk_core_driver_t *driver, void *data)
{
	d2tk_base_t *base = calloc(1, sizeof(d2tk_base_t));
	if(!base)
	{
		return NULL;
	}

	atomic_init(&base->again, false);

	base->core = d2tk_core_new(driver, data);

	return base;
}

D2TK_API void
d2tk_base_set_ttls(d2tk_base_t *base, uint32_t sprites, uint32_t memcaches)
{
	d2tk_core_set_ttls(base->core, sprites, memcaches);
}

D2TK_API void
d2tk_base_free(d2tk_base_t *base)
{
	for(unsigned i = 0; i < _D2TK_MAX_ATOM; i++)
	{
		d2tk_atom_t *atom = &base->atoms[i];

		atom->id = 0;
		atom->type = 0;
		if(atom->event)
		{
			atom->event(D2TK_ATOM_EVENT_DEINIT, atom->body);
			atom->event = NULL;
		}
		free(atom->body);
		atom->body = NULL;
	}

	d2tk_core_free(base->core);
	free(base);
}

D2TK_API int
d2tk_base_pre(d2tk_base_t *base, void *pctx)
{
	// reset hot item
	_d2tk_flip_clear(&base->hotitem);

	// calculate mouse motion
	base->mouse.dx = (int32_t)base->mouse.x - base->mouse.ox;
	base->mouse.dy = (int32_t)base->mouse.y - base->mouse.oy;

	// reset clear-focus flag
	base->clear_focus = false;

	const d2tk_style_t *style = d2tk_base_get_style(base);
	d2tk_core_set_bg_color(base->core, style->bg_color);

	return d2tk_core_pre(base->core, pctx);
}

D2TK_API void
d2tk_base_post(d2tk_base_t *base)
{
	// clear scroll
	base->scroll.dx = 0.f;
	base->scroll.dy = 0.f;

	// store old mouse position
	base->mouse.ox = base->mouse.x;
	base->mouse.oy = base->mouse.y;

	if(base->clear_focus)
	{
		_d2tk_flip_clear(&base->activeitem);

		base->focused = false;
	}

	base->mouse.mask_prev = base->mouse.mask;
	base->keys.mask_prev = base->keys.mask;

	_d2tk_base_clear_chars(base);

	d2tk_core_post(base->core);
}

D2TK_API void
d2tk_base_probe(d2tk_base_t *base)
{
	for(unsigned i = 0; i < _D2TK_MAX_ATOM; i++)
	{
		d2tk_atom_t *atom = &base->atoms[i];

		if(atom->id && atom->type && atom->event)
		{
			if(atom->event(D2TK_ATOM_EVENT_PROBE, atom->body))
			{
				d2tk_base_set_again(base);
				break;
			}
		}
	}
}

D2TK_API void
d2tk_base_clear_focus(d2tk_base_t *base)
{
	base->clear_focus = true;
}

D2TK_API bool
d2tk_base_set_again(d2tk_base_t *base)
{
	return atomic_exchange(&base->again, true);
}

D2TK_API bool
d2tk_base_get_again(d2tk_base_t *base)
{
	return atomic_exchange(&base->again, false);
}

D2TK_API void
d2tk_base_set_mouse_pos(d2tk_base_t *base, d2tk_coord_t x, d2tk_coord_t y)
{
	base->mouse.x = x;
	base->mouse.y = y;
}

D2TK_API void
d2tk_base_get_mouse_pos(d2tk_base_t *base, d2tk_coord_t *x, d2tk_coord_t *y)
{
	if(x)
	{
		*x = base->mouse.x;
	}

	if(y)
	{
		*y = base->mouse.y;
	}
}

D2TK_API void
d2tk_base_add_mouse_scroll(d2tk_base_t *base, int32_t dx, int32_t dy)
{
	base->scroll.dx += dx;
	base->scroll.dy += dy;
}

D2TK_API void
d2tk_base_get_mouse_scroll(d2tk_base_t *base, int32_t *dx, int32_t *dy,
	bool clear)
{
	if(dx)
	{
		*dx = base->scroll.dx;
	}

	if(dy)
	{
		*dy = base->scroll.dy;
	}

	if(clear)
	{
		base->scroll.dx = 0;
		base->scroll.dy = 0;
	}
}

D2TK_API bool
d2tk_base_set_butmask(d2tk_base_t *base, d2tk_butmask_t mask, bool down)
{
	const bool old_state = (base->mouse.mask & mask) == mask;

	if(down)
	{
		base->mouse.mask |= mask;
	}
	else
	{
		base->mouse.mask &= ~mask;
	}

	return old_state;
}

D2TK_API bool
d2tk_base_get_butmask(d2tk_base_t *base, d2tk_butmask_t mask, bool clear)
{
	const bool old_state = (base->mouse.mask & mask) == mask;

	if(clear)
	{
		base->mouse.mask &= ~mask;
	}

	return old_state;

}

D2TK_API bool
d2tk_base_get_butmask_prev(d2tk_base_t *base, d2tk_butmask_t mask)
{
	const bool old_state = (base->mouse.mask_prev & mask) == mask;

	return old_state;

}

D2TK_API bool
d2tk_base_get_butmask_down(d2tk_base_t *base, d2tk_butmask_t mask)
{
	return !d2tk_base_get_butmask_prev(base, mask)
		&& d2tk_base_get_butmask(base, mask, false);
}

D2TK_API bool
d2tk_base_get_butmask_up(d2tk_base_t *base, d2tk_butmask_t mask)
{
	return d2tk_base_get_butmask_prev(base, mask)
		&& !d2tk_base_get_butmask(base, mask, false);
}

D2TK_API bool
d2tk_base_set_modmask(d2tk_base_t *base, d2tk_modmask_t mask, bool down)
{
	const bool old_state = (base->keys.mod & mask) == mask;

	if(down)
	{
		base->keys.mod |= mask;
	}
	else
	{
		base->keys.mod &= ~mask;
	}

	return old_state;
}

D2TK_API bool
d2tk_base_get_modmask(d2tk_base_t *base, d2tk_modmask_t mask, bool clear)
{
	const bool old_state = (base->keys.mod & mask) == mask;

	if(clear)
	{
		base->keys.mod &= ~mask;
	}

	return old_state;

}

D2TK_API bool
d2tk_base_set_keymask(d2tk_base_t *base, d2tk_keymask_t mask, bool down)
{
	const bool old_state = (base->keys.mask & mask) == mask;

	if(down)
	{
		base->keys.mask |= mask;
	}
	else
	{
		base->keys.mask &= ~mask;
	}

	return old_state;
}

D2TK_API bool
d2tk_base_get_keymask(d2tk_base_t *base, d2tk_keymask_t mask, bool clear)
{
	const bool old_state = (base->keys.mask & mask) == mask;

	if(clear)
	{
		base->keys.mask &= ~mask;
	}

	return old_state;

}

D2TK_API bool
d2tk_base_get_keymask_prev(d2tk_base_t *base, d2tk_keymask_t mask)
{
	const bool old_state = (base->keys.mask_prev & mask) == mask;

	return old_state;
}

D2TK_API bool
d2tk_base_get_keymask_down(d2tk_base_t *base, d2tk_keymask_t mask)
{
	return !d2tk_base_get_keymask_prev(base, mask)
		&& d2tk_base_get_keymask(base, mask, false);
}

D2TK_API bool
d2tk_base_get_keymask_up(d2tk_base_t *base, d2tk_keymask_t mask)
{
	return d2tk_base_get_keymask_prev(base, mask)
		&& !d2tk_base_get_keymask(base, mask, false);
}

D2TK_API void
d2tk_base_set_dimensions(d2tk_base_t *base, d2tk_coord_t w, d2tk_coord_t h)
{
	d2tk_core_set_dimensions(base->core, w, h);
}

D2TK_API void
d2tk_base_get_dimensions(d2tk_base_t *base, d2tk_coord_t *w, d2tk_coord_t *h)
{
	d2tk_core_get_dimensions(base->core, w, h);
}

D2TK_API void
d2tk_base_set_full_refresh(d2tk_base_t *base)
{
	d2tk_core_set_full_refresh(base->core);
}
