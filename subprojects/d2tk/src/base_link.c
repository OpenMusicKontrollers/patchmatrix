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

#include <string.h>

#include "base_internal.h"

static void
_d2tk_base_draw_link(d2tk_base_t *base, ssize_t lbl_len, const char *lbl,
	float mul, const d2tk_rect_t *rect, d2tk_align_t align, d2tk_triple_t triple,
	const d2tk_style_t *style)
{
	const bool has_lbl = lbl_len && lbl;

	if(has_lbl && (lbl_len == -1) ) // zero terminated string
	{
		lbl_len = strlen(lbl);
	}

	const d2tk_hash_dict_t dict [] = {
		{ &triple, sizeof(d2tk_triple_t) },
		{ rect, sizeof(d2tk_rect_t) },
		{ style, sizeof(d2tk_style_t) },
		{ &mul, sizeof(float) },
		{ &align, sizeof(d2tk_align_t) },
		{ lbl, lbl_len },
		{ NULL, 0 }
	};
	const uint64_t hash = d2tk_hash_dict(dict);

	d2tk_core_t *core = base->core;

	//FIXME analyse link and draw underline, hover, etc.

	D2TK_CORE_WIDGET(core, hash, widget)
	{
		d2tk_rect_t bnd;
		d2tk_rect_shrink(&bnd, rect, style->padding);

		{
			const size_t ref = d2tk_core_bbox_push(core, true, rect);

			d2tk_core_save(core);
			d2tk_core_scissor(core, &bnd);
			d2tk_core_font_size(core, mul*bnd.h);
			d2tk_core_font_face(core, strlen(style->font_face), style->font_face);
			d2tk_core_color(core, style->text_stroke_color[triple]);
			d2tk_core_text(core, &bnd, lbl_len, lbl, align);
			d2tk_core_restore(core);

			d2tk_core_bbox_pop(core, ref);
		}

		{
			const size_t ref = d2tk_core_bbox_push(core, true, rect);

			d2tk_core_begin_path(core);
			d2tk_core_move_to(core, bnd.x, bnd.y + bnd.h);
			d2tk_core_line_to(core, bnd.x + bnd.w, bnd.y + bnd.h);
			if(triple & D2TK_TRIPLE_FOCUS)
			{
				d2tk_core_color(core, style->stroke_color[triple]);
			}
			else
			{
				d2tk_core_color(core, style->fill_color[triple]);
			}
			d2tk_core_stroke_width(core, style->border_width);
			d2tk_core_stroke(core);

			d2tk_core_bbox_pop(core, ref);
		}
	}
}

D2TK_API d2tk_state_t
d2tk_base_link(d2tk_base_t *base, d2tk_id_t id, ssize_t lbl_len, const char *lbl,
	float mul, const d2tk_rect_t *rect, d2tk_align_t align)
{
	d2tk_state_t state = d2tk_base_is_active_hot(base, id, rect, D2TK_FLAG_NONE);

	if(d2tk_state_is_down(state) || d2tk_state_is_enter(state))
	{
		state |= D2TK_STATE_CHANGED;
	}

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

	_d2tk_base_draw_link(base, lbl_len, lbl, mul, rect, align, triple,
		d2tk_base_get_style(base));

	return state;
}
