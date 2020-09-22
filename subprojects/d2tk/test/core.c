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
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <d2tk/core.h>
#include <d2tk/hash.h>
#include "mock.h"

static void
_test_hash()
{
	const uint32_t bar = 12;
	const char *foo = "barbarbarbar";

	const uint64_t hash1 = d2tk_hash(&bar, sizeof(bar));
	const uint64_t hash2 = d2tk_hash(foo, strlen(foo));

	assert(hash1 != hash2);
}

static void
_test_hash_foreach()
{
	const uint32_t bar = 12;
	const char *foo = "foofoofoofoo";

	const d2tk_hash_dict_t dict [] = {
		{ foo, strlen(foo) },
		{ &bar, sizeof(bar) },
		{ NULL, 0 }
	};

	const uint64_t hash1 = d2tk_hash_dict(dict);

	const uint64_t hash2 = d2tk_hash_foreach(foo, strlen(foo),
		&bar, sizeof(bar),
		NULL);

	assert(hash1 == hash2);
}

static void
_test_rect_shrink()
{
	d2tk_rect_t dst;

	// initialization
	const d2tk_rect_t src = D2TK_RECT(1, 2, 3, 4);
	assert(src.x == 1);
	assert(src.y == 2);
	assert(src.w == 3);
	assert(src.h == 4);

	// shrink on x-axis
	memset(&dst, 0x0, sizeof(dst));
	d2tk_rect_shrink_x(&dst, &src, 1);
	assert(dst.x == 1 + 1);
	assert(dst.y == 2);
	assert(dst.w == 3 - 2*1);
	assert(dst.h == 4);

	// shrink on y-axis
	memset(&dst, 0x0, sizeof(dst));
	d2tk_rect_shrink_y(&dst, &src, 1);
	assert(dst.x == 1);
	assert(dst.y == 2 + 1);
	assert(dst.w == 3);
	assert(dst.h == 4 - 2*1);

	// shrink on x/y-axes
	memset(&dst, 0x0, sizeof(dst));
	d2tk_rect_shrink(&dst, &src, 1);
	assert(dst.x == 1 + 1);
	assert(dst.y == 2 + 1);
	assert(dst.w == 3 - 2*1);
	assert(dst.h == 4 - 2*1);
}

static void
_test_point()
{
	// initialization
	const d2tk_point_t src = D2TK_POINT(11, 22);
	assert(src.x == 11);
	assert(src.y == 22);
}

static void
_test_dimensions()
{
	d2tk_coord_t w;
	d2tk_coord_t h;

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, NULL);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	w = 0;
	h = 0;
	d2tk_core_get_dimensions(core, &w, &h);
	assert(w == DIM_W);
	assert(h == DIM_H);

	w = 0;
	h = 0;
	d2tk_core_get_dimensions(core, NULL, NULL);
	assert(w == 0);
	assert(h == 0);

	d2tk_core_free(core);
}

#define BG_COLOR 0x222222ff

static void
_test_bg_color()
{
	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, NULL);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	d2tk_core_set_bg_color(core, BG_COLOR);
	assert(BG_COLOR == d2tk_core_get_bg_color(core));

	d2tk_core_free(core);
}

#undef BG_COLOR

#define MOVE_TO_X 10
#define MOVE_TO_Y 20

static void
_check_move_to(const d2tk_com_t *com, const d2tk_clip_t *clip)
{
	assert(clip->x0 == CLIP_X);
	assert(clip->y0 == CLIP_Y);
	assert(clip->x1 == CLIP_X + CLIP_W);
	assert(clip->y1 == CLIP_Y + CLIP_H);
	assert(clip->w == CLIP_W);
	assert(clip->h == CLIP_H);

	assert(com->size == sizeof(d2tk_body_move_to_t));
	assert(com->instr == D2TK_INSTR_MOVE_TO);
	assert(com->body->move_to.x == MOVE_TO_X - CLIP_X);
	assert(com->body->move_to.y == MOVE_TO_Y - CLIP_Y);
}

static void
_test_move_to()
{
	d2tk_mock_ctx_t ctx = {
		.check = _check_move_to
	};

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, &ctx);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	d2tk_core_pre(core, NULL);
	const ssize_t ref = d2tk_core_bbox_push(core, true,
		&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
	assert(ref >= 0);

	d2tk_core_move_to(core, MOVE_TO_X, MOVE_TO_Y);

	d2tk_core_bbox_pop(core, ref);
	d2tk_core_post(core);

	// trigger garbage collector
	for(unsigned i = 0; i < 0x400; i++)
	{
		d2tk_core_pre(core, NULL);
		d2tk_core_post(core);
	}

	d2tk_core_free(core);
}

#undef MOVE_TO_X
#undef MOVE_TO_Y

#define LINE_TO_X 10
#define LINE_TO_Y 20

static void
_check_line_to(const d2tk_com_t *com, const d2tk_clip_t *clip)
{
	assert(clip->x0 == CLIP_X);
	assert(clip->y0 == CLIP_Y);
	assert(clip->x1 == CLIP_X + CLIP_W);
	assert(clip->y1 == CLIP_Y + CLIP_H);
	assert(clip->w == CLIP_W);
	assert(clip->h == CLIP_H);

	assert(com->size == sizeof(d2tk_body_line_to_t));
	assert(com->instr == D2TK_INSTR_LINE_TO);
	assert(com->body->line_to.x == LINE_TO_X - CLIP_X);
	assert(com->body->line_to.y == LINE_TO_Y - CLIP_Y);
}

