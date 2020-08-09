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

static inline void
_d2tk_base_draw_bar(d2tk_core_t *core, const d2tk_rect_t *rect,
	d2tk_state_t state, const d2tk_style_t *style, float v, float z)
{
	const d2tk_hash_dict_t dict [] = {
		{ rect, sizeof(d2tk_rect_t) },
		{ &state , sizeof(d2tk_state_t) },
		{ style, sizeof(d2tk_style_t) },
		{ &v, sizeof(float) },
		{ &z, sizeof(float) },
		{ NULL, 0 }
	};
	const uint64_t hash = d2tk_hash_dict(dict);

	D2TK_CORE_WIDGET(core, hash, widget)
	{
		d2tk_rect_t bnd_outer;
		d2tk_rect_t bnd_inner_inactive;
		d2tk_rect_t bnd_inner_active;
		d2tk_rect_shrink(&bnd_outer, rect, style->padding);
		d2tk_rect_shrink(&bnd_inner_inactive, &bnd_outer, 2*style->padding);
		d2tk_rect_shrink(&bnd_inner_active, &bnd_outer, 2*style->padding);

		if(v < z) // in the negative
		{
			v = z - v;
			const d2tk_coord_t wv = v != 0.f ? bnd_inner_active.w * v : 1;
			const d2tk_coord_t wz = bnd_inner_active.w * z;

			bnd_inner_active.x += wz - wv;
			bnd_inner_active.w = wv;
		}
		else // in the positive
		{
			v = v - z;
			const d2tk_coord_t wv = v != 0.f ? bnd_inner_active.w * v : 1;
			const d2tk_coord_t wz = bnd_inner_active.w * z;

			bnd_inner_active.x += wz;
			bnd_inner_active.w = wv;
		}

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

		{
			const size_t ref = d2tk_core_bbox_push(core, true, rect);

			d2tk_core_begin_path(core);
			d2tk_core_rect(core, &bnd_inner_inactive);
			d2tk_core_color(core, style->fill_color[triple_inactive]);
			d2tk_core_fill(core);

			d2tk_core_begin_path(core);
			d2tk_core_rect(core, &bnd_inner_active);
			d2tk_core_color(core, style->fill_color[triple_active]);
			d2tk_core_fill(core);

			d2tk_core_begin_path(core);
			d2tk_core_rect(core, &bnd_outer);
			d2tk_core_color(core, style->stroke_color[triple]);
			d2tk_core_stroke_width(core, style->border_width);
			d2tk_core_stroke(core);

			d2tk_core_bbox_pop(core, ref);
		}
	}
}

D2TK_API d2tk_state_t
d2tk_base_bar_int32(d2tk_base_t *base, d2tk_id_t id, const d2tk_rect_t *rect,
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

	const float range_1 = 1.f / (max - min);
	float v = (*value - min) * range_1;
	float z = (   0.f - min) * range_1;

	d2tk_clip_float(0.f, &v, 1.f);
	d2tk_clip_float(0.f, &z, 1.f);

	_d2tk_base_draw_bar(base->core, rect, state, d2tk_base_get_style(base), v, z);

	return state;
}

D2TK_API d2tk_state_t
d2tk_base_bar_float(d2tk_base_t *base, d2tk_id_t id, const d2tk_rect_t *rect,
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

	const float range_1 = 1.f / (max - min);
	float v = (*value - min) * range_1;
	float z = (   0.f - min) * range_1;

	d2tk_clip_float(0.f, &v, 1.f);
	d2tk_clip_float(0.f, &z, 1.f);

	_d2tk_base_draw_bar(base->core, rect, state, d2tk_base_get_style(base), v, z);

	return state;
}
