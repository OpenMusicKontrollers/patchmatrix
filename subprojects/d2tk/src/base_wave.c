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
_d2tk_base_draw_wave(d2tk_core_t *core, const d2tk_rect_t *rect,
	d2tk_state_t state, const d2tk_style_t *style, float min,
	const float *value, int32_t nelem, float max)
{
	const d2tk_hash_dict_t dict [] = {
		{ rect, sizeof(d2tk_rect_t) },
		{ &state , sizeof(d2tk_state_t) },
		{ style, sizeof(d2tk_style_t) },
		{ &min, sizeof(float) },
		{ value, sizeof(float)*nelem },
		{ &max, sizeof(float) },
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
		const float range_1 = 1.f / (max - min);

		d2tk_rect_t bnd_outer;
		d2tk_rect_t bnd_inner;
		d2tk_rect_shrink(&bnd_outer, rect, style->padding);
		d2tk_rect_shrink(&bnd_inner, &bnd_outer, 2*style->padding);

		d2tk_core_begin_path(core);
		d2tk_core_rect(core, &bnd_inner);
		d2tk_core_color(core, style->fill_color[triple_inactive]);
		d2tk_core_fill(core);

		d2tk_core_color(core, style->fill_color[triple_active]);

		if(nelem > bnd_inner.w)
		{
			for(int32_t i = 0; i < bnd_inner.w; i++)
			{
				const int32_t off = i * (nelem - 1) / (bnd_inner.w - 1);
				if(value[off] == HUGE_VAL)
				{
					continue;
				}
				const float rel = 1.f - (value[off] - min)*range_1;
				const d2tk_rect_t point = {
					.x = bnd_inner.x + i,
					.y = bnd_inner.y + rel*bnd_inner.h,
					.w = style->border_width,
					.h = style->border_width
				};

				d2tk_core_begin_path(core);
				d2tk_core_rect(core, &point);
				d2tk_core_fill(core);
			}
		}
		else
		{
			for(int32_t off = 0; off < nelem; off++)
			{
				const int32_t i = off * (bnd_inner.w - 1) / (nelem - 1);
				if(value[off] == HUGE_VAL)
				{
					continue;
				}
				const float rel = 1.f - (value[off] - min) * range_1;
				const d2tk_rect_t point = {
					.x = bnd_inner.x + i,
					.y = bnd_inner.y + rel*bnd_inner.h,
					.w = style->border_width,
					.h = style->border_width
				};

				d2tk_core_begin_path(core);
				d2tk_core_rect(core, &point);
				d2tk_core_fill(core);
			}
		}

		d2tk_core_begin_path(core);
		d2tk_core_rect(core, &bnd_outer);
		d2tk_core_color(core, style->stroke_color[triple]);
		d2tk_core_stroke_width(core, style->border_width);
		d2tk_core_stroke(core);

		d2tk_core_bbox_pop(core, ref);
	}
}

D2TK_API d2tk_state_t
d2tk_base_wave_float(d2tk_base_t *base, d2tk_id_t id, const d2tk_rect_t *rect,
	float min, const float *value, int32_t nelem, float max)
{
	d2tk_state_t state = d2tk_base_is_active_hot(base, id, rect,
		D2TK_FLAG_SCROLL);

	_d2tk_base_draw_wave(base->core, rect, state, d2tk_base_get_style(base),
		min, value, nelem, max);

	return state;
}
