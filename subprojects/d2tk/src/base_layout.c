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

struct _d2tk_layout_t {
	unsigned N;
	const d2tk_coord_t *frac;
	d2tk_flag_t flag;
	d2tk_coord_t dd;
	d2tk_coord_t rem;
	unsigned k;
	d2tk_rect_t rect;
};

const size_t d2tk_layout_sz = sizeof(d2tk_layout_t);

D2TK_API d2tk_layout_t *
d2tk_layout_begin(const d2tk_rect_t *rect, unsigned N, const d2tk_coord_t *frac,
	d2tk_flag_t flag, d2tk_layout_t *lay)
{
	lay->N = N;
	lay->frac = frac;
	lay->flag = flag;

	unsigned tot = 0;
	unsigned missing = 0;
	for(unsigned i = 0; i < N; i++)
	{
		tot += frac[i];

		if(frac[i] == 0)
		{
			missing += 1;
		}
	}

	lay->k = 0;
	lay->rect.x = rect->x;
	lay->rect.y = rect->y;

	if(lay->flag & D2TK_FLAG_LAYOUT_Y)
	{
		if(lay->flag & D2TK_FLAG_LAYOUT_REL)
		{
			lay->dd = tot ? (rect->h / tot) : 0;
		}
		else
		{
			lay->dd = 1;
		}

		lay->rem = missing ? (rect->h - tot) / missing : 0;

		lay->rect.h = lay->frac[lay->k]
			? lay->dd * lay->frac[lay->k]
			: lay->rem;
		lay->rect.w = rect->w;
	}
	else // D2TK_FLAG_LAYOUT_X
	{
		if(lay->flag & D2TK_FLAG_LAYOUT_REL)
		{
			lay->dd = tot ? (rect->w / tot) : 0;
		}
		else
		{
			lay->dd = 1;
		}

		lay->rem = missing ? (rect->w - tot) / missing : 0;

		lay->rect.w = lay->frac[lay->k]
			? lay->dd * lay->frac[lay->k]
			: lay->rem;
		lay->rect.h = rect->h;
	}

	return lay;
}

D2TK_API bool
d2tk_layout_not_end(d2tk_layout_t *lay)
{
	return lay;
}

D2TK_API d2tk_layout_t *
d2tk_layout_next(d2tk_layout_t *lay)
{
	if(++lay->k >= lay->N)
	{
		return NULL;
	}

	if(lay->flag & D2TK_FLAG_LAYOUT_Y)
	{
		lay->rect.y += lay->rect.h;
		lay->rect.h = lay->frac[lay->k]
			? lay->dd * lay->frac[lay->k]
			: lay->rem;
	}
	else // D2TK_FLAG_LAYOUT_X
	{
		lay->rect.x += lay->rect.w;
		lay->rect.w = lay->frac[lay->k]
			? lay->dd * lay->frac[lay->k]
			: lay->rem;
	}

	return lay;
}

D2TK_API unsigned
d2tk_layout_get_index(d2tk_layout_t *lay)
{
	return lay->k;
}

D2TK_API const d2tk_rect_t *
d2tk_layout_get_rect(d2tk_layout_t *lay)
{
	return &lay->rect;
}