static void
_test_line_to()
{
	d2tk_mock_ctx_t ctx = {
		.check = _check_line_to
	};

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, &ctx);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	d2tk_core_pre(core, NULL);
	const ssize_t ref = d2tk_core_bbox_push(core, true,
		&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
	assert(ref >= 0);

	d2tk_core_line_to(core, LINE_TO_X, LINE_TO_Y);

	d2tk_core_bbox_pop(core, ref);
	d2tk_core_post(core);
	d2tk_core_free(core);
}

#undef LINE_TO_X
#undef LINE_TO_Y

#define RECT_X 10
#define RECT_Y 20
#define RECT_W 30
#define RECT_H 40

static void
_check_rect(const d2tk_com_t *com, const d2tk_clip_t *clip)
{
	assert(clip->x0 == CLIP_X);
	assert(clip->y0 == CLIP_Y);
	assert(clip->x1 == CLIP_X + CLIP_W);
	assert(clip->y1 == CLIP_Y + CLIP_H);
	assert(clip->w == CLIP_W);
	assert(clip->h == CLIP_H);

	assert(com->size == sizeof(d2tk_body_rect_t));
	assert(com->instr == D2TK_INSTR_RECT);
	assert(com->body->rect.x == RECT_X - CLIP_X);
	assert(com->body->rect.y == RECT_Y - CLIP_Y);
	assert(com->body->rect.w == RECT_W);
	assert(com->body->rect.h == RECT_H);
}

static void
_test_rect()
{
	d2tk_mock_ctx_t ctx = {
		.check = _check_rect
	};

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, &ctx);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	d2tk_core_pre(core, NULL);
	const ssize_t ref = d2tk_core_bbox_push(core, true,
		&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
	assert(ref >= 0);

	d2tk_core_rect(core, &D2TK_RECT(RECT_X, RECT_Y, RECT_W, RECT_H));

	d2tk_core_bbox_pop(core, ref);
	d2tk_core_post(core);
	d2tk_core_free(core);
}

#undef RECT_X
#undef RECT_Y
#undef RECT_W
#undef RECT_H

#define ROUNDED_RECT_X 10
#define ROUNDED_RECT_Y 20
#define ROUNDED_RECT_W 30
#define ROUNDED_RECT_H 40
#define ROUNDED_RECT_R 5

static void
_check_rounded_rect(const d2tk_com_t *com, const d2tk_clip_t *clip)
{
	assert(clip->x0 == CLIP_X);
	assert(clip->y0 == CLIP_Y);
	assert(clip->x1 == CLIP_X + CLIP_W);
	assert(clip->y1 == CLIP_Y + CLIP_H);
	assert(clip->w == CLIP_W);
	assert(clip->h == CLIP_H);

	assert(com->size == sizeof(d2tk_body_rounded_rect_t));
	assert(com->instr == D2TK_INSTR_ROUNDED_RECT);
	assert(com->body->rounded_rect.x == ROUNDED_RECT_X - CLIP_X);
	assert(com->body->rounded_rect.y == ROUNDED_RECT_Y - CLIP_Y);
	assert(com->body->rounded_rect.w == ROUNDED_RECT_W);
	assert(com->body->rounded_rect.h == ROUNDED_RECT_H);
	assert(com->body->rounded_rect.r == ROUNDED_RECT_R);
}

static void
_test_rounded_rect()
{
	d2tk_mock_ctx_t ctx = {
		.check = _check_rounded_rect
	};

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, &ctx);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	d2tk_core_pre(core, NULL);
	const ssize_t ref = d2tk_core_bbox_push(core, true,
		&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
	assert(ref >= 0);

	d2tk_core_rounded_rect(core,
		&D2TK_RECT(ROUNDED_RECT_X, ROUNDED_RECT_Y, ROUNDED_RECT_W, ROUNDED_RECT_H),
		ROUNDED_RECT_R);

	d2tk_core_bbox_pop(core, ref);
	d2tk_core_post(core);
	d2tk_core_free(core);
}

#undef ROUNDED_RECT_X
#undef ROUNDED_RECT_Y
#undef ROUNDED_RECT_W
#undef ROUNDED_RECT_H
#undef ROUNDED_RECT_R

#define ARC_X 10
#define ARC_Y 20
#define ARC_R 5
#define ARC_A 30
#define ARC_B 60
#define ARC_CW true

static void
_check_arc(const d2tk_com_t *com, const d2tk_clip_t *clip)
{
	assert(clip->x0 == CLIP_X);
	assert(clip->y0 == CLIP_Y);
	assert(clip->x1 == CLIP_X + CLIP_W);
	assert(clip->y1 == CLIP_Y + CLIP_H);
	assert(clip->w == CLIP_W);
	assert(clip->h == CLIP_H);

	assert(com->size == sizeof(d2tk_body_arc_t));
	assert(com->instr == D2TK_INSTR_ARC);
	assert(com->body->arc.x == ARC_X - CLIP_X);
	assert(com->body->arc.y == ARC_Y - CLIP_Y);
	assert(com->body->arc.r == ARC_R);
	assert(com->body->arc.a == ARC_A);
	assert(com->body->arc.b == ARC_B);
	assert(com->body->arc.cw == ARC_CW);
}

