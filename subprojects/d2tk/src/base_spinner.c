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
#include <string.h>

#include "base_internal.h"

#define FONT_CODE_MEDIUM    "FiraCode:medium"

static inline void
_d2tk_base_spinner_draw_dec(d2tk_core_t *core, const d2tk_rect_t *rect,
	d2tk_triple_t triple, const d2tk_style_t *style)
{
	const d2tk_hash_dict_t dict [] = {
		{ &triple, sizeof(d2tk_triple_t) },
		{ rect, sizeof(d2tk_rect_t) },
		{ style, sizeof(d2tk_style_t) },
		{ NULL, 0 }
	};
	const uint64_t hash = d2tk_hash_dict(dict);

	D2TK_CORE_WIDGET(core, hash, widget)
	{
		d2tk_rect_t bnd_outer;
		d2tk_rect_t bnd_inner;
		d2tk_rect_shrink(&bnd_outer, rect, style->padding);
		d2tk_rect_shrink(&bnd_inner, &bnd_outer, 2*style->padding);

		const d2tk_coord_t r_outer = bnd_outer.h/2;
		const d2tk_coord_t r_inner = bnd_inner.h/2;

		{
			const size_t ref = d2tk_core_bbox_push(core, true, rect);
			const d2tk_coord_t cx = bnd_inner.x + bnd_inner.w;
			const d2tk_coord_t cy = bnd_inner.y + r_inner;
			static const char arrow [] = "◀";

			d2tk_core_begin_path(core);
			d2tk_core_arc(core, cx, cy, r_inner, 90, 270, true);
			d2tk_core_close_path(core);
			d2tk_core_color(core, style->fill_color[triple]);
			d2tk_core_stroke_width(core, 0);
			d2tk_core_fill(core);

			d2tk_core_save(core);
			d2tk_core_scissor(core, &bnd_inner);
			d2tk_core_font_size(core, r_inner);
			d2tk_core_font_face(core, strlen(style->font_face), style->font_face);
			d2tk_core_color(core, style->text_stroke_color[triple]);
			d2tk_core_text(core, &bnd_inner, sizeof(arrow), arrow, D2TK_ALIGN_CENTERED);
			d2tk_core_restore(core);

			d2tk_core_begin_path(core);
			d2tk_core_arc(core, cx, cy, r_outer, 90, 270, true);
			d2tk_core_close_path(core);
			d2tk_core_color(core, style->stroke_color[triple]);
			d2tk_core_stroke_width(core, style->border_width);
			d2tk_core_stroke(core);

			d2tk_core_bbox_pop(core, ref);
		}
	}
}

static inline void
_d2tk_base_spinner_draw_inc(d2tk_core_t *core, const d2tk_rect_t *rect,
	d2tk_triple_t triple, const d2tk_style_t *style)
{
	const d2tk_hash_dict_t dict [] = {
		{ &triple, sizeof(d2tk_triple_t) },
		{ rect, sizeof(d2tk_rect_t) },
		{ style, sizeof(d2tk_style_t) },
		{ NULL, 0 }
	};
	const uint64_t hash = d2tk_hash_dict(dict);

	D2TK_CORE_WIDGET(core, hash, widget)
	{
		d2tk_rect_t bnd_outer;
		d2tk_rect_t bnd_inner;
		d2tk_rect_shrink(&bnd_outer, rect, style->padding);
		d2tk_rect_shrink(&bnd_inner, &bnd_outer, 2*style->padding);

		const d2tk_coord_t r_outer = bnd_outer.h/2;
		const d2tk_coord_t r_inner = bnd_inner.h/2;

		{
			const size_t ref = d2tk_core_bbox_push(core, true, rect);
			const d2tk_coord_t cx = bnd_inner.x;
			const d2tk_coord_t cy = bnd_inner.y + r_inner;
			static const char arrow [] = "▶";

			d2tk_core_begin_path(core);
			d2tk_core_arc(core, cx, cy, r_inner, -90, 90, true);
			d2tk_core_close_path(core);
			d2tk_core_color(core, style->fill_color[triple]);
			d2tk_core_stroke_width(core, 0);
			d2tk_core_fill(core);

			d2tk_core_save(core);
			d2tk_core_scissor(core, &bnd_inner);
			d2tk_core_font_size(core, r_inner);
			d2tk_core_font_face(core, strlen(style->font_face), style->font_face);
			d2tk_core_color(core, style->text_stroke_color[triple]);
			d2tk_core_text(core, &bnd_inner, sizeof(arrow), arrow, D2TK_ALIGN_CENTERED);
			d2tk_core_restore(core);

			d2tk_core_begin_path(core);
			d2tk_core_arc(core, cx, cy, r_outer, -90, 90, true);
			d2tk_core_close_path(core);
			d2tk_core_color(core, style->stroke_color[triple]);
			d2tk_core_stroke_width(core, style->border_width);
			d2tk_core_stroke(core);

			d2tk_core_bbox_pop(core, ref);
		}
	}
}

