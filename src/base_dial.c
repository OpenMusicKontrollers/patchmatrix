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

#include <inttypes.h>
#include <math.h>
#include <stdio.h>

#include "base_internal.h"

D2TK_API d2tk_state_t
d2tk_base_dial_bool(d2tk_base_t *base, d2tk_id_t id, const d2tk_rect_t *rect,
	bool *value)
{
	const bool oldvalue = *value;

	d2tk_state_t state = d2tk_base_is_active_hot(base, id, rect,
		D2TK_FLAG_SCROLL);

	if(d2tk_state_is_down(state) || d2tk_state_is_enter(state))
	{
		*value = !*value;
	}
	else if(d2tk_state_is_scroll_up(state))
	{
		*value = true;
	}
	else if(d2tk_state_is_scroll_down(state))
	{
		*value = false;
	}

	if(oldvalue != *value)
	{
		state |= D2TK_STATE_CHANGED;
	}

	const d2tk_style_t *style = d2tk_base_get_style(base);
	d2tk_core_t *core = base->core;

	const d2tk_hash_dict_t dict [] = {
		{ &state, sizeof(d2tk_state_t) },
		{ rect, sizeof(d2tk_rect_t) },
		{ style, sizeof(d2tk_style_t) },
		{ value, sizeof(bool) },
		{ NULL, 0 }
	};
	const uint64_t hash = d2tk_hash_dict(dict);

	D2TK_CORE_WIDGET(core, hash, widget)
	{
		d2tk_triple_t triple = D2TK_TRIPLE_NONE;

		if(*value)
		{
			triple |= D2TK_TRIPLE_ACTIVE;
		}

		if(d2tk_state_is_hot(state))
		{
			triple |= D2TK_TRIPLE_HOT;
		}

		if(d2tk_state_is_focused(state))
		{
			triple |= D2TK_TRIPLE_FOCUS;
		}

		const size_t ref = d2tk_core_bbox_push(core, true, rect);

		d2tk_rect_t bnd;
		d2tk_rect_shrink(&bnd, rect, style->padding);

		const d2tk_coord_t d = bnd.h < bnd.w ? bnd.h : bnd.w;
		const d2tk_coord_t r1 = d / 2;
		const d2tk_coord_t r0 = d / 3;
		bnd.x += bnd.w / 2;
		bnd.y += bnd.h / 2;

		d2tk_core_begin_path(core);
		d2tk_core_arc(core, bnd.x, bnd.y, r1, 0, 360, true);
		d2tk_core_color(core, style->fill_color[D2TK_TRIPLE_NONE]);
		d2tk_core_stroke_width(core, 0);
		d2tk_core_fill(core);

		d2tk_core_begin_path(core);
		d2tk_core_arc(core, bnd.x, bnd.y, r1, 0, 360, true);
		d2tk_core_color(core, style->stroke_color[triple]);
		d2tk_core_stroke_width(core, style->border_width);
		d2tk_core_stroke(core);

		d2tk_core_begin_path(core);
		d2tk_core_arc(core, bnd.x, bnd.y, r0, 0, 360, true);
		d2tk_core_color(core, style->fill_color[triple]);
		d2tk_core_stroke_width(core, 0);
		d2tk_core_fill(core);

		d2tk_core_begin_path(core);
		d2tk_core_arc(core, bnd.x, bnd.y, r0, 0, 360, true);
		d2tk_core_color(core, style->stroke_color[D2TK_TRIPLE_NONE]);
		d2tk_core_stroke_width(core, style->border_width);
		d2tk_core_stroke(core);

		d2tk_core_bbox_pop(core, ref);
	}

	return state;
}

