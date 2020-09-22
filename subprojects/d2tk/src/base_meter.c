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

#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "base_internal.h"

static inline void
_d2tk_base_draw_meter(d2tk_core_t *core, const d2tk_rect_t *rect,
	d2tk_state_t state, int32_t value, const d2tk_style_t *style)
{
	const d2tk_hash_dict_t dict [] = {
		{ &state, sizeof(d2tk_state_t) },
		{ rect, sizeof(d2tk_rect_t) },
		{ style, sizeof(d2tk_style_t) },
		{ &value, sizeof(int32_t) },
		{ NULL, 0 }
	};
	const uint64_t hash = d2tk_hash_dict(dict);

	D2TK_CORE_WIDGET(core, hash, widget)
	{
#define N 4
#define N_1 (N - 1)
#define L 11

#define dBFS3_min -18 // -54 dBFS
#define dBFS3_max 2  // +6 dBFS
#define dBFS3_range (dBFS3_max - dBFS3_min)

		static const int32_t dBFS3_off [N] = {
			dBFS3_min,
			-2, // -6 dBFS
			0,  // +0 dBFS
			dBFS3_max
		};

		static const uint32_t rgba [N] = {
			0x00ffffff, // cyan
			0x00ff00ff, // green
			0xffff00ff, // yellow
			0xff0000ff  // red
		};

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

		d2tk_rect_t bnd;
		d2tk_rect_shrink(&bnd, rect, style->padding);
		bnd.h /= 2;

		const d2tk_coord_t dx = bnd.w / dBFS3_range;
		const d2tk_coord_t y0 = bnd.y;
		const d2tk_coord_t y1 = y0 + bnd.h;
		const d2tk_coord_t ym = (y0 + y1)/2;

		const d2tk_point_t p [N] = {
			D2TK_POINT(bnd.x + (dBFS3_off[0] - dBFS3_min)*dx, ym),
			D2TK_POINT(bnd.x + (dBFS3_off[1] - dBFS3_min)*dx, ym),
			D2TK_POINT(bnd.x + (dBFS3_off[2] - dBFS3_min)*dx, ym),
			D2TK_POINT(bnd.x + (dBFS3_off[3] - dBFS3_min)*dx, ym)
		};

		// dependent on value, e.g. linear gradient
		{
			const d2tk_coord_t xv = bnd.x + (value - dBFS3_min*3)*dx/3;

			const size_t ref = d2tk_core_bbox_push(core, true, rect);

			for(unsigned i = 0; i < N_1; i++)
			{
				const d2tk_coord_t x0 = p[i].x;
				d2tk_coord_t x1 = p[i+1].x;
				bool do_break = false;

				if(x1 > xv)
				{
					x1 = xv;
					do_break = true;
				}

				const d2tk_rect_t bnd2 = {
					.x = x0,
					.y = y0,
					.w = x1 - x0,
					.h = bnd.h
				};

				d2tk_core_begin_path(core);
				d2tk_core_rect(core, &bnd2);
				d2tk_core_linear_gradient(core, &p[i], &rgba[i]);
				d2tk_core_stroke_width(core, 0);
				d2tk_core_fill(core);

				if(do_break)
				{
					break;
				}
			}

			d2tk_core_bbox_pop(core, ref);
		}

		// independent on value, eg scale + border
		{
			const size_t ref = d2tk_core_bbox_push(core, true, rect);

			for(int32_t dBFS3 = dBFS3_min + 1; dBFS3 < dBFS3_max; dBFS3++)
			{
				const d2tk_coord_t x = bnd.x + (dBFS3 - dBFS3_min)*dx;

				d2tk_core_begin_path(core);
				d2tk_core_move_to(core, x, y0);
				d2tk_core_line_to(core, x, y1);
				d2tk_core_color(core, style->stroke_color[triple]);
				d2tk_core_stroke_width(core, style->border_width);
				d2tk_core_stroke(core);
			}

			{
				const d2tk_rect_t bnd2 = {
					.x = bnd.x,
					.y = y0,
					.w = p[N_1].x - p[0].x,
					.h = bnd.h
				};

				d2tk_core_begin_path(core);
				d2tk_core_rect(core, &bnd2);
				d2tk_core_color(core, style->stroke_color[triple]);
				d2tk_core_stroke_width(core, style->border_width);
				d2tk_core_stroke(core);
			}

			static const unsigned lbls [L] = {
				+2,
				+1,
				+0,
				-1,
				-2,
				-4,
				-8,
				-12,
				-15, // unit
				-16
			};

			for(unsigned i = 0; i<L; i++)
			{
				const int32_t dBFS3 = lbls[i] - 1;
				const d2tk_coord_t x = bnd.x + (dBFS3 - dBFS3_min)*dx;
				const bool is_unit = (i == 8);

				const d2tk_rect_t bnd2 = {
					.x = x,
					.y = y0 + bnd.h,
					.w = is_unit ? 3*dx : dx,
					.h = bnd.h
				};

				char lbl [16];
				const ssize_t lbl_len = is_unit
					? snprintf(lbl, sizeof(lbl), " dBFS")
					: snprintf(lbl, sizeof(lbl), "%+"PRIi32, (dBFS3+1)*3);

				d2tk_core_save(core);
				d2tk_core_scissor(core, &bnd2);
				d2tk_core_font_size(core, bnd2.h);
				d2tk_core_font_face(core, strlen(style->font_face), style->font_face);
				d2tk_core_color(core, style->text_stroke_color[triple]);
				d2tk_core_text(core, &bnd2, lbl_len, lbl,
					D2TK_ALIGN_BOTTOM | (is_unit ? D2TK_ALIGN_LEFT: D2TK_ALIGN_RIGHT));
				d2tk_core_restore(core);
			}

			d2tk_core_bbox_pop(core, ref);
		}

#undef dBFS3_range
#undef dBFS3_max
#undef dBFS3_min

#undef L
#undef N
#undef N_1
	}
}

D2TK_API d2tk_state_t
d2tk_base_meter(d2tk_base_t *base, d2tk_id_t id, const d2tk_rect_t *rect,
	const int32_t *value)
{
	const d2tk_style_t *style = d2tk_base_get_style(base);

	const d2tk_state_t state = d2tk_base_is_active_hot(base, id, rect,
		D2TK_FLAG_NONE);

	d2tk_core_t *core = base->core;

	_d2tk_base_draw_meter(core, rect, state, *value, style);

	return state;
}
