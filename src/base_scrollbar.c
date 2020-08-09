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

#include "base_internal.h"

typedef struct _d2tk_atom_body_scroll_t d2tk_atom_body_scroll_t;

struct _d2tk_atom_body_scroll_t {
	float offset [2];
};

struct _d2tk_scrollbar_t {
	d2tk_id_t id;
	d2tk_flag_t flags;
	int32_t max [2];
	int32_t num [2];
	d2tk_atom_body_scroll_t *atom_body;
	const d2tk_rect_t *rect;
	d2tk_rect_t sub;
};

const size_t d2tk_atom_body_scroll_sz = sizeof(d2tk_atom_body_scroll_t);
const size_t d2tk_scrollbar_sz = sizeof(d2tk_scrollbar_t);

D2TK_API d2tk_scrollbar_t *
d2tk_scrollbar_begin(d2tk_base_t *base, const d2tk_rect_t *rect, d2tk_id_t id,
	d2tk_flag_t flags, const uint32_t max [2], const uint32_t num [2],
	d2tk_scrollbar_t *scrollbar)
{
	scrollbar->id = id;
	scrollbar->flags = flags;
	scrollbar->max[0] = max[0];
	scrollbar->max[1] = max[1];
	scrollbar->num[0] = num[0];
	scrollbar->num[1] = num[1];
	scrollbar->rect = rect;
	scrollbar->sub = *rect;
	scrollbar->atom_body = _d2tk_base_get_atom(base, id, D2TK_ATOM_SCROLL, NULL);

	const d2tk_coord_t s = 10; //FIXME

	if(flags & D2TK_FLAG_SCROLL_X)
	{
		scrollbar->sub.h -= s;
	}

	if(flags & D2TK_FLAG_SCROLL_Y)
	{
		scrollbar->sub.w -= s;
	}

	return scrollbar;
}

D2TK_API bool
d2tk_scrollbar_not_end(d2tk_scrollbar_t *scrollbar)
{
	return scrollbar ? true : false;
}

static void
_d2tk_draw_scrollbar(d2tk_core_t *core, d2tk_state_t hstate, d2tk_state_t vstate,
	const d2tk_rect_t *hbar, const d2tk_rect_t *vbar, const d2tk_style_t *style,
	d2tk_flag_t flags)
{
	const d2tk_hash_dict_t dict [] = {
		{ &hstate, sizeof(d2tk_state_t) },
		{ &vstate, sizeof(d2tk_state_t) },
		{ hbar, sizeof(d2tk_rect_t) },
		{ vbar, sizeof(d2tk_rect_t) },
		{ style, sizeof(d2tk_style_t) },
		{ &flags, sizeof(d2tk_flag_t) },
		{ NULL, 0 }
	};
	const uint64_t hash = d2tk_hash_dict(dict);

	D2TK_CORE_WIDGET(core, hash, widget)
	{
		if(flags & D2TK_FLAG_SCROLL_X)
		{
			d2tk_triple_t triple = D2TK_TRIPLE_NONE;

			if(d2tk_state_is_active(hstate))
			{
				triple |= D2TK_TRIPLE_ACTIVE;
			}

			if(d2tk_state_is_hot(hstate))
			{
				triple |= D2TK_TRIPLE_HOT;
			}

			if(d2tk_state_is_focused(hstate))
			{
				triple |= D2TK_TRIPLE_FOCUS;
			}

			const size_t ref = d2tk_core_bbox_push(core, true, hbar);

			d2tk_core_begin_path(core);
			d2tk_core_rounded_rect(core, hbar, style->rounding);
			d2tk_core_color(core, style->fill_color[triple]);
			d2tk_core_stroke_width(core, 0);
			d2tk_core_fill(core);

			d2tk_core_begin_path(core);
			d2tk_core_rounded_rect(core, hbar, style->rounding);
			d2tk_core_color(core, style->stroke_color[triple]);
			d2tk_core_stroke_width(core, style->border_width);
			d2tk_core_stroke(core);

			d2tk_core_bbox_pop(core, ref);\
		}

		if(flags & D2TK_FLAG_SCROLL_Y)
		{
			d2tk_triple_t triple = D2TK_TRIPLE_NONE;

			if(d2tk_state_is_active(vstate))
			{
				triple |= D2TK_TRIPLE_ACTIVE;
			}

			if(d2tk_state_is_hot(vstate))
			{
				triple |= D2TK_TRIPLE_HOT;
			}

			if(d2tk_state_is_focused(vstate))
			{
				triple |= D2TK_TRIPLE_FOCUS;
			}

			const size_t ref = d2tk_core_bbox_push(core, true, vbar);

			d2tk_core_begin_path(core);
			d2tk_core_rounded_rect(core, vbar, style->rounding);
			d2tk_core_color(core, style->fill_color[triple]);
			d2tk_core_stroke_width(core, 0);
			d2tk_core_fill(core);

			d2tk_core_begin_path(core);
			d2tk_core_rounded_rect(core, vbar, style->rounding);
			d2tk_core_color(core, style->stroke_color[triple]);
			d2tk_core_stroke_width(core, style->border_width);
			d2tk_core_stroke(core);

			d2tk_core_bbox_pop(core, ref);
		}
	}
}