static inline void
_d2tk_base_draw_dial(d2tk_core_t *core, const d2tk_rect_t *rect,
	d2tk_state_t state, float rel, const d2tk_style_t *style)
{
	const d2tk_hash_dict_t dict [] = {
		{ &state, sizeof(d2tk_state_t) },
		{ rect, sizeof(d2tk_rect_t) },
		{ style, sizeof(d2tk_style_t) },
		{ &rel, sizeof(float) },
		{ NULL, 0 }
	};
	const uint64_t hash = d2tk_hash_dict(dict);

	D2TK_CORE_WIDGET(core, hash, widget)
	{
		d2tk_triple_t triple = D2TK_TRIPLE_NONE;
		d2tk_triple_t triple_active = D2TK_TRIPLE_ACTIVE;
		d2tk_triple_t triple_inactive = D2TK_TRIPLE_NONE;

		if(d2tk_state_is_active(state))
		{
			triple |= D2TK_TRIPLE_ACTIVE;
		}

		if(d2tk_state_is_hot(state))
		{
			triple |= D2TK_TRIPLE_HOT;
			triple_active |= D2TK_TRIPLE_HOT;
			triple_inactive |= D2TK_TRIPLE_HOT;
		}

		if(d2tk_state_is_focused(state))
		{
			triple |= D2TK_TRIPLE_FOCUS;
			triple_active |= D2TK_TRIPLE_FOCUS;
			triple_inactive |= D2TK_TRIPLE_FOCUS;
		}

		const size_t ref = d2tk_core_bbox_push(core, true, rect);

		d2tk_rect_t bnd;
		d2tk_rect_shrink(&bnd, rect, style->padding);

		const d2tk_coord_t d = bnd.h < bnd.w ? bnd.h : bnd.w;
		const d2tk_coord_t r1 = d / 2;
		const d2tk_coord_t r0 = d / 4;
		bnd.x += bnd.w / 2;
		bnd.y += bnd.h / 2;

		static const d2tk_coord_t a = 90 + 22; //FIXME
		static const d2tk_coord_t c = 90 - 22; //FIXME
		const d2tk_coord_t b = a + (360 - 44)*rel; //FIXME

		d2tk_core_begin_path(core);
		d2tk_core_arc(core, bnd.x, bnd.y, (r1 + r0)/2, a, c, true);
		d2tk_core_color(core, style->fill_color[triple_inactive]);
		d2tk_core_stroke_width(core, r1 - r0);
		d2tk_core_stroke(core);

		if(rel > 0.f)
		{
			d2tk_core_begin_path(core);
			d2tk_core_arc(core, bnd.x, bnd.y, (r1 + r0)/2, a, b, true);
			d2tk_core_color(core, style->fill_color[triple_active]);
			d2tk_core_stroke_width(core, (r1 - r0) * 3/4);
			d2tk_core_stroke(core);
		}

		const float phi = (b + 90) / 180.f * M_PI;
		const d2tk_coord_t rx1 = bnd.x + r0 * sinf(phi);
		const d2tk_coord_t ry1 = bnd.y - r0 * cosf(phi);

		d2tk_core_begin_path(core);
		d2tk_core_move_to(core, bnd.x, bnd.y);
		d2tk_core_line_to(core, rx1, ry1);
		d2tk_core_close_path(core);
		d2tk_core_color(core, style->fill_color[triple_active]);
		d2tk_core_stroke_width(core, style->border_width);
		d2tk_core_stroke(core);

		d2tk_core_begin_path(core);
		d2tk_core_arc(core, bnd.x, bnd.y, r1, a, c, true);
		d2tk_core_arc(core, bnd.x, bnd.y, r0, c, a, false);
		d2tk_core_close_path(core);
		d2tk_core_color(core, style->stroke_color[triple]);
		d2tk_core_stroke_width(core, style->border_width);
		d2tk_core_stroke(core);

		d2tk_core_bbox_pop(core, ref);
	}
}

D2TK_API d2tk_state_t
d2tk_base_dial_int32(d2tk_base_t *base, d2tk_id_t id, const d2tk_rect_t *rect,
	int32_t min, int32_t *value, int32_t max)
{
	d2tk_state_t state = d2tk_base_is_active_hot(base, id, rect,
		D2TK_FLAG_SCROLL);

	const int32_t oldvalue = *value;

	if(d2tk_state_is_scroll_up(state))
	{
		*value += base->scroll.ody;
		d2tk_clip_int32(min, value, max);
	}
	else if(d2tk_state_is_scroll_down(state))
	{
		*value += base->scroll.ody;
		d2tk_clip_int32(min, value, max);
	}
	else if(d2tk_state_is_motion(state))
	{
		const int32_t adx = abs(base->mouse.dx);
		const int32_t ady = abs(base->mouse.dy);
		const int32_t adz = adx > ady ? base->mouse.dx : -base->mouse.dy;

		*value += adz;
		d2tk_clip_int32(min, value, max);
	}

	if(oldvalue != *value)
	{
		state |= D2TK_STATE_CHANGED;
	}

	float rel = (float)(*value - min) / (max - min);
	d2tk_clip_float(0.f, &rel, 1.f);

	d2tk_core_t *core = base->core;
	_d2tk_base_draw_dial(core, rect, state, rel, d2tk_base_get_style(base));

	return state;
}

