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

struct _d2tk_frame_t {
	d2tk_rect_t rect;
};

const size_t d2tk_frame_sz = sizeof(d2tk_frame_t);

D2TK_API d2tk_frame_t *
d2tk_frame_begin(d2tk_base_t *base, const d2tk_rect_t *rect,
	ssize_t lbl_len, const char *lbl, d2tk_frame_t *frm)
{
	const bool has_lbl = lbl_len && lbl;

	const d2tk_style_t *style = d2tk_base_get_style(base);
	d2tk_core_t *core = base->core;
	const d2tk_coord_t h = 17; //FIXME

	if(has_lbl && (lbl_len == -1) ) // zero-terminated string
	{
		lbl_len = strlen(lbl);
	}

	const d2tk_hash_dict_t dict [] = {
		{ rect, sizeof(d2tk_rect_t) },
		{ style, sizeof(d2tk_style_t) },
		{ lbl, lbl_len },
		{ NULL, 0 }
	};
	const uint64_t hash = d2tk_hash_dict(dict);

	d2tk_rect_shrink(&frm->rect, rect, 2*style->padding);

	if(has_lbl)
	{
		frm->rect.y += h;
		frm->rect.h -= h;
	}

	D2TK_CORE_WIDGET(core, hash, widget)
	{
		d2tk_rect_t bnd_outer;
		d2tk_rect_shrink(&bnd_outer, rect, style->padding);
		d2tk_rect_t bnd_inner = bnd_outer;;

		const size_t ref = d2tk_core_bbox_push(core, true, rect);

		if(has_lbl)
		{
			bnd_inner.h = h;

			d2tk_core_begin_path(core);
			d2tk_core_rounded_rect(core, &bnd_inner, style->rounding);
			d2tk_core_color(core, style->fill_color[D2TK_TRIPLE_NONE]);
			d2tk_core_stroke_width(core, 0);
			d2tk_core_fill(core);

			bnd_inner.x += style->rounding;
			bnd_inner.w -= style->rounding*2;

			d2tk_core_save(core);
			d2tk_core_scissor(core, &bnd_inner);
			d2tk_core_font_size(core, bnd_inner.h - 2*style->padding);
			d2tk_core_font_face(core, strlen(style->font_face), style->font_face);
			d2tk_core_color(core, style->text_stroke_color[D2TK_TRIPLE_NONE]);
			d2tk_core_text(core, &bnd_inner, lbl_len, lbl,
				D2TK_ALIGN_LEFT | D2TK_ALIGN_MIDDLE);
			d2tk_core_restore(core);
		}

		d2tk_core_begin_path(core);
		d2tk_core_rounded_rect(core, &bnd_outer, style->rounding);
		d2tk_core_color(core, style->stroke_color[D2TK_TRIPLE_NONE]);
		d2tk_core_stroke_width(core, style->border_width);
		d2tk_core_stroke(core);

		d2tk_core_bbox_pop(core, ref);
	}

	return frm;
}

D2TK_API bool
d2tk_frame_not_end(d2tk_frame_t *frm)
{
	return frm ? true : false;
}

D2TK_API d2tk_frame_t *
d2tk_frame_next(d2tk_frame_t *frm __attribute__((unused)))
{
	return NULL;
}

D2TK_API const d2tk_rect_t *
d2tk_frame_get_rect(d2tk_frame_t *frm)
{
	return &frm->rect;
}