static d2tk_state_t
_d2tk_base_spinner_dec(d2tk_base_t *base, d2tk_id_t id, const d2tk_rect_t *rect)
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

	_d2tk_base_spinner_draw_dec(base->core, rect, triple,
		d2tk_base_get_style(base));

	return state;
}

static d2tk_state_t
_d2tk_base_spinner_inc(d2tk_base_t *base, d2tk_id_t id, const d2tk_rect_t *rect)
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

	_d2tk_base_spinner_draw_inc(base->core, rect, triple,
		d2tk_base_get_style(base));

	return state;
}

D2TK_API d2tk_state_t
d2tk_base_spinner_int32(d2tk_base_t *base, d2tk_id_t id, const d2tk_rect_t *rect,
	ssize_t lbl_len, const char *lbl, int32_t min, int32_t *value, int32_t max)
{
	d2tk_state_t state = D2TK_STATE_NONE;
	const d2tk_style_t *style = d2tk_base_get_style(base);
	const d2tk_coord_t h2 = rect->h/2 + style->padding*3;
	const d2tk_coord_t fract [3] = { h2, 0, h2 };
	D2TK_BASE_LAYOUT(rect, 3, fract, D2TK_FLAG_LAYOUT_X_ABS, lay)
	{
		const unsigned k = d2tk_layout_get_index(lay);
		const d2tk_rect_t *lrect= d2tk_layout_get_rect(lay);
		const d2tk_id_t itrid = D2TK_ID_IDX(k);
		const d2tk_id_t subid = (itrid << 32) | id;

		switch(k)
		{
			case 0:
			{
				const d2tk_state_t substate = _d2tk_base_spinner_dec(base, subid, lrect);

				if(d2tk_state_is_changed(substate))
				{
					const int32_t old_value = *value;

					*value -= 1;
					d2tk_clip_int32(min, value, max);

					if(old_value != *value)
					{
						state |= D2TK_STATE_CHANGED;
					}
				}
			} break;
			case 1:
			{
				const d2tk_state_t substate = d2tk_base_bar_int32(base, subid, lrect,
					min, value, max);

				state |= substate;

				d2tk_rect_t bnd;
				d2tk_rect_shrink(&bnd, lrect, style->padding*5);

				const d2tk_style_t *old_style = d2tk_base_get_style(base);
				d2tk_style_t style = *old_style;
				style.font_face = FONT_CODE_MEDIUM;

				const bool grow = d2tk_state_is_focused(substate)
					|| d2tk_state_is_hot(substate);

				if(grow)
				{
					d2tk_base_set_style(base, &style);

					char lbl2 [16];
					const ssize_t lbl2_len = snprintf(lbl2, sizeof(lbl2), "%+"PRIi32, *value);
					d2tk_base_label(base, lbl2_len, lbl2, 0.66f, &bnd,
						D2TK_ALIGN_BOTTOM | D2TK_ALIGN_RIGHT);

					d2tk_base_set_style(base, old_style);

					if(lbl_len && lbl)
					{
						d2tk_base_label(base, lbl_len, lbl, 0.33f, &bnd,
							D2TK_ALIGN_TOP | D2TK_ALIGN_LEFT);
					}
				}
				else
				{
					d2tk_base_set_style(base, &style);

					char lbl2 [16];
					const ssize_t lbl2_len = snprintf(lbl2, sizeof(lbl2), "%+"PRIi32, *value);
					d2tk_base_label(base, lbl2_len, lbl2, 0.33f, &bnd,
						D2TK_ALIGN_BOTTOM | D2TK_ALIGN_RIGHT);

					d2tk_base_set_style(base, old_style);

					if(lbl_len && lbl)
					{
						d2tk_base_label(base, lbl_len, lbl, 0.66f, &bnd,
							D2TK_ALIGN_TOP| D2TK_ALIGN_LEFT);
					}
				}
			} break;
			case 2:
			{
				const d2tk_state_t substate = _d2tk_base_spinner_inc(base, subid, lrect);

				if(d2tk_state_is_changed(substate))
				{
					const int32_t old_value = *value;

					*value += 1;
					d2tk_clip_int32(min, value, max);

					if(old_value != *value)
					{
						state |= D2TK_STATE_CHANGED;
					}
				}
			} break;
		}
	}

	return state;
}