static void
_test_arc()
{
	d2tk_mock_ctx_t ctx = {
		.check = _check_arc
	};

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, &ctx);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	d2tk_core_pre(core, NULL);
	const ssize_t ref = d2tk_core_bbox_push(core, true,
		&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
	assert(ref >= 0);

	d2tk_core_arc(core, ARC_X, ARC_Y, ARC_R, ARC_A, ARC_B, ARC_CW);

	d2tk_core_bbox_pop(core, ref);
	d2tk_core_post(core);
	d2tk_core_free(core);
}

#undef ARC_X
#undef ARC_Y
#undef ARC_R
#undef ARC_A
#undef ARC_B
#undef ARC_CW

#define CURVE_TO_X1 10
#define CURVE_TO_Y1 10
#define CURVE_TO_X2 20
#define CURVE_TO_Y2 20
#define CURVE_TO_X3 30
#define CURVE_TO_Y3 30

static void
_check_curve_to(const d2tk_com_t *com, const d2tk_clip_t *clip)
{
	assert(clip->x0 == CLIP_X);
	assert(clip->y0 == CLIP_Y);
	assert(clip->x1 == CLIP_X + CLIP_W);
	assert(clip->y1 == CLIP_Y + CLIP_H);
	assert(clip->w == CLIP_W);
	assert(clip->h == CLIP_H);

	assert(com->size == sizeof(d2tk_body_curve_to_t));
	assert(com->instr == D2TK_INSTR_CURVE_TO);
	assert(com->body->curve_to.x1 == CURVE_TO_X1 - CLIP_X);
	assert(com->body->curve_to.y1 == CURVE_TO_Y1 - CLIP_Y);
	assert(com->body->curve_to.x2 == CURVE_TO_X2 - CLIP_X);
	assert(com->body->curve_to.y2 == CURVE_TO_Y2 - CLIP_Y);
	assert(com->body->curve_to.x3 == CURVE_TO_X3 - CLIP_X);
	assert(com->body->curve_to.y3 == CURVE_TO_Y3 - CLIP_Y);
}

static void
_test_curve_to()
{
	d2tk_mock_ctx_t ctx = {
		.check = _check_curve_to
	};

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, &ctx);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	d2tk_core_pre(core, NULL);
	const ssize_t ref = d2tk_core_bbox_push(core, true,
		&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
	assert(ref >= 0);

	d2tk_core_curve_to(core, CURVE_TO_X1, CURVE_TO_Y1, CURVE_TO_X2, CURVE_TO_Y2,
		CURVE_TO_X3, CURVE_TO_Y3);

	d2tk_core_bbox_pop(core, ref);
	d2tk_core_post(core);
	d2tk_core_free(core);
}

#undef CURVE_TO_X1
#undef CURVE_TO_Y1
#undef CURVE_TO_X2
#undef CURVE_TO_Y2
#undef CURVE_TO_X3
#undef CURVE_TO_Y3

#define COLOR_RGBA 0xff00ff00

static void
_check_color(const d2tk_com_t *com, const d2tk_clip_t *clip)
{
	assert(clip->x0 == CLIP_X);
	assert(clip->y0 == CLIP_Y);
	assert(clip->x1 == CLIP_X + CLIP_W);
	assert(clip->y1 == CLIP_Y + CLIP_H);
	assert(clip->w == CLIP_W);
	assert(clip->h == CLIP_H);

	assert(com->size == sizeof(d2tk_body_color_t));
	assert(com->instr == D2TK_INSTR_COLOR);
	assert(com->body->color.rgba == COLOR_RGBA);
}

static void
_test_color()
{
	d2tk_mock_ctx_t ctx = {
		.check = _check_color
	};

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, &ctx);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	d2tk_core_pre(core, NULL);
	const ssize_t ref = d2tk_core_bbox_push(core, true,
		&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
	assert(ref >= 0);

	d2tk_core_color(core, COLOR_RGBA);

	d2tk_core_bbox_pop(core, ref);
	d2tk_core_post(core);
	d2tk_core_free(core);
}

#undef COLOR_RGBA

#define LINEAR_GRADIENT_X_0 10
#define LINEAR_GRADIENT_Y_0 20
#define LINEAR_GRADIENT_X_1 30
#define LINEAR_GRADIENT_Y_1 40
#define LINEAR_GRADIENT_RGBA_0 0xff00ff00
#define LINEAR_GRADIENT_RGBA_1 0xffff00ff

static void
_check_linear_gradient(const d2tk_com_t *com, const d2tk_clip_t *clip)
{
	assert(clip->x0 == CLIP_X);
	assert(clip->y0 == CLIP_Y);
	assert(clip->x1 == CLIP_X + CLIP_W);
	assert(clip->y1 == CLIP_Y + CLIP_H);
	assert(clip->w == CLIP_W);
	assert(clip->h == CLIP_H);

	assert(com->size == sizeof(d2tk_body_linear_gradient_t));
	assert(com->instr == D2TK_INSTR_LINEAR_GRADIENT);
	assert(com->body->linear_gradient.p[0].x == LINEAR_GRADIENT_X_0 - CLIP_X);
	assert(com->body->linear_gradient.p[0].y == LINEAR_GRADIENT_Y_0 - CLIP_Y);
	assert(com->body->linear_gradient.p[1].x == LINEAR_GRADIENT_X_1 - CLIP_X);
	assert(com->body->linear_gradient.p[1].y == LINEAR_GRADIENT_Y_1 - CLIP_Y);
	assert(com->body->linear_gradient.rgba[0] == LINEAR_GRADIENT_RGBA_0);
	assert(com->body->linear_gradient.rgba[1] == LINEAR_GRADIENT_RGBA_1);
}

