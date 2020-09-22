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
_d2tk_base_draw_button(d2tk_core_t *core, ssize_t lbl_len, const char *lbl,
	d2tk_align_t align, ssize_t path_len, const char *path,
	const d2tk_rect_t *rect, d2tk_triple_t triple, const d2tk_style_t *style)
{
	const bool has_lbl = lbl_len && lbl;
	const bool has_img = path_len && path;

	if(has_lbl && (lbl_len == -1) ) // zero-terminated string
	{
		lbl_len = strlen(lbl);
	}

	if(has_img && (path_len == -1) ) // zero-terminated string
	{
		path_len = strlen(path);
	}

	const d2tk_hash_dict_t dict [] = {
		{ &triple, sizeof(d2tk_triple_t) },
		{ rect, sizeof(d2tk_rect_t) },
		{ style, sizeof(d2tk_style_t) },
		{ &align, sizeof(d2tk_align_t) },
		{ (lbl ? lbl : path), (lbl ? lbl_len : path_len) },
		{ path, path_len },
		{ NULL, 0 }
	};
	const uint64_t hash = d2tk_hash_dict(dict);

	D2TK_CORE_WIDGET(core, hash, widget)
	{
		d2tk_rect_t bnd_outer;
		d2tk_rect_t bnd_inner;
		d2tk_rect_shrink(&bnd_outer, rect, style->padding);
		d2tk_rect_shrink(&bnd_inner, &bnd_outer, 2*style->padding);

		{
			const size_t ref = d2tk_core_bbox_push(core, true, rect);

			d2tk_core_begin_path(core);
			d2tk_core_rounded_rect(core, &bnd_outer, style->rounding);
			d2tk_core_color(core, style->fill_color[triple]);
			d2tk_core_stroke_width(core, 0);
			d2tk_core_fill(core);

			d2tk_core_begin_path(core);
			d2tk_core_rounded_rect(core, &bnd_outer, style->rounding);
			d2tk_core_color(core, style->stroke_color[triple]);
			d2tk_core_stroke_width(core, style->border_width);
			d2tk_core_stroke(core);

			d2tk_core_bbox_pop(core, ref);
		}

		if(has_lbl)
		{
			const d2tk_coord_t h_2 = rect->h / 2;
			const d2tk_align_t lbl_align = align;

			const size_t ref = d2tk_core_bbox_push(core, true, rect);

			d2tk_core_save(core);
			d2tk_core_scissor(core, &bnd_inner);
			d2tk_core_font_size(core, h_2);
			d2tk_core_font_face(core, strlen(style->font_face), style->font_face);
			d2tk_core_color(core, style->text_stroke_color[triple]);
			d2tk_core_text(core, &bnd_inner, lbl_len, lbl, lbl_align);
			d2tk_core_restore(core);

			d2tk_core_bbox_pop(core, ref);
		}

		if(has_img)
		{
			const d2tk_align_t img_align = D2TK_ALIGN_MIDDLE
				| (has_lbl ? D2TK_ALIGN_RIGHT : D2TK_ALIGN_CENTER);

			const size_t ref = d2tk_core_bbox_push(core, true, rect);

			d2tk_core_image(core, &bnd_inner, path_len, path, img_align);

			d2tk_core_bbox_pop(core, ref);
		}
	}
}

D2TK_API d2tk_state_t
d2tk_base_button_label_image(d2tk_base_t *base, d2tk_id_t id, ssize_t lbl_len,
	const char *lbl, d2tk_align_t align, ssize_t path_len, const char *path,
	const d2tk_rect_t *rect)
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

	_d2tk_base_draw_button(base->core, lbl_len, lbl, align, path_len, path, rect,
		triple, d2tk_base_get_style(base));

	return state;
}

D2TK_API d2tk_state_t
d2tk_base_button_label(d2tk_base_t *base, d2tk_id_t id, ssize_t lbl_len,
	const char *lbl, d2tk_align_t align, const d2tk_rect_t *rect)
{
	return d2tk_base_button_label_image(base, id, lbl_len, lbl,
		align, 0, NULL, rect);
}

D2TK_API d2tk_state_t
d2tk_base_button_image(d2tk_base_t *base, d2tk_id_t id, ssize_t path_len,
	const char *path, const d2tk_rect_t *rect)
{
	return d2tk_base_button_label_image(base, id, 0, NULL,
		D2TK_ALIGN_NONE, path_len, path, rect);
}

D2TK_API d2tk_state_t
d2tk_base_button(d2tk_base_t *base, d2tk_id_t id, const d2tk_rect_t *rect)
{
	return d2tk_base_button_label_image(base, id, 0, NULL,
		D2TK_ALIGN_NONE, 0, NULL, rect);
}

D2TK_API d2tk_state_t
d2tk_base_toggle_label_image(d2tk_base_t *base, d2tk_id_t id, ssize_t lbl_len,
	const char *lbl, d2tk_align_t align, ssize_t path_len, const char *path,
	const d2tk_rect_t *rect, bool *value)
{
	d2tk_state_t state = d2tk_base_is_active_hot(base, id, rect, D2TK_FLAG_NONE);

	if(d2tk_state_is_down(state) || d2tk_state_is_enter(state))
	{
		*value = !*value;
		state |= D2TK_STATE_CHANGED;
	}

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

	_d2tk_base_draw_button(base->core, lbl_len, lbl, align, path_len, path, rect,
		triple, d2tk_base_get_style(base));

	return state;
}

D2TK_API d2tk_state_t
d2tk_base_toggle_label(d2tk_base_t *base, d2tk_id_t id, ssize_t lbl_len,
	const char *lbl, d2tk_align_t align, const d2tk_rect_t *rect, bool *value)
{
	return d2tk_base_toggle_label_image(base, id, lbl_len, lbl, align, 0, NULL,
		rect, value);
}

D2TK_API d2tk_state_t
d2tk_base_toggle(d2tk_base_t *base, d2tk_id_t id, const d2tk_rect_t *rect,
	bool *value)
{
	return d2tk_base_toggle_label(base, id, 0, NULL,
		D2TK_ALIGN_NONE, rect, value);
}
