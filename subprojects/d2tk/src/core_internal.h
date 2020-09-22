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

#ifndef _D2TK_CORE_INTERNAL_H
#define _D2TK_CORE_INTERNAL_H

#include <d2tk/core.h>

#ifdef __cplusplus
extern "C" {
#endif

#define D2TK_PAD_SIZE(size) ( ((size) + 7U) & (~7U) )

typedef struct _d2tk_clip_t d2tk_clip_t;
typedef struct _d2tk_com_t d2tk_com_t;

typedef void *(*d2tk_core_new_t)(const char *bundle_path);
typedef void (*d2tk_core_free_t)(void *data);
typedef int (*d2tk_core_context_t)(void *data, void *pctx);
typedef void (*d2tk_core_pre_t)(void *data, d2tk_core_t *core,
	d2tk_coord_t w, d2tk_coord_t h, unsigned pass);
typedef bool (*d2tk_core_post_t)(void *data, d2tk_core_t *core,
	d2tk_coord_t w, d2tk_coord_t h, unsigned pass);
typedef void (*d2tk_core_end_t)(void *data, d2tk_core_t *core,
	d2tk_coord_t w, d2tk_coord_t h);
typedef void (*d2tk_core_process_t)(void *data, d2tk_core_t *core,
	const d2tk_com_t *com, d2tk_coord_t xo, d2tk_coord_t yo,
	const d2tk_clip_t *clip, unsigned pass);
typedef void (*d2tk_core_sprite_free_t)(void *data, uint8_t type, uintptr_t body);

typedef struct _d2tk_body_move_to_t d2tk_body_move_to_t;
typedef struct _d2tk_body_line_to_t d2tk_body_line_to_t;
typedef struct _d2tk_body_rect_t d2tk_body_rect_t;
typedef struct _d2tk_body_scissor_t d2tk_body_scissor_t;
typedef struct _d2tk_body_rounded_rect_t d2tk_body_rounded_rect_t;
typedef struct _d2tk_body_arc_t d2tk_body_arc_t;
typedef struct _d2tk_body_curve_to_t d2tk_body_curve_to_t;
typedef struct _d2tk_body_color_t d2tk_body_color_t;
typedef struct _d2tk_body_linear_gradient_t d2tk_body_linear_gradient_t;
typedef struct _d2tk_body_rotate_t d2tk_body_rotate_t;
typedef struct _d2tk_body_font_face_t d2tk_body_font_face_t;
typedef struct _d2tk_body_font_size_t d2tk_body_font_size_t;
typedef struct _d2tk_body_text_t d2tk_body_text_t;
typedef struct _d2tk_body_image_t d2tk_body_image_t;
typedef struct _d2tk_body_bitmap_surf_t d2tk_body_bitmap_surf_t;
typedef struct _d2tk_body_bitmap_t d2tk_body_bitmap_t;
typedef struct _d2tk_body_custom_t d2tk_body_custom_t;
typedef struct _d2tk_body_stroke_width_t d2tk_body_stroke_width_t;
typedef struct _d2tk_body_bbox_t d2tk_body_bbox_t;
typedef union _d2tk_body_t d2tk_body_t;

struct _d2tk_clip_t {
	d2tk_coord_t x0;
	d2tk_coord_t y0;
	d2tk_coord_t x1;
	d2tk_coord_t y1;
	d2tk_coord_t w;
	d2tk_coord_t h;
};

struct _d2tk_core_driver_t {
	d2tk_core_new_t new;
	d2tk_core_free_t free;
	d2tk_core_context_t context;
	d2tk_core_pre_t pre;
	d2tk_core_process_t process;
	d2tk_core_post_t post;
	d2tk_core_end_t end;
	d2tk_core_sprite_free_t sprite_free;
};

struct _d2tk_body_move_to_t {
	d2tk_coord_t x;
	d2tk_coord_t y;
};

struct _d2tk_body_line_to_t {
	d2tk_coord_t x;
	d2tk_coord_t y;
};

struct _d2tk_body_rect_t {
	d2tk_coord_t x;
	d2tk_coord_t y;
	d2tk_coord_t w;
	d2tk_coord_t h;
};

struct _d2tk_body_scissor_t {
	d2tk_coord_t x;
	d2tk_coord_t y;
	d2tk_coord_t w;
	d2tk_coord_t h;
};

struct _d2tk_body_rounded_rect_t {
	d2tk_coord_t x;
	d2tk_coord_t y;
	d2tk_coord_t w;
	d2tk_coord_t h;
	d2tk_coord_t r;
};

struct _d2tk_body_arc_t {
	d2tk_coord_t x;
	d2tk_coord_t y;
	d2tk_coord_t r;
	d2tk_coord_t a;
	d2tk_coord_t b;
	bool cw;
};

struct _d2tk_body_curve_to_t {
	d2tk_coord_t x1;
	d2tk_coord_t y1;
	d2tk_coord_t x2;
	d2tk_coord_t y2;
	d2tk_coord_t x3;
	d2tk_coord_t y3;
};

struct _d2tk_body_color_t {
	uint32_t rgba;
};

struct _d2tk_body_linear_gradient_t {
	d2tk_point_t p [2];
	uint32_t rgba [2];
};

struct _d2tk_body_rotate_t {
	d2tk_coord_t deg;
};

struct _d2tk_body_font_face_t {
	char face [1]; // at least zero-terminator
};

struct _d2tk_body_font_size_t {
	d2tk_coord_t size;
};

struct _d2tk_body_text_t {
	d2tk_coord_t x;
	d2tk_coord_t y;
	d2tk_coord_t w;
	d2tk_coord_t h;
	d2tk_align_t align;
	char text [1]; // at least zero-terminator
};

struct _d2tk_body_image_t {
	d2tk_coord_t x;
	d2tk_coord_t y;
	d2tk_coord_t w;
	d2tk_coord_t h;
	d2tk_align_t align;
	char path [1]; // at least zero-terminator
};

struct _d2tk_body_bitmap_surf_t {
	uint32_t w;
	uint32_t h;
	uint32_t stride;
	const uint32_t *argb;
	uint64_t rev;
};

struct _d2tk_body_bitmap_t {
	d2tk_coord_t x;
	d2tk_coord_t y;
	d2tk_coord_t w;
	d2tk_coord_t h;
	d2tk_align_t align;
	d2tk_body_bitmap_surf_t surf;
};

struct _d2tk_body_custom_t {
	d2tk_coord_t x;
	d2tk_coord_t y;
	d2tk_coord_t w;
	d2tk_coord_t h;
	uint32_t size;
	const void *data;
	d2tk_core_custom_t custom;
};

struct _d2tk_body_stroke_width_t {
	d2tk_coord_t width;
};

struct _d2tk_body_bbox_t {
	bool dirty;
	bool cached;
	bool container;
	uint32_t hash;
	d2tk_clip_t clip;
};

union _d2tk_body_t {
	d2tk_body_move_to_t move_to;
	d2tk_body_line_to_t line_to;
	d2tk_body_rect_t rect;
	d2tk_body_rounded_rect_t rounded_rect;
	d2tk_body_arc_t arc;
	d2tk_body_curve_to_t curve_to;
	d2tk_body_color_t color;
	d2tk_body_linear_gradient_t linear_gradient;
	d2tk_body_rotate_t rotate;
	d2tk_body_scissor_t scissor;
	d2tk_body_font_face_t font_face;
	d2tk_body_font_size_t font_size;
	d2tk_body_text_t text;
	d2tk_body_image_t image;
	d2tk_body_custom_t custom;
	d2tk_body_bitmap_t bitmap;
	d2tk_body_stroke_width_t stroke_width;
	d2tk_body_bbox_t bbox;
};

typedef enum _d2tk_instr_t {
	D2TK_INSTR_LINE_TO,
	D2TK_INSTR_MOVE_TO,
	D2TK_INSTR_RECT,
	D2TK_INSTR_ROUNDED_RECT,
	D2TK_INSTR_ARC,
	D2TK_INSTR_CURVE_TO,
	D2TK_INSTR_COLOR,
	D2TK_INSTR_LINEAR_GRADIENT,
	D2TK_INSTR_ROTATE,
	D2TK_INSTR_STROKE,
	D2TK_INSTR_FILL,
	D2TK_INSTR_SAVE,
	D2TK_INSTR_RESTORE,
	D2TK_INSTR_BBOX,
	D2TK_INSTR_BEGIN_PATH,
	D2TK_INSTR_CLOSE_PATH,
	D2TK_INSTR_SCISSOR,
	D2TK_INSTR_RESET_SCISSOR,
	D2TK_INSTR_FONT_SIZE,
	D2TK_INSTR_FONT_FACE,
	D2TK_INSTR_TEXT,
	D2TK_INSTR_IMAGE,
	D2TK_INSTR_BITMAP,
	D2TK_INSTR_CUSTOM,
	D2TK_INSTR_STROKE_WIDTH
} d2tk_instr_t;

struct _d2tk_com_t {
	uint32_t size;
	uint32_t instr;
	d2tk_body_t body [] __attribute__((aligned(8)));
};

uintptr_t *
d2tk_core_get_sprite(d2tk_core_t *core, uint64_t hash, uint8_t type);

const d2tk_com_t *
d2tk_com_begin_const(const d2tk_com_t *com);

const d2tk_com_t *
d2tk_com_get_end_const(const d2tk_com_t *com);

bool
d2tk_com_not_end_const(const d2tk_com_t *end, const d2tk_com_t *bbox);

const d2tk_com_t *
d2tk_com_next_const(const d2tk_com_t *bbox);

#define D2TK_COM_FOREACH_CONST(COM, BBOX) \
	for(const d2tk_com_t *(BBOX) = d2tk_com_begin_const((COM)), \
			*__end = d2tk_com_get_end_const((COM)); \
		d2tk_com_not_end_const(__end, (BBOX)); \
		(BBOX) = d2tk_com_next_const((BBOX)))

uint32_t *
d2tk_core_get_pixels(d2tk_core_t *core, d2tk_rect_t *rect);

void
d2tk_core_set_bg_color(d2tk_core_t *core, uint32_t rgba);

uint32_t
d2tk_core_get_bg_color(d2tk_core_t *core);

int
d2tk_core_get_font_path(d2tk_core_t *core, const char *bundle_path,
	const char *rel_path, size_t abs_len, char *abs_path);

#ifdef __cplusplus
}
#endif

#endif // _D2TK_CORE_INTERNAL_H
