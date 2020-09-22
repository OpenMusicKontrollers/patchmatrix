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

D2TK_API void
d2tk_base_image(d2tk_base_t *base, ssize_t path_len, const char *path,
	const d2tk_rect_t *rect, d2tk_align_t align)
{
	const bool has_img = path_len && path;

	if(has_img && (path_len == -1) ) // zero-terminated string
	{
		path_len = strlen(path);
	}

	const d2tk_hash_dict_t dict [] = {
		{ rect, sizeof(d2tk_rect_t) },
		{ (path ? path : NULL), (path ? path_len : 0) },
		{ NULL, 0 }
	};
	const uint64_t hash = d2tk_hash_dict(dict);

	d2tk_core_t *core = base->core;;

	D2TK_CORE_WIDGET(core, hash, widget)
	{
		if(has_img)
		{
			const size_t ref = d2tk_core_bbox_push(core, true, rect);

			d2tk_core_image(core, rect, path_len, path, align);

			d2tk_core_bbox_pop(core, ref);
		}
	}
}
