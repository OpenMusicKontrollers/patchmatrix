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

#include <assert.h>
#include <stdlib.h>

#include "mock.h"

static inline int
_d2tk_mock_context(void *data, void *pctx)
{
	d2tk_mock_ctx_t *ctx = data;
	assert(ctx);

	assert(pctx == NULL);

	return 0;
}

static inline void
_d2tk_mock_pre(void *data, d2tk_core_t *core, d2tk_coord_t w, d2tk_coord_t h,
	unsigned pass)
{
	d2tk_mock_ctx_t *ctx = data;
	assert(ctx);

	assert(core);
	assert(w == DIM_W);
	assert(h == DIM_H);
	assert(pass == 0);
}

static inline bool
_d2tk_mock_post(void *data, d2tk_core_t *core, d2tk_coord_t w, d2tk_coord_t h,
	unsigned pass)
{
	d2tk_mock_ctx_t *ctx = data;
	assert(ctx);

	assert(core);
	assert(w == DIM_W);
	assert(h == DIM_H);
	assert(pass == 0);

	d2tk_rect_t rect;
	assert(d2tk_core_get_pixels(core, &rect));
	assert( (rect.w != 0) && (rect.h != 0) );

	return false; // do NOT enter 3rd pass
}

static inline void
_d2tk_mock_end(void *data, d2tk_core_t *core, d2tk_coord_t w, d2tk_coord_t h)
{
	d2tk_mock_ctx_t *ctx = data;
	assert(ctx);

	assert(core);
	assert(w == DIM_W);
	assert(h == DIM_H);
}

static inline void
_d2tk_mock_sprite_free(void *data, uint8_t type, uintptr_t body)
{
	d2tk_mock_ctx_t *ctx = data;
	assert(ctx);

	assert(type == 1);

	uint32_t *dummy = (uint32_t *)body;
	assert(*dummy == 1234);
	free(dummy);
}

static inline void
_d2tk_mock_process(void *data, d2tk_core_t *core, const d2tk_com_t *com,
	d2tk_coord_t xo, d2tk_coord_t yo,
	const d2tk_clip_t *clip __attribute__((unused)), unsigned pass)
{
	d2tk_mock_ctx_t *ctx = data;
	assert(ctx);

	assert(core);
	assert(com);
	assert(xo == CLIP_X);
	assert(yo == CLIP_Y);
	assert(pass == 0);

	assert(com->instr == D2TK_INSTR_BBOX);

	const d2tk_body_bbox_t *body = &com->body->bbox;
	assert(body->dirty == false);
	assert(body->cached == true);
	assert(body->hash != 0x0); //FIXME

	uintptr_t *sprite = d2tk_core_get_sprite(core, body->hash, 1);
	assert(sprite);
	assert(*sprite == 0);

	uint32_t *dummy = calloc(1, sizeof(uint32_t));
	assert(dummy);
	*dummy = 1234;

	*sprite = (uintptr_t)dummy;

	assert(sprite == d2tk_core_get_sprite(core, body->hash, 1));
	assert(sprite != d2tk_core_get_sprite(core, body->hash, 0));
	assert(sprite != d2tk_core_get_sprite(core, ~body->hash, 1));
	assert(sprite != d2tk_core_get_sprite(core, ~body->hash, 0));

	unsigned num = 0;
	D2TK_COM_FOREACH_CONST(com, bbox)
	{
		if(ctx->check)
		{
			ctx->check(bbox, &body->clip);
		}
		num += 1;
	}
	assert(num == 1);
}

