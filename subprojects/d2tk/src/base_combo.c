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

static inline void
_d2tk_base_draw_combo(d2tk_core_t *core, ssize_t nitms, const char **itms,
	const d2tk_rect_t *rect, d2tk_state_t state, int32_t value,
	const d2tk_style_t *style)
{
	const d2tk_hash_dict_t dict [] = {
		{ &state, sizeof(d2tk_state_t) },
		{ rect, sizeof(d2tk_rect_t) },
		{ style, sizeof(d2tk_style_t) },
		{ &value, sizeof(int32_t) },
		{ &nitms, sizeof(ssize_t) },
		{ itms, sizeof(const char **) }, //FIXME we should actually cache the labels
		{ NULL, 0 }
	};
	const uint64_t hash = d2tk_hash_dict(dict);

	D2TK_CORE_WIDGET(core, hash, widget)
	{
		d2tk_rect_t bnd;
		d2tk_rect_shrink(&bnd, rect, style->padding);

		const d2tk_coord_t w_2 = bnd.w / 2;
		const d2tk_coord_t w_4 = bnd.w / 4;

		d2tk_rect_t left = bnd;
		left.x -= w_4;
		left.w = w_2;

		d2tk_rect_t midd = bnd;
		midd.x += w_4;
		midd.w = w_2;

		d2tk_rect_t right = bnd;
		right.x += w_2 + w_4;
		right.w = w_2;

		d2tk_triple_t triple = D2TK_TRIPLE_NONE;

		if(d2tk_state_is_hot(state))
		{
			triple |= D2TK_TRIPLE_HOT;
		}

		if(d2tk_state_is_focused(state))
		{
			triple |= D2TK_TRIPLE_FOCUS;
		}

		{
			const size_t ref = d2tk_core_bbox_push(core, true, rect);

			uint32_t fill_color_2 = style->fill_color[triple];
			fill_color_2 = (fill_color_2 & 0xffffff00) | (fill_color_2 & 0xff / 2);

			// left filling
			d2tk_core_begin_path(core);
			d2tk_core_rect(core, &left);
			d2tk_core_color(core, fill_color_2);
			d2tk_core_stroke_width(core, 0);
			d2tk_core_fill(core);

			// middle filling
			d2tk_core_begin_path(core);
			d2tk_core_rect(core, &midd);
			d2tk_core_color(core, style->fill_color[triple]);
			d2tk_core_stroke_width(core, 0);
			d2tk_core_fill(core);

			// right filling
			d2tk_core_begin_path(core);
			d2tk_core_rect(core, &right);
			d2tk_core_color(core, fill_color_2);
			d2tk_core_stroke_width(core, 0);
			d2tk_core_fill(core);

			// draw lines above and below text
			const d2tk_coord_t h_8 = bnd.h / 8;
			const d2tk_coord_t dx = bnd.w / nitms;
			const d2tk_coord_t x0 = bnd.x;
			const d2tk_coord_t x1 = bnd.x + value*dx;
			const d2tk_coord_t x2 = x1 + dx;
			const d2tk_coord_t x3 = bnd.x + bnd.w;
			const d2tk_coord_t y0 = bnd.y + h_8;
			const d2tk_coord_t y1 = bnd.y + bnd.h - h_8;

			d2tk_core_begin_path(core);
			d2tk_core_move_to(core, x0, y0);
			d2tk_core_line_to(core, x3, y0);
			d2tk_core_color(core, style->stroke_color[D2TK_TRIPLE_NONE]);
			d2tk_core_stroke_width(core, style->border_width);
			d2tk_core_stroke(core);

			d2tk_core_begin_path(core);
			d2tk_core_move_to(core, x0, y1);
			d2tk_core_line_to(core, x3, y1);
			d2tk_core_color(core, style->stroke_color[D2TK_TRIPLE_NONE]);
			d2tk_core_stroke_width(core, style->border_width*2);
			d2tk_core_stroke(core);

			d2tk_core_begin_path(core);
			d2tk_core_move_to(core, x1, y0);
			d2tk_core_line_to(core, x2, y0);
			d2tk_core_color(core, style->fill_color[triple | D2TK_TRIPLE_ACTIVE]);
			d2tk_core_stroke_width(core, style->border_width);
			d2tk_core_stroke(core);

			d2tk_core_begin_path(core);
			d2tk_core_move_to(core, x1, y1);
			d2tk_core_line_to(core, x2, y1);
			d2tk_core_color(core, style->fill_color[triple | D2TK_TRIPLE_ACTIVE]);
			d2tk_core_stroke_width(core, style->border_width*2);
			d2tk_core_stroke(core);

			// draw bounding box
			d2tk_core_begin_path(core);
			d2tk_core_rect(core, &bnd);
			d2tk_core_color(core, style->stroke_color[triple]);
			d2tk_core_stroke_width(core, style->border_width);
			d2tk_core_stroke(core);

			d2tk_core_bbox_pop(core, ref);
		}

		{
			const size_t ref = d2tk_core_bbox_push(core, true, rect);

			// left label
			if(value > 0)
			{
				const char *lbl = itms[value - 1];
				const size_t lbl_len = lbl ? strlen(lbl) : 0;

				d2tk_core_save(core);
				d2tk_core_scissor(core, &left);
				d2tk_core_font_size(core, left.h / 2);
				d2tk_core_font_face(core, strlen(style->font_face), style->font_face);
				d2tk_core_color(core, style->text_stroke_color[D2TK_TRIPLE_NONE]);
				d2tk_core_text(core, &left, lbl_len, lbl, D2TK_ALIGN_CENTERED);
				d2tk_core_restore(core);
			}

			// middle label
			{
				const char *lbl = itms[value];
				const size_t lbl_len = lbl ? strlen(lbl) : 0;

				d2tk_core_save(core);
				d2tk_core_scissor(core, &midd);
				d2tk_core_font_size(core, midd.h / 2);
				d2tk_core_font_face(core, strlen(style->font_face), style->font_face);
				d2tk_core_color(core, style->text_stroke_color[triple]);
				d2tk_core_text(core, &midd, lbl_len, lbl, D2TK_ALIGN_CENTERED);
				d2tk_core_restore(core);
			}

			// right label
			if(value < (nitms-1) )
			{
				const char *lbl = itms[value + 1];
				const size_t lbl_len = lbl ? strlen(lbl) : 0;

				d2tk_core_save(core);
				d2tk_core_scissor(core, &right);
				d2tk_core_font_size(core, right.h / 2);
				d2tk_core_font_face(core, strlen(style->font_face), style->font_face);
				d2tk_core_color(core, style->text_stroke_color[D2TK_TRIPLE_NONE]);
				d2tk_core_text(core, &right, lbl_len, lbl, D2TK_ALIGN_CENTERED);
				d2tk_core_restore(core);
			}

			d2tk_core_bbox_pop(core, ref);
		}
	}
}

