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

D2TK_API void
d2tk_base_cursor(d2tk_base_t *base, const d2tk_rect_t *rect)
{
	d2tk_core_t *core = base->core;
	const d2tk_style_t *style = d2tk_base_get_style(base);

	const d2tk_hash_dict_t dict [] = {
		{ rect, sizeof(rect) },
		{ style, sizeof(d2tk_style_t) },
		{ NULL, 0 }
	};
	const uint64_t hash = d2tk_hash_dict(dict);

	D2TK_CORE_WIDGET(core, hash, widget)
	{
		const d2tk_coord_t x0 = rect->x;
		const d2tk_coord_t x1 = x0 + rect->w/2;
		const d2tk_coord_t x2 = x0 + rect->w;
		const d2tk_coord_t y0 = rect->y;
		const d2tk_coord_t y1 = y0 + rect->h/2;
		const d2tk_coord_t y2 = y0 + rect->h;

		const size_t ref = d2tk_core_bbox_push(core, true, rect);

		d2tk_core_begin_path(core);
		d2tk_core_move_to(core, x0, y0);
		d2tk_core_line_to(core, x1, y2);
		d2tk_core_line_to(core, x1, y1);
		d2tk_core_line_to(core, x2, y1);
		d2tk_core_close_path(core);
		d2tk_core_color(core, style->stroke_color[D2TK_TRIPLE_FOCUS]);
		d2tk_core_stroke_width(core, 0);
		d2tk_core_fill(core);

		d2tk_core_begin_path(core);
		d2tk_core_move_to(core, x0, y0);
		d2tk_core_line_to(core, x1, y2);
		d2tk_core_line_to(core, x1, y1);
		d2tk_core_line_to(core, x2, y1);
		d2tk_core_close_path(core);
		d2tk_core_color(core, style->stroke_color[D2TK_TRIPLE_NONE]);
		d2tk_core_stroke_width(core, 2*style->border_width);
		d2tk_core_stroke(core);

		d2tk_core_bbox_pop(core, ref);
	}
}
