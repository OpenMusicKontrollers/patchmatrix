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

#ifndef _D2TK_CORE_H
#define _D2TK_CORE_H

#include <stdint.h>
#include <stdbool.h>

#include "config.h"
#include <d2tk/d2tk.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t d2tk_coord_t;
typedef struct _d2tk_rect_t d2tk_rect_t;
typedef struct _d2tk_widget_t d2tk_widget_t;
typedef struct _d2tk_point_t d2tk_point_t;
typedef struct _d2tk_core_t d2tk_core_t;
typedef struct _d2tk_core_driver_t d2tk_core_driver_t;
typedef void (*d2tk_core_custom_t)(void *ctx, uint32_t size, const void *data);

typedef enum _d2tk_align_t {
	D2TK_ALIGN_NONE 				= 0,
	D2TK_ALIGN_LEFT					= (1 << 0),
	D2TK_ALIGN_CENTER				= (1 << 1),
	D2TK_ALIGN_RIGHT				= (1 << 2),
	D2TK_ALIGN_TOP					= (1 << 3),
	D2TK_ALIGN_MIDDLE				= (1 << 4),
	D2TK_ALIGN_BOTTOM				= (1 << 5)
} d2tk_align_t;

#define D2TK_ALIGN_CENTERED (D2TK_ALIGN_CENTER | D2TK_ALIGN_MIDDLE)

struct _d2tk_rect_t {
	d2tk_coord_t x;
	d2tk_coord_t y;
	d2tk_coord_t w;
	d2tk_coord_t h;
};

struct _d2tk_point_t {
	d2tk_coord_t x;
	d2tk_coord_t y;
};

#define D2TK_RECT(X, Y, W, H) \
	((d2tk_rect_t){ .x = (X), .y = (Y), .w = (W), .h = (H) })

#define D2TK_POINT(X, Y) \
	((d2tk_point_t){ .x = (X), .y = (Y) })

extern const size_t d2tk_widget_sz;

D2TK_API void
d2tk_rect_shrink_x(d2tk_rect_t *dst, const d2tk_rect_t *src,
	d2tk_coord_t brd);

D2TK_API void
d2tk_rect_shrink_y(d2tk_rect_t *dst, const d2tk_rect_t *src,
	d2tk_coord_t brd);

D2TK_API void
d2tk_rect_shrink(d2tk_rect_t *dst, const d2tk_rect_t *src,
	d2tk_coord_t brd);

D2TK_API void
d2tk_core_move_to(d2tk_core_t *core, d2tk_coord_t x, d2tk_coord_t y);

D2TK_API void
d2tk_core_line_to(d2tk_core_t *core, d2tk_coord_t x, d2tk_coord_t y);

D2TK_API void
d2tk_core_rect(d2tk_core_t *core, const d2tk_rect_t *rect);

D2TK_API void
d2tk_core_rounded_rect(d2tk_core_t *core, const d2tk_rect_t *rect,
	d2tk_coord_t r);

D2TK_API void
d2tk_core_arc(d2tk_core_t *core, d2tk_coord_t x, d2tk_coord_t y, d2tk_coord_t r,
	d2tk_coord_t a, d2tk_coord_t b, bool cw);

D2TK_API void
d2tk_core_curve_to(d2tk_core_t *core, d2tk_coord_t x1, d2tk_coord_t y1,
	d2tk_coord_t x2, d2tk_coord_t y2, d2tk_coord_t x3, d2tk_coord_t y3);

D2TK_API void
d2tk_core_color(d2tk_core_t *core, uint32_t rgba);

D2TK_API void
d2tk_core_linear_gradient(d2tk_core_t *core, const d2tk_point_t point [2],
	const uint32_t rgba [2]);

D2TK_API void
d2tk_core_stroke(d2tk_core_t *core);

D2TK_API void
d2tk_core_fill(d2tk_core_t *core);

D2TK_API void
d2tk_core_save(d2tk_core_t *core);

D2TK_API void
d2tk_core_restore(d2tk_core_t *core);

D2TK_API void
d2tk_core_rotate(d2tk_core_t *core, d2tk_coord_t deg);

D2TK_API d2tk_widget_t *
d2tk_core_widget_begin(d2tk_core_t *core, uint64_t hash, d2tk_widget_t *widget);

D2TK_API bool
d2tk_core_widget_not_end(d2tk_core_t *core, d2tk_widget_t *widget);

D2TK_API d2tk_widget_t *
d2tk_core_widget_next(d2tk_core_t *core, d2tk_widget_t *widget);

#define D2TK_CORE_WIDGET(CORE, HASH, WIDGET) \
	for(d2tk_widget_t *(WIDGET) = d2tk_core_widget_begin((CORE), (HASH), \
			alloca(d2tk_widget_sz)); \
		d2tk_core_widget_not_end((CORE), (WIDGET)); \
		(WIDGET) = d2tk_core_widget_next((CORE), (WIDGET)))

D2TK_API ssize_t
d2tk_core_bbox_push(d2tk_core_t *core, bool cached, const d2tk_rect_t *rect);

D2TK_API ssize_t
d2tk_core_bbox_container_push(d2tk_core_t *core, bool cached,
	const d2tk_rect_t *rect);

D2TK_API void
d2tk_core_bbox_pop(d2tk_core_t *core, ssize_t ref);

D2TK_API void
d2tk_core_begin_path(d2tk_core_t *core);

D2TK_API void
d2tk_core_close_path(d2tk_core_t *core);

D2TK_API void
d2tk_core_scissor(d2tk_core_t *core, const d2tk_rect_t *rect);

D2TK_API void
d2tk_core_reset_scissor(d2tk_core_t *core);

D2TK_API void
d2tk_core_font_size(d2tk_core_t *core, d2tk_coord_t size);

D2TK_API void
d2tk_core_font_face(d2tk_core_t *core, size_t sz, const char *face);

D2TK_API void
d2tk_core_text(d2tk_core_t *core, const d2tk_rect_t *rect, size_t sz,
	const char *text, d2tk_align_t align);

D2TK_API void
d2tk_core_image(d2tk_core_t *core, const d2tk_rect_t *rect, size_t sz,
	const char *path, d2tk_align_t align);

D2TK_API void
d2tk_core_bitmap(d2tk_core_t *core, const d2tk_rect_t *rect, uint32_t w,
	uint32_t h, uint32_t stride, const uint32_t *argb, uint64_t rev,
	d2tk_align_t align);

D2TK_API void
d2tk_core_custom(d2tk_core_t *core, const d2tk_rect_t *rect, uint32_t size,
	const void *data, d2tk_core_custom_t custom);

D2TK_API void
d2tk_core_stroke_width(d2tk_core_t *core, d2tk_coord_t width);

D2TK_API int
d2tk_core_pre(d2tk_core_t *core, void *pctx);

D2TK_API void
d2tk_core_post(d2tk_core_t *core);

D2TK_API d2tk_core_t *
d2tk_core_new(const d2tk_core_driver_t *driver, void *data);

D2TK_API void
d2tk_core_set_ttls(d2tk_core_t *core, uint32_t sprites, uint32_t memcaches);

D2TK_API void
d2tk_core_free(d2tk_core_t *core);

D2TK_API void
d2tk_core_set_dimensions(d2tk_core_t *core, d2tk_coord_t w, d2tk_coord_t h);

D2TK_API void
d2tk_core_get_dimensions(d2tk_core_t *core, d2tk_coord_t *w, d2tk_coord_t *h);

D2TK_API void
d2tk_core_set_full_refresh(d2tk_core_t *core);

#ifdef __cplusplus
}
#endif

#endif // _D2TK_CORE_H