D2TK_API d2tk_state_t
d2tk_base_spinner_float(d2tk_base_t *base, d2tk_id_t id, const d2tk_rect_t *rect,
	ssize_t lbl_len, const char *lbl, float min, float *value, float max)
{
	d2tk_state_t state = D2TK_STATE_NONE;
	const d2tk_style_t *style = d2tk_base_get_style(base);
	const d2tk_coord_t h2 = rect->h/2 + style->padding*3;
	const d2tk_coord_t fract [3] = { h2, 0, h2 };
	D2TK_BASE_LAYOUT(rect, 3, fract, D2TK_FLAG_LAYOUT_X_ABS, lay)
	{
		const unsigned k = d2tk_layout_get_index(lay);
		const d2tk_rect_t *lrect= d2tk_layout_get_rect(lay);
		const d2tk_id_t itrid = D2TK_ID_IDX(k);
		const d2tk_id_t subid = (itrid << 32) | id;

		switch(k)
		{
			case 0:
			{
				const d2tk_state_t substate = _d2tk_base_spinner_dec(base, subid, lrect);

				if(d2tk_state_is_changed(substate))
				{
					const float old_value = *value;

					*value -= 0.01f; //FIXME
					d2tk_clip_float(min, value, max);

					if(old_value != *value)
					{
						state |= D2TK_STATE_CHANGED;
					}
				}
			} break;
			case 1:
			{
				const d2tk_state_t substate = d2tk_base_bar_float(base, subid, lrect,
					min, value, max);

				state |= substate;

				d2tk_rect_t bnd;
				d2tk_rect_shrink(&bnd, lrect, style->padding*5);

				const d2tk_style_t *old_style = d2tk_base_get_style(base);
				d2tk_style_t style = *old_style;
				style.font_face = FONT_CODE_MEDIUM;

				const bool grow = d2tk_state_is_focused(substate)
					|| d2tk_state_is_hot(substate);

				if(grow)
				{
					d2tk_base_set_style(base, &style);

					char lbl2 [16];
					const ssize_t lbl2_len = snprintf(lbl2, sizeof(lbl2), "%+.4f", *value);
					d2tk_base_label(base, lbl2_len, lbl2, 0.66f, &bnd,
						D2TK_ALIGN_BOTTOM | D2TK_ALIGN_RIGHT);

					d2tk_base_set_style(base, old_style);

					if(lbl_len && lbl)
					{
						d2tk_base_label(base, lbl_len, lbl, 0.33f, &bnd,
							D2TK_ALIGN_TOP | D2TK_ALIGN_LEFT);
					}
				}
				else
				{
					d2tk_base_set_style(base, &style);

					char lbl2 [16];
					const ssize_t lbl2_len = snprintf(lbl2, sizeof(lbl2), "%+.4f", *value);
					d2tk_base_label(base, lbl2_len, lbl2, 0.33f, &bnd,
						D2TK_ALIGN_BOTTOM | D2TK_ALIGN_RIGHT);

					d2tk_base_set_style(base, old_style);

					if(lbl_len && lbl)
					{
						d2tk_base_label(base, lbl_len, lbl, 0.66f, &bnd,
							D2TK_ALIGN_TOP | D2TK_ALIGN_LEFT);
					}
				}
			} break;
			case 2:
			{
				const d2tk_state_t substate = _d2tk_base_spinner_inc(base, subid, lrect);

				if(d2tk_state_is_changed(substate))
				{
					const float old_value = *value;

					*value += 0.01f; //FIXME
					d2tk_clip_float(min, value, max);

					if(old_value != *value)
					{
						state |= D2TK_STATE_CHANGED;
					}
				}
			} break;
		}
	}

	return state;
}

