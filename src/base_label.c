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

D2TK_API d2tk_state_t
d2tk_base_label(d2tk_base_t *base, ssize_t lbl_len, const char *lbl,
	float mul, const d2tk_rect_t *rect, d2tk_align_t align)
{
	const bool has_lbl = lbl_len && lbl;

	if(has_lbl && (lbl_len == -1) ) // zero terminated string
	{
		lbl_len = strlen(lbl);
	}

	const d2tk_style_t *style = d2tk_base_get_style(base);

	const d2tk_hash_dict_t dict [] = {
		{ rect, sizeof(d2tk_rect_t) },
		{ style, sizeof(d2tk_style_t) },
		{ &mul, sizeof(float) },
		{ &align, sizeof(d2tk_align_t) },
		{ lbl, lbl_len },
		{ NULL, 0 }
	};
	const uint64_t hash = d2tk_hash_dict(dict);

	d2tk_core_t *core = base->core;

	D2TK_CORE_WIDGET(core, hash, widget)
	{
		d2tk_rect_t bnd;
		d2tk_rect_shrink(&bnd, rect, style->padding);

		const d2tk_triple_t triple = D2TK_TRIPLE_NONE;

		const size_t ref = d2tk_core_bbox_push(core, true, rect);

		if(style->text_fill_color[triple])
		{
				d2tk_core_begin_path(core);
				d2tk_core_rect(core, &bnd);
				d2tk_core_color(core, style->text_fill_color[triple]);
				d2tk_core_stroke_width(core, 0);
				d2tk_core_fill(core);
		}

		if(lbl_len > 0)
		{
			d2tk_core_save(core);
			d2tk_core_scissor(core, &bnd);
			d2tk_core_font_size(core, mul*bnd.h);
			d2tk_core_font_face(core, strlen(style->font_face), style->font_face);
			d2tk_core_color(core, style->text_stroke_color[triple]);
			d2tk_core_text(core, &bnd, lbl_len, lbl, align);
			d2tk_core_restore(core);
		}

		d2tk_core_bbox_pop(core, ref);
	}

	return D2TK_STATE_NONE;
}