static void
_test_linear_gradient()
{
	d2tk_mock_ctx_t ctx = {
		.check = _check_linear_gradient
	};

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, &ctx);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	d2tk_core_pre(core, NULL);
	const ssize_t ref = d2tk_core_bbox_push(core, true,
		&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
	assert(ref >= 0);

	const d2tk_point_t p [2] = {
		D2TK_POINT(LINEAR_GRADIENT_X_0, LINEAR_GRADIENT_Y_0),
		D2TK_POINT(LINEAR_GRADIENT_X_1, LINEAR_GRADIENT_Y_1)
	};
	const uint32_t rgba [2] = {
		LINEAR_GRADIENT_RGBA_0,
		LINEAR_GRADIENT_RGBA_1
	};
	d2tk_core_linear_gradient(core, p, rgba);

	d2tk_core_bbox_pop(core, ref);
	d2tk_core_post(core);
	d2tk_core_free(core);
}

#undef LINEAR_GRADIENT_X_0
#undef LINEAR_GRADIENT_Y_0
#undef LINEAR_GRADIENT_X_1
#undef LINEAR_GRADIENT_Y_1
#undef LINEAR_GRADIENT_RGBA_0
#undef LINEAR_GRADIENT_RGBA_1

static void
_check_stroke(const d2tk_com_t *com, const d2tk_clip_t *clip)
{
	assert(clip->x0 == CLIP_X);
	assert(clip->y0 == CLIP_Y);
	assert(clip->x1 == CLIP_X + CLIP_W);
	assert(clip->y1 == CLIP_Y + CLIP_H);
	assert(clip->w == CLIP_W);
	assert(clip->h == CLIP_H);

	assert(com->size == 0);
	assert(com->instr == D2TK_INSTR_STROKE);
}

static void
_test_stroke()
{
	d2tk_mock_ctx_t ctx = {
		.check = _check_stroke
	};

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, &ctx);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	d2tk_core_pre(core, NULL);
	const ssize_t ref = d2tk_core_bbox_push(core, true,
		&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
	assert(ref >= 0);

	d2tk_core_stroke(core);

	d2tk_core_bbox_pop(core, ref);
	d2tk_core_post(core);
	d2tk_core_free(core);
}

static void
_check_fill(const d2tk_com_t *com, const d2tk_clip_t *clip)
{
	assert(clip->x0 == CLIP_X);
	assert(clip->y0 == CLIP_Y);
	assert(clip->x1 == CLIP_X + CLIP_W);
	assert(clip->y1 == CLIP_Y + CLIP_H);
	assert(clip->w == CLIP_W);
	assert(clip->h == CLIP_H);

	assert(com->size == 0);
	assert(com->instr == D2TK_INSTR_FILL);
}

static void
_test_fill()
{
	d2tk_mock_ctx_t ctx = {
		.check = _check_fill
	};

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, &ctx);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	d2tk_core_pre(core, NULL);
	const ssize_t ref = d2tk_core_bbox_push(core, true,
		&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
	assert(ref >= 0);

	d2tk_core_fill(core);

	d2tk_core_bbox_pop(core, ref);
	d2tk_core_post(core);
	d2tk_core_free(core);
}

static void
_check_save(const d2tk_com_t *com, const d2tk_clip_t *clip)
{
	assert(clip->x0 == CLIP_X);
	assert(clip->y0 == CLIP_Y);
	assert(clip->x1 == CLIP_X + CLIP_W);
	assert(clip->y1 == CLIP_Y + CLIP_H);
	assert(clip->w == CLIP_W);
	assert(clip->h == CLIP_H);

	assert(com->size == 0);
	assert(com->instr == D2TK_INSTR_SAVE);
}

static void
_test_save()
{
	d2tk_mock_ctx_t ctx = {
		.check = _check_save
	};

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, &ctx);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	d2tk_core_pre(core, NULL);
	const ssize_t ref = d2tk_core_bbox_push(core, true,
		&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
	assert(ref >= 0);

	d2tk_core_save(core);

	d2tk_core_bbox_pop(core, ref);
	d2tk_core_post(core);
	d2tk_core_free(core);
}

static void
_check_restore(const d2tk_com_t *com, const d2tk_clip_t *clip)
{
	assert(clip->x0 == CLIP_X);
	assert(clip->y0 == CLIP_Y);
	assert(clip->x1 == CLIP_X + CLIP_W);
	assert(clip->y1 == CLIP_Y + CLIP_H);
	assert(clip->w == CLIP_W);
	assert(clip->h == CLIP_H);

	assert(com->size == 0);
	assert(com->instr == D2TK_INSTR_RESTORE);
}

static void
_test_restore()
{
	d2tk_mock_ctx_t ctx = {
		.check = _check_restore
	};

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, &ctx);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	d2tk_core_pre(core, NULL);
	const ssize_t ref = d2tk_core_bbox_push(core, true,
		&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
	assert(ref >= 0);

	d2tk_core_restore(core);

	d2tk_core_bbox_pop(core, ref);
	d2tk_core_post(core);
	d2tk_core_free(core);
}

#define ROTATE_DEG 45