D2TK_API d2tk_state_t
d2tk_base_prop_int32(d2tk_base_t *base, d2tk_id_t id, const d2tk_rect_t *rect,
	int32_t min, int32_t *value, int32_t max)
{
	const d2tk_coord_t dw = rect->w / 3;

	const d2tk_rect_t left = {
		.x = rect->x,
		.y = rect->y,
		.w = dw,
		.h = rect->h
	};
	const d2tk_rect_t right = {
		.x = rect->x + dw,
		.y = rect->y,
		.w = rect->w - dw,
		.h = rect->h
	};

	const d2tk_id_t id_left = (1 << 24) | id;
	const d2tk_id_t id_right = (2 << 24) | id;

	const d2tk_state_t state_dial = d2tk_base_dial_int32(base, id_left, &left,
		min, value, max);

	char text [32];
	snprintf(text, sizeof(text), "%+"PRIi32, *value);

	const d2tk_state_t state_field = d2tk_base_text_field(base, id_right, &right,
		sizeof(text), text, D2TK_ALIGN_RIGHT | D2TK_ALIGN_MIDDLE, "1234567890+-");

	if(d2tk_state_is_changed(state_field))
	{
		d2tk_base_set_again(base);
	}

	const d2tk_state_t state = state_dial | state_field;

	if(d2tk_state_is_focus_out(state))
	{
		int32_t val;
		if(sscanf(text, "%"SCNi32, &val) == 1)
		{
			*value = val;
			d2tk_clip_int32(min, value, max);
		}
	}

	return state;
}

D2TK_API d2tk_state_t
d2tk_base_dial_int64(d2tk_base_t *base, d2tk_id_t id, const d2tk_rect_t *rect,
	int64_t min, int64_t *value, int64_t max)
{
	d2tk_state_t state = d2tk_base_is_active_hot(base, id, rect,
		D2TK_FLAG_SCROLL);

	const int64_t oldvalue = *value;

	if(d2tk_state_is_scroll_up(state))
	{
		*value += base->scroll.ody;
		d2tk_clip_int64(min, value, max);
	}
	else if(d2tk_state_is_scroll_down(state))
	{
		*value += base->scroll.ody;
		d2tk_clip_int64(min, value, max);
	}
	else if(d2tk_state_is_motion(state))
	{
		const int64_t adx = abs(base->mouse.dx);
		const int64_t ady = abs(base->mouse.dy);
		const int64_t adz = adx > ady ? base->mouse.dx : -base->mouse.dy;

		*value += adz;
		d2tk_clip_int64(min, value, max);
	}

	if(oldvalue != *value)
	{
		state |= D2TK_STATE_CHANGED;
	}

	float rel = (float)(*value - min) / (max - min);
	d2tk_clip_float(0.f, &rel, 1.f);

	d2tk_core_t *core = base->core;
	_d2tk_base_draw_dial(core, rect, state, rel, d2tk_base_get_style(base));

	return state;
}