static inline void
_d2tk_mock_process_triple(void *data, d2tk_core_t *core, const d2tk_com_t *com,
	d2tk_coord_t xo, d2tk_coord_t yo,
	const d2tk_clip_t *clip __attribute__((unused)), unsigned pass)
{
	d2tk_mock_ctx_t *ctx = data;
	assert(ctx);

	assert(core);
	assert(com);
	assert(xo == CLIP_X);
	assert(yo == CLIP_Y);
	assert(pass == 0);

	assert(com->instr == D2TK_INSTR_BBOX);

	const d2tk_body_bbox_t *body = &com->body->bbox;
	//assert(body->dirty == false);
	assert(body->cached == true);
	assert(body->hash != 0x0); //FIXME

	uintptr_t *sprite = d2tk_core_get_sprite(core, body->hash, 1);
	assert(sprite);

	if(*sprite == 0)
	{
		uint32_t *dummy = calloc(1, sizeof(uint32_t));
		assert(dummy);
		*dummy = 1234;

		*sprite = (uintptr_t)dummy;
	}

	assert(sprite == d2tk_core_get_sprite(core, body->hash, 1));
	assert(sprite != d2tk_core_get_sprite(core, body->hash, 0));
	assert(sprite != d2tk_core_get_sprite(core, ~body->hash, 1));
	assert(sprite != d2tk_core_get_sprite(core, ~body->hash, 0));

	unsigned num = 0;
	D2TK_COM_FOREACH_CONST(com, bbox)
	{
		if(ctx->check)
		{
			ctx->check(bbox, &body->clip);
		}
		num += 1;
	}
	assert(num == 1);
}

static inline void
_d2tk_mock_process_lazy(void *data, d2tk_core_t *core, const d2tk_com_t *com,
	d2tk_coord_t xo __attribute__((unused)), d2tk_coord_t yo __attribute__((unused)),
	const d2tk_clip_t *clip __attribute__((unused)), unsigned pass)
{
	d2tk_mock_ctx_t *ctx = data;
	assert(ctx);

	assert(core);
	assert(com);
	assert(pass == 0);

	assert(com->instr == D2TK_INSTR_BBOX);

	const d2tk_body_bbox_t *body = &com->body->bbox;
	assert(body->cached == true);
	assert(body->hash != 0x0); //FIXME

	uintptr_t *sprite = d2tk_core_get_sprite(core, body->hash, 1);
	assert(sprite);

	if(*sprite == 0)
	{
		uint32_t *dummy = calloc(1, sizeof(uint32_t));
		assert(dummy);
		*dummy = 1234;

		*sprite = (uintptr_t)dummy;
	}

	assert(sprite == d2tk_core_get_sprite(core, body->hash, 1));
	assert(sprite != d2tk_core_get_sprite(core, body->hash, 0));
	assert(sprite != d2tk_core_get_sprite(core, ~body->hash, 1));
	assert(sprite != d2tk_core_get_sprite(core, ~body->hash, 0));

	unsigned num = 0;
	D2TK_COM_FOREACH_CONST(com, bbox)
	{
		if(ctx->check)
		{
			ctx->check(bbox, &body->clip);
		}
		num += 1;
	}
	assert(num > 0);
}

const d2tk_core_driver_t d2tk_mock_driver = {
	.new = NULL,
	.free = NULL,
	.context = _d2tk_mock_context,
	.pre = _d2tk_mock_pre,
	.process = _d2tk_mock_process,
	.post = _d2tk_mock_post,
	.end = _d2tk_mock_end,
	.sprite_free = _d2tk_mock_sprite_free
};

const d2tk_core_driver_t d2tk_mock_driver_triple = {
	.new = NULL,
	.free = NULL,
	.context = _d2tk_mock_context,
	.pre = _d2tk_mock_pre,
	.process = _d2tk_mock_process_triple,
	.post = _d2tk_mock_post,
	.end = _d2tk_mock_end,
	.sprite_free = _d2tk_mock_sprite_free
};

const d2tk_core_driver_t d2tk_mock_driver_lazy = {
	.new = NULL,
	.free = NULL,
	.context = _d2tk_mock_context,
	.pre = _d2tk_mock_pre,
	.process = _d2tk_mock_process_lazy,
	.post = _d2tk_mock_post,
	.end = _d2tk_mock_end,
	.sprite_free = _d2tk_mock_sprite_free
};