D2TK_API d2tk_scrollbar_t *
d2tk_scrollbar_next(d2tk_base_t *base, d2tk_scrollbar_t *scrollbar)
{
	const d2tk_style_t *style = d2tk_base_get_style(base);
	const d2tk_coord_t s = 10; //FIXME
	const d2tk_coord_t s2 = s*2;

	const d2tk_id_t id = scrollbar->id;
	d2tk_flag_t flags = scrollbar->flags;
	const int32_t *max = scrollbar->max;
	const int32_t *num = scrollbar->num;
	const d2tk_rect_t *rect = scrollbar->rect;
	float *scroll = scrollbar->atom_body->offset;

	const int32_t rel_max [2] = {
		max[0] - num[0],
		max[1] - num[1]
	};
	d2tk_rect_t hbar = {
		.x = rect->x,
		.y = rect->y + rect->h - s,
		.w = rect->w,
		.h = s
	};
	d2tk_rect_t vbar = {
		.x = rect->x + rect->w - s,
		.y = rect->y,
		.w = s,
		.h = rect->h
	};
	d2tk_rect_t sub = *rect;
	d2tk_state_t hstate = D2TK_STATE_NONE;
	d2tk_state_t vstate = D2TK_STATE_NONE;

	const d2tk_id_t hid = (1 << 24) | id;
	const d2tk_id_t vid = (2 << 24) | id;

	if(max[0] < num[0])
	{
		flags &= ~D2TK_FLAG_SCROLL_X;
	}

	if(max[1] < num[1])
	{
		flags &= ~D2TK_FLAG_SCROLL_Y;
	}

	if(flags & D2TK_FLAG_SCROLL_X)
	{
		sub.h -= s;

		hstate |= d2tk_base_is_active_hot(base, hid, &hbar, D2TK_FLAG_SCROLL_X);
	}

	if(flags & D2TK_FLAG_SCROLL_Y)
	{
		sub.w -= s;

		vstate |= d2tk_base_is_active_hot(base, vid, &vbar, D2TK_FLAG_SCROLL_Y);
	}

	if(d2tk_base_is_hit(base, &sub))
	{
		if(flags & D2TK_FLAG_SCROLL_X)
		{
			hstate |= _d2tk_base_is_active_hot_horizontal_scroll(base);
		}

		if(flags & D2TK_FLAG_SCROLL_Y)
		{
			vstate |= _d2tk_base_is_active_hot_vertical_scroll(base);
		}
	}

	const float old_scroll [2] = {
		scroll[0],
		scroll[1]
	};

	if(flags & D2TK_FLAG_SCROLL_X)
	{
		int32_t dw = hbar.w * num[0] / max[0];
		d2tk_clip_int32(s2, &dw, dw);
		const float w = (float)(hbar.w - dw) / rel_max[0];

		if(d2tk_state_is_scroll_right(hstate))
		{
			scroll[0] += base->scroll.odx;
		}
		else if(d2tk_state_is_scroll_left(hstate))
		{
			scroll[0] += base->scroll.odx;
		}
		else if(d2tk_state_is_motion(hstate))
		{
			const float adx = base->mouse.dx;

			scroll[0] += adx / w;
		}

		// always do clipping, as max may have changed in due course
		d2tk_clip_float(0, &scroll[0], rel_max[0]);

		hbar.w = dw;
		hbar.x += scroll[0]*w;
	}

	if(flags & D2TK_FLAG_SCROLL_Y)
	{
		int32_t dh = vbar.h * num[1] / max[1];
		d2tk_clip_int32(s2, &dh, dh);
		const float h = (float)(vbar.h - dh) / rel_max[1];

		if(d2tk_state_is_scroll_down(vstate))
		{
			scroll[1] -= base->scroll.ody;
		}
		else if(d2tk_state_is_scroll_up(vstate))
		{
			scroll[1] -= base->scroll.ody;
		}
		else if(d2tk_state_is_motion(vstate))
		{
			const float ady = base->mouse.dy;

			scroll[1] += ady / h;
		}

		// always do clipping, as max may have changed in due course
		d2tk_clip_float(0, &scroll[1], rel_max[1]);

		vbar.h = dh;
		vbar.y += scroll[1]*h;
	}

	if( (old_scroll[0] != scroll[0]) || (old_scroll[1] != scroll[1]) )
	{
		d2tk_base_set_again(base);
	}

	d2tk_core_t *core = base->core;

	_d2tk_draw_scrollbar(core, hstate, vstate, &hbar, &vbar, style, flags);

	//return state; //FIXME
	return NULL;
}

D2TK_API float
d2tk_scrollbar_get_offset_y(d2tk_scrollbar_t *scrollbar)
{
	return scrollbar->atom_body->offset[1];
}

D2TK_API float
d2tk_scrollbar_get_offset_x(d2tk_scrollbar_t *scrollbar)
{
	return scrollbar->atom_body->offset[0];
}

D2TK_API const d2tk_rect_t *
d2tk_scrollbar_get_rect(d2tk_scrollbar_t *scrollbar)
{
	return &scrollbar->sub;
}