D2TK_API d2tk_state_t
d2tk_base_dial_float(d2tk_base_t *base, d2tk_id_t id, const d2tk_rect_t *rect,
	float min, float *value, float max)
{
	d2tk_state_t state = d2tk_base_is_active_hot(base, id, rect,
		D2TK_FLAG_SCROLL);

	const float oldvalue = *value;

	if(d2tk_state_is_scroll_up(state))
	{
		const float dv = (max - min);
		const float mul = d2tk_base_get_mod(base) ? 0.01f : 0.1f;
		*value += dv * mul * base->scroll.ody;
		d2tk_clip_float(min, value, max);
	}
	else if(d2tk_state_is_scroll_down(state))
	{
		const float dv = (max - min);
		const float mul = d2tk_base_get_mod(base) ? 0.01f : 0.1f;
		*value += dv * mul * base->scroll.ody;
		d2tk_clip_float(min, value, max);
	}
	else if(d2tk_state_is_motion(state))
	{
		const float adx = abs(base->mouse.dx);
		const float ady = abs(base->mouse.dy);
		const float adz = adx > ady ? base->mouse.dx : -base->mouse.dy;

		const float dv = (max - min);
		const float mul = d2tk_base_get_mod(base) ? 0.001f : 0.01f;
		*value += dv * adz * mul;
		d2tk_clip_float(min, value, max);
	}

	if(oldvalue != *value)
	{
		state |= D2TK_STATE_CHANGED;
	}

	float rel = (*value - min) / (max - min);
	d2tk_clip_float(0.f, &rel, 1.f);

	d2tk_core_t *core = base->core;
	_d2tk_base_draw_dial(core, rect, state, rel, d2tk_base_get_style(base));

	return state;
}

D2TK_API d2tk_state_t
d2tk_base_prop_float(d2tk_base_t *base, d2tk_id_t id, const d2tk_rect_t *rect,
	float min, float *value, float max)
{
	const d2tk_coord_t dw = rect->w / 3;

	const d2tk_rect_t left = {
		.x = rect->x,
		.y = rect->y,
		.w = dw,
		.h = rect->h
	};
	const d2tk_rect_t right = {
		.x = rect->x + dw,
		.y = rect->y,
		.w = rect->w - dw,
		.h = rect->h
	};

	const d2tk_id_t id_left = (1 << 24) | id;
	const d2tk_id_t id_right = (2 << 24) | id;

	const d2tk_state_t state_dial = d2tk_base_dial_float(base, id_left, &left,
		min, value, max);

	char text [32];
	snprintf(text, sizeof(text), "%+.4f", *value);

	const d2tk_state_t state_field = d2tk_base_text_field(base, id_right, &right,
		sizeof(text), text, D2TK_ALIGN_RIGHT | D2TK_ALIGN_MIDDLE, "1234567890.+-");

	if(d2tk_state_is_changed(state_field))
	{
		d2tk_base_set_again(base);
	}

	const d2tk_state_t state = state_dial | state_field;

	if(d2tk_state_is_focus_out(state))
	{
		float val;
		if(sscanf(text, "%f", &val) == 1)
		{
			*value = val;
			d2tk_clip_float(min, value, max);
		}
	}

	return state;
}

D2TK_API d2tk_state_t
d2tk_base_dial_double(d2tk_base_t *base, d2tk_id_t id, const d2tk_rect_t *rect,
	double min, double *value, double max)
{
	d2tk_state_t state = d2tk_base_is_active_hot(base, id, rect,
		D2TK_FLAG_SCROLL);

	const double oldvalue = *value;

	if(d2tk_state_is_scroll_up(state))
	{
		const double dv = (max - min);
		const double mul = d2tk_base_get_mod(base) ? 0.01 : 0.1;
		*value += dv * mul * base->scroll.ody;
		d2tk_clip_double(min, value, max);
	}
	else if(d2tk_state_is_scroll_down(state))
	{
		const double dv = (max - min);
		const double mul = d2tk_base_get_mod(base) ? 0.01 : 0.1;
		*value += dv * mul * base->scroll.ody;
		d2tk_clip_double(min, value, max);
	}
	else if(d2tk_state_is_motion(state))
	{
		const double adx = abs(base->mouse.dx);
		const double ady = abs(base->mouse.dy);
		const double adz = adx > ady ? base->mouse.dx : -base->mouse.dy;

		const double dv = (max - min);
		const double mul = d2tk_base_get_mod(base) ? 0.001 : 0.01;
		*value += dv * adz * mul;
		d2tk_clip_double(min, value, max);
	}

	if(oldvalue != *value)
	{
		state |= D2TK_STATE_CHANGED;
	}

	float rel = (*value - min) / (max - min);
	d2tk_clip_float(0.f, &rel, 1.f);

	d2tk_core_t *core = base->core;
	_d2tk_base_draw_dial(core, rect, state, rel, d2tk_base_get_style(base));

	return state;
}
