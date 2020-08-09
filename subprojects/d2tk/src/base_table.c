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

struct _d2tk_table_t {
	unsigned x;
	unsigned y;
	unsigned N;
	unsigned NM;
	unsigned k;
	unsigned x0;
	d2tk_rect_t rect;
};

const size_t d2tk_table_sz = sizeof(d2tk_table_t);

D2TK_API d2tk_table_t *
d2tk_table_begin(const d2tk_rect_t *rect, unsigned N, unsigned M,
	d2tk_flag_t flag, d2tk_table_t *tab)
{
	if( (N == 0) || (M == 0) )
	{
		return NULL;
	}

	unsigned w;
	unsigned h;

	tab->x = 0;
	tab->y = 0;
	tab->k = 0;
	tab->x0 = rect->x;

	if(flag & D2TK_FLAG_TABLE_REL)
	{
		w = rect->w / N;
		h = rect->h / M;
	}
	else
	{
		w = N;
		h = M;

		N = rect->w / N;
		M = rect->h / M;
	}

	tab->N = N;
	tab->NM = N*M;

	tab->rect.x = rect->x;
	tab->rect.y = rect->y;
	tab->rect.w = w;
	tab->rect.h = h;

	return tab;
}

D2TK_API bool
d2tk_table_not_end(d2tk_table_t *tab)
{
	return tab && (tab->k < tab->NM);
}

D2TK_API d2tk_table_t *
d2tk_table_next(d2tk_table_t *tab)
{
	++tab->k;

	if(++tab->x % tab->N)
	{
		tab->rect.x += tab->rect.w;
	}
	else // overflow
	{
		tab->x = 0;
		++tab->y;

		tab->rect.x = tab->x0;
		tab->rect.y += tab->rect.h;
	}

	return tab;
}

D2TK_API unsigned
d2tk_table_get_index(d2tk_table_t *tab)
{
	return tab->k;
}

D2TK_API unsigned
d2tk_table_get_index_x(d2tk_table_t *tab)
{
	return tab->x;
}

D2TK_API unsigned
d2tk_table_get_index_y(d2tk_table_t *tab)
{
	return tab->y;
}

D2TK_API const d2tk_rect_t *
d2tk_table_get_rect(d2tk_table_t *tab)
{
	return &tab->rect;
}