static void
_check_rotate(const d2tk_com_t *com, const d2tk_clip_t *clip)
{
	assert(clip->x0 == CLIP_X);
	assert(clip->y0 == CLIP_Y);
	assert(clip->x1 == CLIP_X + CLIP_W);
	assert(clip->y1 == CLIP_Y + CLIP_H);
	assert(clip->w == CLIP_W);
	assert(clip->h == CLIP_H);

	assert(com->size == sizeof(d2tk_body_rotate_t));
	assert(com->instr == D2TK_INSTR_ROTATE);
	assert(com->body->rotate.deg == ROTATE_DEG);
}

static void
_test_rotate()
{
	d2tk_mock_ctx_t ctx = {
		.check = _check_rotate
	};

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, &ctx);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	d2tk_core_pre(core, NULL);
	const ssize_t ref = d2tk_core_bbox_push(core, true,
		&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
	assert(ref >= 0);

	d2tk_core_rotate(core, ROTATE_DEG);

	d2tk_core_bbox_pop(core, ref);
	d2tk_core_post(core);
	d2tk_core_free(core);
}

#undef ROTATE_DEG

static void
_check_begin_path(const d2tk_com_t *com, const d2tk_clip_t *clip)
{
	assert(clip->x0 == CLIP_X);
	assert(clip->y0 == CLIP_Y);
	assert(clip->x1 == CLIP_X + CLIP_W);
	assert(clip->y1 == CLIP_Y + CLIP_H);
	assert(clip->w == CLIP_W);
	assert(clip->h == CLIP_H);

	assert(com->size == 0);
	assert(com->instr == D2TK_INSTR_BEGIN_PATH);
}

static void
_test_begin_path()
{
	d2tk_mock_ctx_t ctx = {
		.check = _check_begin_path
	};

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, &ctx);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	d2tk_core_pre(core, NULL);
	const ssize_t ref = d2tk_core_bbox_push(core, true,
		&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
	assert(ref >= 0);

	d2tk_core_begin_path(core);

	d2tk_core_bbox_pop(core, ref);
	d2tk_core_post(core);
	d2tk_core_free(core);
}

static void
_check_close_path(const d2tk_com_t *com, const d2tk_clip_t *clip)
{
	assert(clip->x0 == CLIP_X);
	assert(clip->y0 == CLIP_Y);
	assert(clip->x1 == CLIP_X + CLIP_W);
	assert(clip->y1 == CLIP_Y + CLIP_H);
	assert(clip->w == CLIP_W);
	assert(clip->h == CLIP_H);

	assert(com->size == 0);
	assert(com->instr == D2TK_INSTR_CLOSE_PATH);
}

static void
_test_close_path()
{
	d2tk_mock_ctx_t ctx = {
		.check = _check_close_path
	};

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, &ctx);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	d2tk_core_pre(core, NULL);
	const ssize_t ref = d2tk_core_bbox_push(core, true,
		&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
	assert(ref >= 0);

	d2tk_core_close_path(core);

	d2tk_core_bbox_pop(core, ref);
	d2tk_core_post(core);
	d2tk_core_free(core);
}

#define SCISSOR_X 10
#define SCISSOR_Y 20
#define SCISSOR_W 30
#define SCISSOR_H 40

static void
_check_scissor(const d2tk_com_t *com, const d2tk_clip_t *clip)
{
	assert(clip->x0 == CLIP_X);
	assert(clip->y0 == CLIP_Y);
	assert(clip->x1 == CLIP_X + CLIP_W);
	assert(clip->y1 == CLIP_Y + CLIP_H);
	assert(clip->w == CLIP_W);
	assert(clip->h == CLIP_H);

	assert(com->size == sizeof(d2tk_body_scissor_t));
	assert(com->instr == D2TK_INSTR_SCISSOR);
	assert(com->body->scissor.x == SCISSOR_X - CLIP_X);
	assert(com->body->scissor.y == SCISSOR_Y - CLIP_Y);
	assert(com->body->scissor.w == SCISSOR_W);
	assert(com->body->scissor.h == SCISSOR_H);
}

static void
_test_scissor()
{
	d2tk_mock_ctx_t ctx = {
		.check = _check_scissor
	};

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, &ctx);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	d2tk_core_pre(core, NULL);
	const ssize_t ref = d2tk_core_bbox_push(core, true,
		&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
	assert(ref >= 0);

	d2tk_core_scissor(core, &D2TK_RECT(SCISSOR_X, SCISSOR_Y, SCISSOR_W, SCISSOR_H));

	d2tk_core_bbox_pop(core, ref);
	d2tk_core_post(core);
	d2tk_core_free(core);
}

#undef SCISSOR_X
#undef SCISSOR_Y
#undef SCISSOR_W
#undef SCISSOR_H

static void
_check_reset_scissor(const d2tk_com_t *com, const d2tk_clip_t *clip)
{
	assert(clip->x0 == CLIP_X);
	assert(clip->y0 == CLIP_Y);
	assert(clip->x1 == CLIP_X + CLIP_W);
	assert(clip->y1 == CLIP_Y + CLIP_H);
	assert(clip->w == CLIP_W);
	assert(clip->h == CLIP_H);

	assert(com->size == 0);
	assert(com->instr == D2TK_INSTR_RESET_SCISSOR);
}