D2TK_API d2tk_state_t
d2tk_base_combo(d2tk_base_t *base, d2tk_id_t id, ssize_t nitms,
	const char **itms, const d2tk_rect_t *rect, int32_t *value)
{
	const d2tk_style_t *style = d2tk_base_get_style(base);

	d2tk_state_t state = d2tk_base_is_active_hot(base, id, rect,
		D2TK_FLAG_SCROLL_X | D2TK_FLAG_SCROLL_Y);

	const int32_t old_value = *value;

	if(  d2tk_state_is_scroll_up(state)
			|| d2tk_state_is_scroll_right(state)
			|| d2tk_state_is_enter(state) )
	{
		*value += 1;
	}

	if(  d2tk_state_is_scroll_down(state)
		|| d2tk_state_is_scroll_left(state))
	{
		*value -= 1;
	}

	if(d2tk_state_is_down(state))
	{
		const d2tk_coord_t w_2 = rect->w/2;
		const d2tk_coord_t w_4 = rect->w/4;

		const d2tk_coord_t x1 = rect->x + w_4;
		const d2tk_coord_t x2 = x1 + w_2;

		if(base->mouse.x < x1)
		{
			*value -= 1;
		}
		else if(base->mouse.x > x2)
		{
			*value += 1;
		}
	}

	d2tk_clip_int32(0, value, nitms-1);

	if(*value != old_value)
	{
		state |= D2TK_STATE_CHANGED;
	}

	d2tk_core_t *core = base->core;

	_d2tk_base_draw_combo(core, nitms, itms, rect, state, *value, style);

	return state;
}
