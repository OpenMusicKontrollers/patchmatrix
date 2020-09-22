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

#ifndef _D2TK_FRONTEND_PUGL_H
#define _D2TK_FRONTEND_PUGL_H

#include <signal.h>

#include <d2tk/base.h>
#include <d2tk/frontend.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _d2tk_pugl_config_t d2tk_pugl_config_t;

struct _d2tk_pugl_config_t {
	uintptr_t parent;
	const char *bundle_path;
	d2tk_coord_t min_w;
	d2tk_coord_t min_h;
	d2tk_coord_t w;
	d2tk_coord_t h;
	bool fixed_size;
	bool fixed_aspect;
	d2tk_frontend_expose_t expose;
	void *data;
};

D2TK_API d2tk_frontend_t *
d2tk_pugl_new(const d2tk_pugl_config_t *config, uintptr_t *widget);

#ifdef __cplusplus
}
#endif

#endif // _D2TK_FRONTEND_PUGL_H