D2TK_API d2tk_state_t
d2tk_base_spinner_bool(d2tk_base_t *base, d2tk_id_t id, const d2tk_rect_t *rect,
	ssize_t lbl_len, const char *lbl, bool *value)
{
	d2tk_state_t state = D2TK_STATE_NONE;
	const d2tk_style_t *style = d2tk_base_get_style(base);
	const d2tk_coord_t h2 = rect->h/2 + style->padding*3;
	const d2tk_coord_t fract [3] = { h2, 0, rect->h };
	D2TK_BASE_LAYOUT(rect, 3, fract, D2TK_FLAG_LAYOUT_X_ABS, lay)
	{
		const unsigned k = d2tk_layout_get_index(lay);
		const d2tk_rect_t *lrect= d2tk_layout_get_rect(lay);
		const d2tk_id_t itrid = D2TK_ID_IDX(k);
		const d2tk_id_t subid = (itrid << 32) | id;

		switch(k)
		{
			case 1:
			{
				d2tk_rect_t bnd;
				d2tk_rect_shrink(&bnd, lrect, style->padding*5);

				if(lbl_len && lbl)
				{
					d2tk_base_label(base, lbl_len, lbl, 0.66f, &bnd,
						D2TK_ALIGN_TOP | D2TK_ALIGN_LEFT);
				}
			} break;
			case 2:
			{
				state = d2tk_base_dial_bool(base, subid, lrect, value);
			} break;
		}
	}

	return state;
}

D2TK_API d2tk_state_t
d2tk_base_spinner_wave_float(d2tk_base_t *base, d2tk_id_t id, const d2tk_rect_t *rect,
	ssize_t lbl_len, const char *lbl, float min, const float *value, int32_t nelem, float max)
{
	d2tk_state_t state = D2TK_STATE_NONE;
	const d2tk_style_t *style = d2tk_base_get_style(base);
	const d2tk_coord_t h2 = rect->h/2 + style->padding*3;
	const d2tk_coord_t fract [3] = { h2, 0, h2 };
	D2TK_BASE_LAYOUT(rect, 3, fract, D2TK_FLAG_LAYOUT_X_ABS, lay)
	{
		const unsigned k = d2tk_layout_get_index(lay);
		const d2tk_rect_t *lrect= d2tk_layout_get_rect(lay);
		const d2tk_id_t itrid = D2TK_ID_IDX(k);
		const d2tk_id_t subid = (itrid << 32) | id;

		switch(k)
		{
			case 0:
			{
				//FIXME decrease viewport
			} break;
			case 1:
			{
				const d2tk_state_t substate = d2tk_base_wave_float(base, subid, lrect,
					min, value, nelem, max);

				state |= substate;

				d2tk_rect_t bnd;
				d2tk_rect_shrink(&bnd, lrect, style->padding*5);

				if(lbl_len && lbl)
				{
					d2tk_base_label(base, lbl_len, lbl, 0.66f, &bnd,
						D2TK_ALIGN_TOP | D2TK_ALIGN_LEFT);
				}
			} break;
			case 2:
			{
				//FIXME increase viewport
			} break;
		}
	}

	return state;
}
