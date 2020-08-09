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

#include <math.h>

#include "base_internal.h"

typedef struct _d2tk_atom_body_pane_t d2tk_atom_body_pane_t;

struct _d2tk_atom_body_pane_t {
	float fraction;
};

struct _d2tk_pane_t {
	d2tk_atom_body_pane_t *atom_body;
	unsigned k;
	d2tk_rect_t rect [2];
};

const size_t d2tk_atom_body_pane_sz = sizeof(d2tk_atom_body_pane_t);
const size_t d2tk_pane_sz = sizeof(d2tk_pane_t);

static void
_d2tk_draw_pane(d2tk_core_t *core, d2tk_state_t state, const d2tk_rect_t *sub,
	const d2tk_style_t *style, d2tk_flag_t flags)
{
	const d2tk_hash_dict_t dict [] = {
		{ &state, sizeof(d2tk_state_t) },
		{ sub, sizeof(d2tk_rect_t) },
		{ style, sizeof(d2tk_style_t) },
		{ &flags, sizeof(d2tk_flag_t) },
		{ NULL, 0 }
	};
	const uint64_t hash = d2tk_hash_dict(dict);

	D2TK_CORE_WIDGET(core, hash, widget)
	{
		const d2tk_coord_t s = 10; //FIXME
		const d2tk_coord_t r = (s - 2) / 2; //FIXME

		d2tk_triple_t triple = D2TK_TRIPLE_NONE;

		if(d2tk_state_is_active(state))
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

		d2tk_coord_t x0, x1, x2, y0, y1, y2;

		if(flags & D2TK_FLAG_PANE_X)
		{
			x0 = sub->x + sub->w/2;
			x1 = x0;
			x2 = x0;

			y0 = sub->y;
			y1 = y0 + sub->h/2;
			y2 = y0 + sub->h;
		}
		else // flags & D2TK_FLAG_PANE_Y
		{
			x0 = sub->x;
			x1 = x0 + sub->w/2;
			x2 = x0 + sub->w;

			y0 = sub->y + sub->h/2;
			y1 = y0;
			y2 = y0;
		}

		const size_t ref = d2tk_core_bbox_push(core, true, sub);

		d2tk_core_begin_path(core);
		d2tk_core_move_to(core, x0, y0);
		d2tk_core_line_to(core, x2, y2);
		d2tk_core_color(core, style->stroke_color[triple]);
		d2tk_core_stroke_width(core, style->border_width);
		d2tk_core_stroke(core);

		d2tk_core_begin_path(core);
		d2tk_core_arc(core, x1, y1, r, 0, 360, true);
		d2tk_core_color(core, style->fill_color[triple]);
		d2tk_core_stroke_width(core, 0);
		d2tk_core_fill(core);

		d2tk_core_begin_path(core);
		d2tk_core_arc(core, x1, y1, r, 0, 360, true);
		d2tk_core_color(core, style->stroke_color[triple]);
		d2tk_core_stroke_width(core, style->border_width);
		d2tk_core_stroke(core);

		d2tk_core_bbox_pop(core, ref);\
	}
}

D2TK_API d2tk_pane_t *
d2tk_pane_begin(d2tk_base_t *base, const d2tk_rect_t *rect, d2tk_id_t id,
	d2tk_flag_t flags, float fmin, float fmax, float fstep, d2tk_pane_t *pane)
{
	pane->k = 0;
	pane->rect[0] = *rect;
	pane->rect[1] = *rect;
	pane->atom_body = _d2tk_base_get_atom(base, id, D2TK_ATOM_PANE, NULL);

	float *fraction = &pane->atom_body->fraction;
	d2tk_rect_t sub = *rect;

	const d2tk_coord_t s = 10; //FIXME

	d2tk_clip_float(fmin, &pane->atom_body->fraction, fmax);

	if(flags & D2TK_FLAG_PANE_X)
	{
		pane->rect[0].w *= pane->atom_body->fraction;

		sub.x += pane->rect[0].w;
		sub.w = s;

		const d2tk_coord_t rsvd = pane->rect[0].w + s;
		pane->rect[1].x += rsvd;
		pane->rect[1].w -= rsvd;
	}
	else if(flags & D2TK_FLAG_PANE_Y)
	{
		pane->rect[0].h *= pane->atom_body->fraction;

		sub.y += pane->rect[0].h;
		sub.h = s;

		const d2tk_coord_t rsvd = pane->rect[0].h + s;
		pane->rect[1].y += rsvd;
		pane->rect[1].h -= rsvd;
	}

	d2tk_state_t state = D2TK_STATE_NONE;

	if(flags & D2TK_FLAG_PANE_X)
	{
		state |= d2tk_base_is_active_hot(base, id, &sub, D2TK_FLAG_NONE);
	}
	else if(flags & D2TK_FLAG_PANE_Y)
	{
		state |= d2tk_base_is_active_hot(base, id, &sub, D2TK_FLAG_NONE);
	}

	const float old_fraction = *fraction;

	if(flags & D2TK_FLAG_PANE_X)
	{
		if(d2tk_state_is_scroll_left(state))
		{
			*fraction -= fstep;
		}
		else if(d2tk_state_is_scroll_right(state))
		{
			*fraction += fstep;
		}
		else if(d2tk_state_is_motion(state))
		{
			*fraction = roundf((float)(base->mouse.x - rect->x) / rect->w / fstep) * fstep;
		}
	}
	else if(flags & D2TK_FLAG_PANE_Y)
	{
		if(d2tk_state_is_scroll_up(state))
		{
			*fraction -= fstep;
		}
		else if(d2tk_state_is_scroll_down(state))
		{
			*fraction += fstep;
		}
		else if(d2tk_state_is_motion(state))
		{
			*fraction = roundf((float)(base->mouse.y - rect->y) / rect->h / fstep) * fstep;
		}
	}

	if(old_fraction != *fraction)
	{
		state |= D2TK_STATE_CHANGED;
		d2tk_base_set_again(base);
	}

	const d2tk_style_t *style = d2tk_base_get_style(base);

	d2tk_core_t *core = base->core;

	_d2tk_draw_pane(core, state, &sub, style, flags);

	return pane;
}

D2TK_API bool
d2tk_pane_not_end(d2tk_pane_t *pane)
{
	return pane ? true : false;
}

D2TK_API d2tk_pane_t *
d2tk_pane_next(d2tk_pane_t *pane)
{
	return (pane->k++ == 0) ? pane : NULL;
}

D2TK_API float
d2tk_pane_get_fraction(d2tk_pane_t *pane)
{
	return pane->atom_body->fraction;
}

D2TK_API unsigned
d2tk_pane_get_index(d2tk_pane_t *pane)
{
	return pane->k;
}

D2TK_API const d2tk_rect_t *
d2tk_pane_get_rect(d2tk_pane_t *pane)
{
	return &pane->rect[pane->k];
}
