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
#include <string.h>

#include "base_internal.h"

static void
_d2tk_base_draw_text_field(d2tk_core_t *core, d2tk_state_t state,
	const d2tk_rect_t *rect, const d2tk_style_t *style, char *value,
	d2tk_align_t align)
{
	const d2tk_hash_dict_t dict [] = {
		{ &state, sizeof(d2tk_state_t) },
		{ rect, sizeof(d2tk_rect_t) },
		{ style, sizeof(d2tk_style_t) },
		{ &align, sizeof(d2tk_align_t) },
		{ value, strlen(value) },
		{ NULL, 0 }
	};
	const uint64_t hash = d2tk_hash_dict(dict);

	D2TK_CORE_WIDGET(core, hash, widget)
	{
		d2tk_triple_t triple = D2TK_TRIPLE_NONE;

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

		const d2tk_coord_t h_8 = bnd.h / 8;

		{
			const size_t ref = d2tk_core_bbox_push(core, true, rect);

			// draw background
			d2tk_core_begin_path(core);
			d2tk_core_rect(core, &bnd);
			d2tk_core_color(core, style->fill_color[triple]);
			d2tk_core_stroke_width(core, 0);
			d2tk_core_fill(core);

			// draw lines above and below text
			const d2tk_coord_t x0 = bnd.x;
			const d2tk_coord_t x1 = bnd.x + bnd.w;
			const d2tk_coord_t y0 = bnd.y + h_8;
			const d2tk_coord_t y1 = bnd.y + bnd.h - h_8;

			d2tk_core_begin_path(core);
			d2tk_core_move_to(core, x0, y0);
			d2tk_core_line_to(core, x1, y0);
			d2tk_core_color(core, style->stroke_color[D2TK_TRIPLE_NONE]);
			d2tk_core_stroke_width(core, style->border_width);
			d2tk_core_stroke(core);

			d2tk_core_begin_path(core);
			d2tk_core_move_to(core, x0, y1);
			d2tk_core_line_to(core, x1, y1);
			d2tk_core_color(core, style->stroke_color[D2TK_TRIPLE_NONE]);
			d2tk_core_stroke_width(core, style->border_width);
			d2tk_core_stroke(core);

			// draw bounding box
			d2tk_core_begin_path(core);
			d2tk_core_rect(core, &bnd);
			d2tk_core_color(core, style->stroke_color[triple]);
			d2tk_core_stroke_width(core, style->border_width);
			d2tk_core_stroke(core);

			d2tk_core_bbox_pop(core, ref);
		}

		const size_t valuelen= strlen(value);
		if(valuelen)
		{
			const d2tk_coord_t h_2 = bnd.h / 2;

			d2tk_rect_t bnd2;
			d2tk_rect_shrink_x(&bnd2, &bnd, h_8);

			const size_t ref = d2tk_core_bbox_push(core, true, rect);

			d2tk_core_save(core);
			d2tk_core_scissor(core, &bnd2);
			d2tk_core_font_size(core, h_2);
			d2tk_core_font_face(core, strlen(style->font_face), style->font_face);
			d2tk_core_color(core, style->text_stroke_color[D2TK_TRIPLE_NONE]);
			d2tk_core_text(core, &bnd2, valuelen, value, align);
			d2tk_core_restore(core);

			d2tk_core_bbox_pop(core, ref);
		}
	}
}

D2TK_API d2tk_state_t
d2tk_base_text_field(d2tk_base_t *base, d2tk_id_t id, const d2tk_rect_t *rect,
	size_t maxlen, char *value, d2tk_align_t align, const char *accept)
{
	const d2tk_style_t *style = d2tk_base_get_style(base);

	d2tk_state_t state = d2tk_base_is_active_hot(base, id, rect, D2TK_FLAG_NONE);

	if(d2tk_state_is_focus_in(state))
	{
		_d2tk_base_clear_chars(base); // eat keys

		// copy text from value to edit.text_in
		strncpy(base->edit.text_in, value, maxlen);
	}

	if(d2tk_state_is_focused(state))
	{
		// use edit.text_in
		value = base->edit.text_in;

		if(d2tk_base_get_keymask(base, D2TK_KEYMASK_BACKSPACE, true))
		{
			const ssize_t ulen = utf8len(value) - 1;
			char *head = value;
			utf8_int32_t codepoint;

			for(ssize_t i = 0; i < ulen; i++)
			{
				head = utf8codepoint(head, &codepoint);
			}

			head[0] = '\0';
			//_d2tk_base_clear_chars(base); // eat key
		}
		else if(d2tk_base_get_keymask(base, D2TK_KEYMASK_DEL, true))
		{
			memset(value, 0x0, maxlen);
			//_d2tk_base_clear_chars(base); // eat key
		}

		if(base->keys.nchars)
		{
			const utf8_int32_t *head = base->keys.chars;

			const ssize_t len = strlen(value);
			char *tail = &value[len];

			utf8_int32_t codepoint;

			for(size_t i = 0; i < base->keys.nchars; i++)
			{
				codepoint = head[i];

				if(accept && !utf8chr(accept, codepoint))
				{
					continue;
				}

				const ssize_t left = maxlen - (tail - value);
				if(left > 0)
				{
					tail = utf8catcodepoint(tail, codepoint, left);
				}
			}

			_d2tk_base_clear_chars(base); // eat keys
		}

		char *buf = alloca(maxlen + 1);
		if(buf)
		{
			snprintf(buf, maxlen, "%s|", value);
			value = buf;
		}
	}

	if(d2tk_state_is_focus_out(state))
	{
		// copy text from edit.text_out to value
		strncpy(value, base->edit.text_out, maxlen);

		state |= D2TK_STATE_CHANGED;
	}

	//FIXME handle d2tk_state_is_enter(state)

	d2tk_core_t *core = base->core;

	_d2tk_base_draw_text_field(core, state, rect, style, value, align);

	return state;
}