static void
_test_reset_scissor()
{
	d2tk_mock_ctx_t ctx = {
		.check = _check_reset_scissor
	};

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, &ctx);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	d2tk_core_pre(core, NULL);
	const ssize_t ref = d2tk_core_bbox_push(core, true,
		&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
	assert(ref >= 0);

	d2tk_core_reset_scissor(core);

	d2tk_core_bbox_pop(core, ref);
	d2tk_core_post(core);
	d2tk_core_free(core);
}

#define FONT_SIZE 11

static void
_check_font_size(const d2tk_com_t *com, const d2tk_clip_t *clip)
{
	assert(clip->x0 == CLIP_X);
	assert(clip->y0 == CLIP_Y);
	assert(clip->x1 == CLIP_X + CLIP_W);
	assert(clip->y1 == CLIP_Y + CLIP_H);
	assert(clip->w == CLIP_W);
	assert(clip->h == CLIP_H);

	assert(com->size == sizeof(d2tk_body_font_size_t));
	assert(com->instr == D2TK_INSTR_FONT_SIZE);
	assert(com->body->font_size.size == FONT_SIZE);
}

static void
_test_font_size()
{
	d2tk_mock_ctx_t ctx = {
		.check = _check_font_size
	};

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, &ctx);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	d2tk_core_pre(core, NULL);
	const ssize_t ref = d2tk_core_bbox_push(core, true,
		&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
	assert(ref >= 0);

	d2tk_core_font_size(core, FONT_SIZE);

	d2tk_core_bbox_pop(core, ref);
	d2tk_core_post(core);
	d2tk_core_free(core);
}

#undef FONT_SIZE

#define FONT_FACE "Monospace"

static void
_check_font_face(const d2tk_com_t *com, const d2tk_clip_t *clip)
{
	assert(clip->x0 == CLIP_X);
	assert(clip->y0 == CLIP_Y);
	assert(clip->x1 == CLIP_X + CLIP_W);
	assert(clip->y1 == CLIP_Y + CLIP_H);
	assert(clip->w == CLIP_W);
	assert(clip->h == CLIP_H);

	assert(com->size == sizeof(d2tk_body_font_face_t) + strlen(FONT_FACE));
	assert(com->instr == D2TK_INSTR_FONT_FACE);
	assert(strcmp(com->body->font_face.face, FONT_FACE) == 0);
}

static void
_test_font_face()
{
	d2tk_mock_ctx_t ctx = {
		.check = _check_font_face
	};

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, &ctx);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	d2tk_core_pre(core, NULL);
	const ssize_t ref = d2tk_core_bbox_push(core, true,
		&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
	assert(ref >= 0);

	d2tk_core_font_face(core, strlen(FONT_FACE), FONT_FACE);

	d2tk_core_bbox_pop(core, ref);
	d2tk_core_post(core);
	d2tk_core_free(core);
}

#undef FONT_FACE

#define TEXT_X 10
#define TEXT_Y 20
#define TEXT_W 30
#define TEXT_H 40
#define TEXT_TEXT "Text" "времени" "äöäÄÖÄ"
#define TEXT_ALIGN D2TK_ALIGN_LEFT

static void
_check_text(const d2tk_com_t *com, const d2tk_clip_t *clip)
{
	assert(clip->x0 == CLIP_X);
	assert(clip->y0 == CLIP_Y);
	assert(clip->x1 == CLIP_X + CLIP_W);
	assert(clip->y1 == CLIP_Y + CLIP_H);
	assert(clip->w == CLIP_W);
	assert(clip->h == CLIP_H);

	assert(com->size == sizeof(d2tk_body_text_t) + strlen(TEXT_TEXT));
	assert(com->instr == D2TK_INSTR_TEXT);
	assert(com->body->text.x == TEXT_X - CLIP_X);
	assert(com->body->text.y == TEXT_Y - CLIP_Y);
	assert(com->body->text.w == TEXT_W);
	assert(com->body->text.h == TEXT_H);
	assert(strcmp(com->body->text.text, TEXT_TEXT) == 0);
	assert(com->body->text.align == TEXT_ALIGN);
}

static void
_test_text()
{
	d2tk_mock_ctx_t ctx = {
		.check = _check_text
	};

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, &ctx);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	d2tk_core_pre(core, NULL);
	const ssize_t ref = d2tk_core_bbox_push(core, true,
		&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
	assert(ref >= 0);

	d2tk_core_text(core, &D2TK_RECT(TEXT_X, TEXT_Y, TEXT_W, TEXT_H),
		strlen(TEXT_TEXT), TEXT_TEXT, TEXT_ALIGN);

	d2tk_core_bbox_pop(core, ref);
	d2tk_core_post(core);
	d2tk_core_free(core);
}

#undef TEXT_X
#undef TEXT_Y
#undef TEXT_W
#undef TEXT_H
#undef TEXT_TEXT
#undef TEXT_ALIGN

#define IMAGE_X 10
#define IMAGE_Y 20
#define IMAGE_W 30
#define IMAGE_H 40
#define IMAGE_PATH "./libre-sugar-skull.png"
#define IMAGE_ALIGN D2TK_ALIGN_LEFT

