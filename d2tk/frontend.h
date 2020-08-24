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

#ifndef _D2TK_FRONTEND_H
#define _D2TK_FRONTEND_H

#include <signal.h>

#include <d2tk/base.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*d2tk_frontend_expose_t)(void *data, d2tk_coord_t w, d2tk_coord_t h);
typedef struct _d2tk_frontend_t d2tk_frontend_t;

D2TK_API void
d2tk_frontend_free(d2tk_frontend_t *dpugl);

D2TK_API int
d2tk_frontend_step(d2tk_frontend_t *dpugl);

D2TK_API int
d2tk_frontend_poll(d2tk_frontend_t *dpugl, double timeout);

D2TK_API int
d2tk_frontend_get_file_descriptor(d2tk_frontend_t *dpugl);

D2TK_API void
d2tk_frontend_run(d2tk_frontend_t *dpugl, const sig_atomic_t *done);

D2TK_API void
d2tk_frontend_redisplay(d2tk_frontend_t *dpugl);

D2TK_API int
d2tk_frontend_set_size(d2tk_frontend_t *dpugl, d2tk_coord_t w, d2tk_coord_t h);

D2TK_API int
d2tk_frontend_get_size(d2tk_frontend_t *dpugl, d2tk_coord_t *w, d2tk_coord_t *h);

D2TK_API d2tk_base_t *
d2tk_frontend_get_base(d2tk_frontend_t *dpugl);

D2TK_API float
d2tk_frontend_get_scale();

#ifdef __cplusplus
}
#endif

#endif // _D2TK_FRONTEND_H