static void
_check_image(const d2tk_com_t *com, const d2tk_clip_t *clip)
{
	assert(clip->x0 == CLIP_X);
	assert(clip->y0 == CLIP_Y);
	assert(clip->x1 == CLIP_X + CLIP_W);
	assert(clip->y1 == CLIP_Y + CLIP_H);
	assert(clip->w == CLIP_W);
	assert(clip->h == CLIP_H);

	assert(com->size == sizeof(d2tk_body_image_t) + strlen(IMAGE_PATH));
	assert(com->instr == D2TK_INSTR_IMAGE);
	assert(com->body->image.x == IMAGE_X - CLIP_X);
	assert(com->body->image.y == IMAGE_Y - CLIP_Y);
	assert(com->body->image.w == IMAGE_W);
	assert(com->body->image.h == IMAGE_H);
	assert(strcmp(com->body->image.path , IMAGE_PATH) == 0);
	assert(com->body->image.align == IMAGE_ALIGN);
}

static void
_test_image()
{
	d2tk_mock_ctx_t ctx = {
		.check = _check_image
	};

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, &ctx);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	d2tk_core_pre(core, NULL);
	const ssize_t ref = d2tk_core_bbox_push(core, true,
		&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
	assert(ref >= 0);

	d2tk_core_image(core, &D2TK_RECT(IMAGE_X, IMAGE_Y, IMAGE_W, IMAGE_H),
		strlen(IMAGE_PATH), IMAGE_PATH, IMAGE_ALIGN);

	d2tk_core_bbox_pop(core, ref);
	d2tk_core_post(core);
	d2tk_core_free(core);
}

#undef IMAGE_X
#undef IMAGE_Y
#undef IMAGE_W
#undef IMAGE_H
#undef IMAGE_PATH
#undef IMAGE_ALIGN

#define BITMAP_X 10
#define BITMAP_Y 20
#define BITMAP_W 30
#define BITMAP_H 40
#define BITMAP_WIDTH 24
#define BITMAP_HEIGHT 24
#define BITMAP_STRIDE 32*sizeof(uint32_t)
#define BITMAP_ALIGN D2TK_ALIGN_LEFT

static void
_check_bitmap(const d2tk_com_t *com, const d2tk_clip_t *clip)
{
	assert(clip->x0 == CLIP_X);
	assert(clip->y0 == CLIP_Y);
	assert(clip->x1 == CLIP_X + CLIP_W);
	assert(clip->y1 == CLIP_Y + CLIP_H);
	assert(clip->w == CLIP_W);
	assert(clip->h == CLIP_H);

	assert(com->size == sizeof(d2tk_body_bitmap_t));
	assert(com->instr == D2TK_INSTR_BITMAP);
	assert(com->body->bitmap.x == BITMAP_X - CLIP_X);
	assert(com->body->bitmap.y == BITMAP_Y - CLIP_Y);
	assert(com->body->bitmap.w == BITMAP_W);
	assert(com->body->bitmap.h == BITMAP_H);
	assert(com->body->bitmap.align == BITMAP_ALIGN);
	assert(com->body->bitmap.surf.w == BITMAP_WIDTH);
	assert(com->body->bitmap.surf.h == BITMAP_HEIGHT);
	assert(com->body->bitmap.surf.stride == BITMAP_STRIDE);
	for(unsigned y = 0, o = 0;
		y < com->body->bitmap.surf.h;
		y++, o += com->body->bitmap.surf.stride/sizeof(uint32_t))
	{
		for(unsigned x = 0; x < com->body->bitmap.surf.w; x++)
		{
			const unsigned idx = o + x;

			assert(com->body->bitmap.surf.argb[idx] == 999 - idx);
		}
	}
}

static void
_test_bitmap()
{
	d2tk_mock_ctx_t ctx = {
		.check = _check_bitmap
	};

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, &ctx);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	d2tk_core_pre(core, NULL);
	const ssize_t ref = d2tk_core_bbox_push(core, true,
		&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
	assert(ref >= 0);

	uint32_t surf [BITMAP_STRIDE/sizeof(uint32_t)*BITMAP_HEIGHT];
	for(unsigned y = 0, o = 0;
		y < BITMAP_HEIGHT;
		y++, o += BITMAP_STRIDE/sizeof(uint32_t))
	{
		for(unsigned x = 0; x < BITMAP_WIDTH; x++)
		{
			const unsigned idx = o + x;

			surf[idx] = 999 - idx;
		}
	}

	const uint64_t rev = 0;

	d2tk_core_bitmap(core, &D2TK_RECT(BITMAP_X, BITMAP_Y, BITMAP_W, BITMAP_H),
		BITMAP_WIDTH, BITMAP_HEIGHT, BITMAP_STRIDE, surf, rev, BITMAP_ALIGN);

	d2tk_core_bbox_pop(core, ref);
	d2tk_core_post(core);
	d2tk_core_free(core);
}

#undef BITMAP_X
#undef BITMAP_Y
#undef BITMAP_W
#undef BITMAP_H
#undef BITMAP_WIDTH
#undef BITMAP_HEIGHT
#undef BITMAP_STRIDE
#undef BITMAP_ALIGN

#define CUSTOM_X 10
#define CUSTOM_Y 20
#define CUSTOM_W 30
#define CUSTOM_H 40
#define CUSTOM_SIZE 0
#define CUSTOM_DATA NULL

static void
_custom(void *ctx, uint32_t size, const void *data)
{
	assert(ctx == NULL);
	assert(size == CUSTOM_SIZE);
	assert(data == CUSTOM_DATA);
}

static void
_check_custom(const d2tk_com_t *com, const d2tk_clip_t *clip)
{
	assert(clip->x0 == CLIP_X);
	assert(clip->y0 == CLIP_Y);
	assert(clip->x1 == CLIP_X + CLIP_W);
	assert(clip->y1 == CLIP_Y + CLIP_H);
	assert(clip->w == CLIP_W);
	assert(clip->h == CLIP_H);

	assert(com->size == sizeof(d2tk_body_custom_t));
	assert(com->instr == D2TK_INSTR_CUSTOM);
	assert(com->body->custom.x == CUSTOM_X - CLIP_X);
	assert(com->body->custom.y == CUSTOM_Y - CLIP_Y);
	assert(com->body->custom.w == CUSTOM_W);
	assert(com->body->custom.h == CUSTOM_H);
	assert(com->body->custom.size == CUSTOM_SIZE);
	assert(com->body->custom.data == CUSTOM_DATA);
	assert(com->body->custom.custom == _custom);
}

static void
_test_custom()
{
	d2tk_mock_ctx_t ctx = {
		.check = _check_custom
	};

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, &ctx);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	d2tk_core_pre(core, NULL);
	const ssize_t ref = d2tk_core_bbox_push(core, true,
		&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
	assert(ref >= 0);

	d2tk_core_custom(core, &D2TK_RECT(CUSTOM_X, CUSTOM_Y, CUSTOM_W, CUSTOM_H),
		CUSTOM_SIZE, CUSTOM_DATA, _custom);

	d2tk_core_bbox_pop(core, ref);
	d2tk_core_post(core);
	d2tk_core_free(core);
}

#undef CUSTOM_X
#undef CUSTOM_Y
#undef CUSTOM_W
#undef CUSTOM_H
#undef CUSTOM_SIZE
#undef CUSTOM_DATA

#define STROKE_WIDTH 2

static void
_check_stroke_width(const d2tk_com_t *com, const d2tk_clip_t *clip)
{
	assert(clip->x0 == CLIP_X);
	assert(clip->y0 == CLIP_Y);
	assert(clip->x1 == CLIP_X + CLIP_W);
	assert(clip->y1 == CLIP_Y + CLIP_H);
	assert(clip->w == CLIP_W);
	assert(clip->h == CLIP_H);

	assert(com->size == sizeof(d2tk_body_stroke_width_t));
	assert(com->instr == D2TK_INSTR_STROKE_WIDTH);
	assert(com->body->stroke_width.width == STROKE_WIDTH);
}

static void
_test_stroke_width()
{
	d2tk_mock_ctx_t ctx = {
		.check = _check_stroke_width
	};

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver, &ctx);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	d2tk_core_pre(core, NULL);
	const ssize_t ref = d2tk_core_bbox_push(core, true,
		&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
	assert(ref >= 0);

	d2tk_core_stroke_width(core, STROKE_WIDTH);

	d2tk_core_bbox_pop(core, ref);
	d2tk_core_post(core);
	d2tk_core_free(core);
}

#undef STROKE_WIDTH

static void
_check_triple(const d2tk_com_t *com, const d2tk_clip_t *clip)
{
	assert(clip->x0 == CLIP_X);
	assert(clip->y0 == CLIP_Y);
	assert(clip->x1 == CLIP_X + CLIP_W);
	assert(clip->y1 == CLIP_Y + CLIP_H);
	assert(clip->w == CLIP_W);
	assert(clip->h == CLIP_H);

	(void)com; //FIXME
}

static void
_test_triple()
{
	d2tk_mock_ctx_t ctx = {
		.check = _check_triple
	};

	d2tk_core_t *core = d2tk_core_new(&d2tk_mock_driver_triple, &ctx);
	assert(core);

	d2tk_core_set_dimensions(core, DIM_W, DIM_H);

	for(unsigned i = 0; i < 3; i++)
	{
		d2tk_core_pre(core, NULL);

		if(i != 0)
		{
			const ssize_t ref = d2tk_core_bbox_push(core, true,
				&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
			assert(ref >= 0);

			d2tk_core_rect(core, &D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));

			d2tk_core_bbox_pop(core, ref);
		}

		if(i != 1)
		{
			const ssize_t ref = d2tk_core_bbox_push(core, true,
				&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
			assert(ref >= 0);

			d2tk_core_rect(core, &D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W/2, CLIP_H));

			d2tk_core_bbox_pop(core, ref);
		}

		if(i != 2)
		{
			const ssize_t ref = d2tk_core_bbox_push(core, true,
				&D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H));
			assert(ref >= 0);

			d2tk_core_rect(core, &D2TK_RECT(CLIP_X, CLIP_Y, CLIP_W, CLIP_H/2));

			d2tk_core_bbox_pop(core, ref);
		}

		d2tk_core_post(core);
	}

	d2tk_core_free(core);
}

int
main(int argc __attribute__((unused)), char **argv __attribute__((unused)))
{
	_test_hash();
	_test_hash_foreach();
	_test_rect_shrink();
	_test_point();
	_test_dimensions();
	_test_bg_color();

	_test_move_to();
	_test_line_to();
	_test_rect();
	_test_rounded_rect();
	_test_arc();
	_test_curve_to();
	_test_color();
	_test_linear_gradient();
	_test_stroke();
	_test_fill();
	_test_save();
	_test_restore();
	_test_rotate();
	_test_begin_path();
	_test_close_path();
	_test_scissor();
	_test_reset_scissor();
	_test_font_size();
	_test_font_face();
	_test_text();
	_test_image();
	_test_bitmap();
	_test_custom();
	_test_stroke_width();

	_test_triple();

	return EXIT_SUCCESS;
}
