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

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <inttypes.h>
#include <limits.h>
#include <assert.h>
#if defined(_WIN32)
#	include <winsock2.h>
#else
#	include <arpa/inet.h>
#endif

#include "core_internal.h"
#include <d2tk/hash.h>

#if D2TK_FONTCONFIG
#	include <fontconfig/fontconfig.h>
#endif

#define _D2TK_SPRITES_MAX			0x10000 //FIXME how big?
#define _D2TK_SPRITES_MASK		(_D2TK_SPRITES_MAX - 1)
#define _D2TK_SPRITES_TTL			0x100

#define _D2TK_MEMCACHES_MAX		0x10000 //FIXME how big?
#define _D2TK_MEMCACHES_MASK	(_D2TK_MEMCACHES_MAX - 1)
#define _D2TK_MEMCACHES_TTL		0x100

typedef struct _d2tk_mem_t d2tk_mem_t;
typedef struct _d2tk_bitmap_t d2tk_bitmap_t;
typedef struct _d2tk_sprite_t d2tk_sprite_t;
typedef struct _d2tk_memcache_t d2tk_memcache_t;
typedef struct _d2tk_widget_body_t d2tk_widget_body_t;

struct _d2tk_mem_t {
	size_t size;
	size_t offset;
	uint8_t *buf;
};

struct _d2tk_bitmap_t {
	size_t size;
	uint32_t *pixels;
	uint32_t *template;
	size_t nfills;
	d2tk_coord_t x0;
	d2tk_coord_t x1;
	d2tk_coord_t y0;
	d2tk_coord_t y1;
};

struct _d2tk_sprite_t {
	uint64_t hash;
	uintptr_t body;
	uint32_t type;
	uint32_t ttl;
};

struct _d2tk_memcache_t {
	uint64_t hash;
	uintptr_t body;
	uint32_t ttl;
};

struct _d2tk_widget_body_t {
	size_t size;
	uint8_t buf [];
};

struct _d2tk_widget_t {
	size_t ref;
	uintptr_t *body;
};

struct _d2tk_core_t {
	const d2tk_core_driver_t *driver;
	void *data;

	d2tk_coord_t w;
	d2tk_coord_t h;

	struct {
		d2tk_coord_t x;
		d2tk_coord_t y;
	} ref;

	d2tk_mem_t mem [2];
	bool curmem;

	bool full_refresh;

	d2tk_bitmap_t bitmap;

	uint32_t bg_color;

	struct {
		uint32_t sprites;
		uint32_t memcaches;
	} ttl;

	d2tk_sprite_t sprites [_D2TK_SPRITES_MAX];
	d2tk_memcache_t memcaches [_D2TK_MEMCACHES_MAX];

	ssize_t parent;
};

const size_t d2tk_widget_sz = sizeof(d2tk_widget_t);

D2TK_API void
d2tk_rect_shrink_x(d2tk_rect_t *dst, const d2tk_rect_t *src,
	d2tk_coord_t brd)
{
	dst->x = src->x + brd;
	dst->y = src->y;
	brd <<= 1;
	dst->w = src->w - brd;
	dst->h = src->h;
}

D2TK_API void
d2tk_rect_shrink_y(d2tk_rect_t *dst, const d2tk_rect_t *src,
	d2tk_coord_t brd)
{
	dst->x = src->x;
	dst->y = src->y + brd;
	brd <<= 1;
	dst->w = src->w;
	dst->h = src->h - brd;
}

D2TK_API void
d2tk_rect_shrink(d2tk_rect_t *dst, const d2tk_rect_t *src,
	d2tk_coord_t brd)
{
	dst->x = src->x + brd;
	dst->y = src->y + brd;
	brd <<= 1;
	dst->w = src->w - brd;
	dst->h = src->h - brd;
}

uintptr_t *
d2tk_core_get_sprite(d2tk_core_t *core, uint64_t hash, uint8_t type)
{
	for(unsigned i = 0; i < _D2TK_SPRITES_MAX; i++)
	{
		const unsigned j = (hash + i*i) & _D2TK_SPRITES_MASK;
		d2tk_sprite_t *sprite = &core->sprites[j];

		if(sprite->body) // sprite is already taken
		{
			// is this the sprite we're looking for?
			if( (sprite->hash == hash) && (sprite->type == type) )
			{
				sprite->ttl = core->ttl.sprites;
				sprite->type = type;
				return &sprite->body;
			}
			else // not our sprite
			{
				continue;
			}
		}

		// empty sprite, ready to be taken
		sprite->hash = hash;
		sprite->ttl = core->ttl.sprites;
		sprite->type = type;
		return &sprite->body;
	}

	return NULL; // out of memory FIXME increase memory and remap existing sprites
}

static inline void
_d2tk_sprites_free(d2tk_core_t *core)
{
	for(unsigned i = 0; i < _D2TK_SPRITES_MAX; i++)
	{
		d2tk_sprite_t *sprite = &core->sprites[i];

		if(!sprite->hash)
		{
			continue;
		}

		if(sprite->body)
		{
			core->driver->sprite_free(core->data, sprite->type, sprite->body);
			sprite->type = 0;
			sprite->body = 0;
		}

		sprite->hash = 0;
	}
}

static inline void
_d2tk_sprites_gc(d2tk_core_t *core)
{
	for(unsigned i = 0; i < _D2TK_SPRITES_MAX; i++)
	{
		d2tk_sprite_t *sprite = &core->sprites[i];

		if(!sprite->hash || (--sprite->ttl > 0) )
		{
			continue;
		}

		if(sprite->body)
		{
#if D2TK_DEBUG
			fprintf(stderr, "\tgc sprites (%08"PRIx64")\n", sprite->hash);
#endif
			core->driver->sprite_free(core->data, sprite->type, sprite->body);
			sprite->type = 0;
			sprite->body = 0;
		}

		sprite->hash = 0;
	}
}

static inline void
_d2tk_mem_init(d2tk_mem_t *mem, size_t size)
{
	mem->size = size;
	mem->offset = 0;
	mem->buf = malloc(mem->size);
}

static inline void
_d2tk_mem_deinit(d2tk_mem_t *mem)
{
	mem->size = 0;
	mem->offset = 0;
	free(mem->buf);
	mem->buf = NULL;
}

static inline void
_d2tk_mem_reset(d2tk_mem_t *mem)
{
	mem->offset = 0;
	memset(mem->buf, 0x0, mem->size);
}

static inline uintptr_t *
_d2tk_core_get_memcache(d2tk_core_t *core, uint64_t hash)
{
	for(unsigned i = 0; i < _D2TK_MEMCACHES_MAX; i++)
	{
		const unsigned j = (hash + i*i) & _D2TK_MEMCACHES_MASK;
		d2tk_memcache_t *memcache = &core->memcaches[j];

		if(memcache->body) // memcache is already taken
		{
			// is this the memcache we're looking for?
			if(memcache->hash == hash)
			{
				memcache->ttl = core->ttl.memcaches;
				return &memcache->body;
			}
			else // not our memcache
			{
				continue;
			}
		}

		// empty memcache, ready to be taken
		memcache->hash = hash;
		memcache->ttl = core->ttl.memcaches;
		return &memcache->body;
	}

	return NULL; // out of memory FIXME increase memory and remap existing memcaches
}

static inline void
_d2tk_memcaches_free(d2tk_core_t *core)
{
	for(unsigned i = 0; i < _D2TK_MEMCACHES_MAX; i++)
	{
		d2tk_memcache_t *memcache = &core->memcaches[i];

		if(!memcache->hash)
		{
			continue;
		}

		if(memcache->body)
		{
			d2tk_widget_body_t *body = (d2tk_widget_body_t *)memcache->body;

			free(body);
			memcache->body = 0;
		}

		memcache->hash = 0;
	}
}

static inline void
_d2tk_memcaches_gc(d2tk_core_t *core)
{
	for(unsigned i = 0; i < _D2TK_MEMCACHES_MAX; i++)
	{
		d2tk_memcache_t *memcache = &core->memcaches[i];

		if(!memcache->hash || (--memcache->ttl > 0) )
		{
			continue;
		}

		if(memcache->body)
		{
#if D2TK_DEBUG
			fprintf(stderr, "\tgc memcaches (%08"PRIx64")\n", memcache->hash);
#endif
			d2tk_widget_body_t *body = (d2tk_widget_body_t *)memcache->body;

			free(body);
			memcache->body = 0;
		}

		memcache->hash = 0;
	}
}

static inline void
_d2tk_bitmap_template_refill(d2tk_core_t *core)
{
	d2tk_bitmap_t *bitmap = &core->bitmap;

	for(d2tk_coord_t x = 0; x < core->w; x++)
	{
		bitmap->template[x] = core->bg_color;
	}
}

static inline void
_d2tk_bitmap_resize(d2tk_core_t *core, d2tk_coord_t w, d2tk_coord_t h)
{
	d2tk_bitmap_t *bitmap = &core->bitmap;

	const size_t stride = w*sizeof(uint32_t);
	bitmap->size = h*stride;
	bitmap->pixels = realloc(bitmap->pixels, bitmap->size);
	bitmap->template = realloc(bitmap->template, stride);
	_d2tk_bitmap_template_refill(core);
}

static inline void
_d2tk_bitmap_deinit(d2tk_bitmap_t *bitmap)
{
	free(bitmap->pixels);
	bitmap->pixels = NULL;
	free(bitmap->template);
	bitmap->template = NULL;
	bitmap->size = 0;
	bitmap->nfills = 0;
}

static inline void
_d2tk_bitmap_reset(d2tk_core_t *core)
{
	d2tk_bitmap_t *bitmap = &core->bitmap;

	// x1/y1 may be wrong after window shrink
	const d2tk_coord_t x1 = bitmap->x1 < core->w
		? bitmap->x1
		: core->w;
	const d2tk_coord_t y1 = bitmap->y1 < core->h
		? bitmap->y1
		: core->h;

	const size_t stride = (x1 - bitmap->x0)*sizeof(uint32_t);

	for(d2tk_coord_t y = bitmap->y0, Y = y*core->w; y < y1; y++, Y+=core->w)
	{
		memset(&bitmap->pixels[Y + bitmap->x0], 0x0, stride);
	}

	bitmap->nfills = 0;
	bitmap->x0 = INT_MAX;
	bitmap->x1 = INT_MIN;
	bitmap->y0 = INT_MAX;
	bitmap->y1 = INT_MIN;
}

static inline void
_d2tk_clip_clip(d2tk_core_t *core, d2tk_clip_t *dst, const d2tk_clip_t *src)
{
	*dst = *src;

	if(dst->x0 < 0)
	{
		dst->x0 = 0;
	}
	if(dst->x1 < 0)
	{
		dst->x1 = 0;
	}

	if(dst->y0 < 0)
	{
		dst->y0 = 0;
	}
	if(dst->y1 < 0)
	{
		dst->y1 = 0;
	}

	if(dst->x0 >= core->w)
	{
		dst->x0 = core->w - 1;
	}
	if(dst->x1 >= core->w)
	{
		dst->x1 = core->w - 1;
	}

	if(dst->y0 >= core->h)
	{
		dst->y0 = core->h - 1;
	}
	if(dst->y1 >= core->h)
	{
		dst->y1 = core->h - 1;
	}
}

static inline void
_d2tk_bitmap_fill(d2tk_core_t *core, const d2tk_clip_t *clip)
{
	d2tk_bitmap_t *bitmap = &core->bitmap;

	d2tk_clip_t dst;
	_d2tk_clip_clip(core, &dst, clip);

	const size_t stride = (dst.x1 - dst.x0)*sizeof(uint32_t);

	for(d2tk_coord_t y = dst.y0, Y = y*core->w; y < dst.y1; y++, Y+=core->w)
	{
		memcpy(&bitmap->pixels[Y + dst.x0], bitmap->template, stride);
	}

	// update area of interest
	if(dst.x0 < bitmap->x0)
	{
		bitmap->x0 = dst.x0;
	}

	if(dst.x1 > bitmap->x1)
	{
		bitmap->x1 = dst.x1;
	}

	if(dst.y0 < bitmap->y0)
	{
		bitmap->y0 = dst.y0;
	}

	if(dst.y1 > bitmap->y1)
	{
		bitmap->y1 = dst.y1;
	}

	bitmap->nfills++;
}

static inline d2tk_com_t *
_d2tk_com_begin(d2tk_com_t *com)
{
	d2tk_com_t *bbox = (d2tk_com_t *)((uint8_t *)com + sizeof(d2tk_com_t)
		+ D2TK_PAD_SIZE(sizeof(d2tk_body_bbox_t)));

	return bbox;
}

static inline d2tk_com_t *
_d2tk_com_get_end(d2tk_com_t *com)
{
	return (d2tk_com_t *)((uint8_t *)com + sizeof(d2tk_com_t)
		+ com->size);
}

static inline bool
_d2tk_com_not_end(d2tk_com_t *end, d2tk_com_t *bbox)
{
	return bbox < end;
}

static inline d2tk_com_t *
_d2tk_com_next(d2tk_com_t *bbox)
{
	d2tk_com_t *nxt = (d2tk_com_t *)((uint8_t *)bbox + sizeof(d2tk_com_t)
		+ D2TK_PAD_SIZE(bbox->size));

	return nxt;
}

const d2tk_com_t *
d2tk_com_begin_const(const d2tk_com_t *com)
{
	return _d2tk_com_begin((d2tk_com_t *)com);
}

const d2tk_com_t *
d2tk_com_get_end_const(const d2tk_com_t *com)
{
	return _d2tk_com_get_end((d2tk_com_t *)com);
}

bool
d2tk_com_not_end_const(const d2tk_com_t *end, const d2tk_com_t *bbox)
{
	return _d2tk_com_not_end((d2tk_com_t *)end, (d2tk_com_t *)bbox);
}

const d2tk_com_t *
d2tk_com_next_const(const d2tk_com_t *bbox)
{
	return _d2tk_com_next((d2tk_com_t *)bbox);
}

#define D2TK_COM_FOREACH_FROM(COM, FROM, BBOX) \
	for(d2tk_com_t *(BBOX) = (FROM), \
			*__end = _d2tk_com_get_end((COM)); \
		_d2tk_com_not_end(__end, (BBOX)); \
		(BBOX) = _d2tk_com_next((BBOX)))

#define D2TK_COM_FOREACH(COM, BBOX) \
	for(d2tk_com_t *(BBOX) = _d2tk_com_begin((COM)), \
			*__end = _d2tk_com_get_end((COM)); \
		_d2tk_com_not_end(__end, (BBOX)); \
		(BBOX) = _d2tk_com_next((BBOX)))

static inline void
_d2tk_bbox_mask(d2tk_core_t *core, d2tk_com_t *com)
{
	d2tk_body_bbox_t *body = &com->body->bbox;

	if(body->container)
	{
		D2TK_COM_FOREACH(com, bbox)
		{
			_d2tk_bbox_mask(core, bbox);
		}
	}
	else
	{
		_d2tk_bitmap_fill(core, &body->clip);
	}

	body->dirty = true;
}

uint32_t *
d2tk_core_get_pixels(d2tk_core_t *core, d2tk_rect_t *rect)
{
	if(rect)
	{
		rect->x = core->bitmap.x0;
		rect->y = core->bitmap.y0;
		rect->w = core->bitmap.x1 - core->bitmap.x0;
		rect->h = core->bitmap.y1 - core->bitmap.y0;
	}

	return core->bitmap.pixels;
}

static inline bool
_d2tk_bitmap_query(d2tk_core_t *core, d2tk_body_bbox_t *body)
{
	const d2tk_clip_t *clip = &body->clip;

	d2tk_clip_t dst;
	_d2tk_clip_clip(core, &dst, clip);

	for(d2tk_coord_t y = dst.y0, Y = y*core->w; y < dst.y1; y++, Y+=core->w)
	{
		for(d2tk_coord_t x = dst.x0; x < dst.x1; x++)
		{
			if(core->bitmap.pixels[Y + x])
			{
				body->dirty = true;
				return true;
			}
		}
	}

	return false;
}

static inline void
_d2tk_mem_compact(d2tk_mem_t *mem)
{
	if(mem->offset == 0)
	{
		return;
	}

	for(size_t nsize = (mem->size >> 1);
		mem->offset <= nsize;
		nsize >>= 1)
	{
		uint8_t *nbuf = realloc(mem->buf, nsize);
		assert(nbuf);

		mem->buf = nbuf;
		mem->size = nsize;
	}
}

static inline d2tk_com_t *
_d2tk_mem_get_com(d2tk_mem_t *mem)
{
	return (d2tk_com_t *)&mem->buf[0];
}

static inline uint8_t *
_d2tk_mem_append_request(d2tk_mem_t *mem, size_t len)
{
	const size_t padlen = D2TK_PAD_SIZE(len);

	for(size_t msize = mem->offset + padlen, nsize = (mem->size << 1);
		mem->size < msize;
		nsize <<= 1)
	{
		uint8_t *nbuf = realloc(mem->buf, nsize);
		assert(nbuf);

		memset(&nbuf[mem->size], 0x0, mem->size);

		mem->buf = nbuf;
		mem->size = nsize;
	}

	return &mem->buf[mem->offset];
}

static inline void
_d2tk_mem_append_advance(d2tk_mem_t *mem, size_t len)
{
	const size_t padlen = D2TK_PAD_SIZE(len);

	mem->offset += padlen;
}

static inline void
_d2tk_append_simple(d2tk_core_t *core, d2tk_instr_t type)
{
	const size_t len = sizeof(d2tk_com_t);
	d2tk_mem_t *mem = &core->mem[core->curmem];
	d2tk_com_t *com = (d2tk_com_t *)_d2tk_mem_append_request(mem, len);

	if(com)
	{
		com->size = 0;
		com->instr = type;

		_d2tk_mem_append_advance(mem, len);
	}
}

static inline d2tk_body_t *
_d2tk_append_request(d2tk_core_t *core, size_t size, d2tk_instr_t type)
{
	const size_t len = sizeof(d2tk_com_t) + size;
	d2tk_mem_t *mem = &core->mem[core->curmem];
	d2tk_com_t *com = (d2tk_com_t *)_d2tk_mem_append_request(mem, len);

	if(com)
	{
		com->size = size;
		com->instr = type;

		return com->body;
	}

	return NULL;
}

static inline void
_d2tk_append_advance(d2tk_core_t *core, size_t size)
{
	const size_t len = sizeof(d2tk_com_t) + size;
	d2tk_mem_t *mem = &core->mem[core->curmem];

	_d2tk_mem_append_advance(mem, len);
}

D2TK_API void
d2tk_core_move_to(d2tk_core_t *core, d2tk_coord_t x, d2tk_coord_t y)
{
	const size_t len = sizeof(d2tk_body_move_to_t);
	d2tk_body_t *body = _d2tk_append_request(core, len, D2TK_INSTR_MOVE_TO);

	if(body)
	{
		body->move_to.x = x;
		body->move_to.y = y;

		body->move_to.x -= core->ref.x;
		body->move_to.y -= core->ref.y;

		_d2tk_append_advance(core, len);
	}
}

D2TK_API void
d2tk_core_line_to(d2tk_core_t *core, d2tk_coord_t x, d2tk_coord_t y)
{
	const size_t len = sizeof(d2tk_body_line_to_t);
	d2tk_body_t *body = _d2tk_append_request(core, len, D2TK_INSTR_LINE_TO);

	if(body)
	{
		body->line_to.x = x;
		body->line_to.y = y;

		body->line_to.x -= core->ref.x;
		body->line_to.y -= core->ref.y;

		_d2tk_append_advance(core, len);
	}
}

D2TK_API void
d2tk_core_rect(d2tk_core_t *core, const d2tk_rect_t *rect)
{
	const size_t len = sizeof(d2tk_body_rect_t);
	d2tk_body_t *body = _d2tk_append_request(core, len, D2TK_INSTR_RECT);

	if(body)
	{
		body->rect.x = rect->x;
		body->rect.y = rect->y;
		body->rect.w = rect->w;
		body->rect.h = rect->h;

		body->rect.x -= core->ref.x;
		body->rect.y -= core->ref.y;

		_d2tk_append_advance(core, len);
	}
}

D2TK_API void
d2tk_core_rounded_rect(d2tk_core_t *core, const d2tk_rect_t *rect,
	d2tk_coord_t r)
{
	const size_t len = sizeof(d2tk_body_rounded_rect_t);
	d2tk_body_t *body = _d2tk_append_request(core, len, D2TK_INSTR_ROUNDED_RECT);

	if(body)
	{
		body->rounded_rect.x = rect->x;
		body->rounded_rect.y = rect->y;
		body->rounded_rect.w = rect->w;
		body->rounded_rect.h = rect->h;
		body->rounded_rect.r = r;

		body->rounded_rect.x -= core->ref.x;
		body->rounded_rect.y -= core->ref.y;

		_d2tk_append_advance(core, len);
	}
}

D2TK_API void
d2tk_core_arc(d2tk_core_t *core, d2tk_coord_t x, d2tk_coord_t y, d2tk_coord_t r,
	d2tk_coord_t a, d2tk_coord_t b, bool cw)
{
	const size_t len = sizeof(d2tk_body_arc_t);
	d2tk_body_t *body = _d2tk_append_request(core, len, D2TK_INSTR_ARC);

	if(body)
	{
		body->arc.x = x;
		body->arc.y = y;
		body->arc.r = r;
		body->arc.a = a;
		body->arc.b = b;
		body->arc.cw = cw;

		body->arc.x -= core->ref.x;
		body->arc.y -= core->ref.y;

		_d2tk_append_advance(core, len);
	}
}

D2TK_API void
d2tk_core_curve_to(d2tk_core_t *core, d2tk_coord_t x1, d2tk_coord_t y1,
	d2tk_coord_t x2, d2tk_coord_t y2, d2tk_coord_t x3, d2tk_coord_t y3)
{
	const size_t len = sizeof(d2tk_body_curve_to_t);
	d2tk_body_t *body = _d2tk_append_request(core, len, D2TK_INSTR_CURVE_TO);

	if(body)
	{
		body->curve_to.x1 = x1;
		body->curve_to.y1 = y1;
		body->curve_to.x2 = x2;
		body->curve_to.y2 = y2;
		body->curve_to.x3 = x3;
		body->curve_to.y3 = y3;

		body->curve_to.x1 -= core->ref.x;
		body->curve_to.x2 -= core->ref.x;
		body->curve_to.x3 -= core->ref.x;

		body->curve_to.y1 -= core->ref.y;
		body->curve_to.y2 -= core->ref.y;
		body->curve_to.y3 -= core->ref.y;

		_d2tk_append_advance(core, len);
	}
}

void
d2tk_core_set_bg_color(d2tk_core_t *core, uint32_t rgba)
{
	core->bg_color = htonl(rgba);
	_d2tk_bitmap_template_refill(core);
}

uint32_t
d2tk_core_get_bg_color(d2tk_core_t *core)
{
	return ntohl(core->bg_color);
}

D2TK_API void
d2tk_core_color(d2tk_core_t *core, uint32_t rgba)
{
	const size_t len = sizeof(d2tk_body_color_t);
	d2tk_body_t *body = _d2tk_append_request(core, len, D2TK_INSTR_COLOR);

	if(body)
	{
		body->color.rgba = rgba;

		_d2tk_append_advance(core, len);
	}
}

D2TK_API void
d2tk_core_linear_gradient(d2tk_core_t *core, const d2tk_point_t point [2],
	const uint32_t rgba [2])
{
	const size_t len = sizeof(d2tk_body_linear_gradient_t);
	d2tk_body_t *body = _d2tk_append_request(core, len, D2TK_INSTR_LINEAR_GRADIENT);

	if(body)
	{
		body->linear_gradient.p[0].x = point[0].x;
		body->linear_gradient.p[0].y = point[0].y;
		body->linear_gradient.p[1].x = point[1].x;
		body->linear_gradient.p[1].y = point[1].y;
		body->linear_gradient.rgba[0] = rgba[0];
		body->linear_gradient.rgba[1] = rgba[1];

		body->linear_gradient.p[0].x -= core->ref.x;
		body->linear_gradient.p[0].y -= core->ref.y;
		body->linear_gradient.p[1].x -= core->ref.x;
		body->linear_gradient.p[1].y -= core->ref.y;

		_d2tk_append_advance(core, len);
	}
}

D2TK_API void
d2tk_core_stroke(d2tk_core_t *core)
{
	_d2tk_append_simple(core, D2TK_INSTR_STROKE);
}

D2TK_API void
d2tk_core_fill(d2tk_core_t *core)
{
	_d2tk_append_simple(core, D2TK_INSTR_FILL);
}

D2TK_API void
d2tk_core_save(d2tk_core_t *core)
{
	_d2tk_append_simple(core, D2TK_INSTR_SAVE);
}

D2TK_API void
d2tk_core_restore(d2tk_core_t *core)
{
	_d2tk_append_simple(core, D2TK_INSTR_RESTORE);
}

D2TK_API void
d2tk_core_rotate(d2tk_core_t *core, d2tk_coord_t deg)
{
	const size_t len = sizeof(d2tk_body_rotate_t);
	d2tk_body_t *body = _d2tk_append_request(core, len, D2TK_INSTR_ROTATE);

	if(body)
	{
		body->rotate.deg = deg;

		_d2tk_append_advance(core, len);
	}
}

D2TK_API d2tk_widget_t *
d2tk_core_widget_begin(d2tk_core_t *core, uint64_t hash, d2tk_widget_t *widget)
{
	widget->body = _d2tk_core_get_memcache(core, hash);
	assert(widget->body);

	if(*widget->body) // bluntly use cached widget instruction buffer
	{
		d2tk_mem_t *mem = &core->mem[core->curmem];
		const d2tk_widget_body_t *body = (const d2tk_widget_body_t *)*widget->body;
		uint8_t *dst = _d2tk_mem_append_request(mem, body->size);

		if(dst)
		{
			memcpy(dst, body->buf, body->size);
			_d2tk_mem_append_advance(mem, body->size);
		}

		widget->ref = 0;

		return NULL;
	}

	// store current offset of instruction buffer
	d2tk_mem_t *mem = &core->mem[core->curmem];
	const size_t ref = mem->offset;

	widget->ref = ref;

	return widget;
}

D2TK_API bool
d2tk_core_widget_not_end(d2tk_core_t *core __attribute__((unused)),
	d2tk_widget_t *widget)
{
	return widget ? true : false;
}

D2TK_API d2tk_widget_t *
d2tk_core_widget_next(d2tk_core_t *core, d2tk_widget_t *widget)
{
	d2tk_mem_t *mem = &core->mem[core->curmem];
	const size_t ref = mem->offset;

	const size_t buf_sz = ref - widget->ref;
	const size_t body_sz = sizeof(d2tk_widget_body_t) + buf_sz;
	d2tk_widget_body_t *body = malloc(body_sz);

	// copy widget instruction buffer to cache
	if(body)
	{
		body->size = buf_sz;
		memcpy(body->buf, &mem->buf[widget->ref], buf_sz);

		// actually store in cache
		*widget->body = (uintptr_t)body;
	}

	return NULL;
}

static inline ssize_t
_d2tk_core_bbox_push(d2tk_core_t *core, bool cached, bool container,
	const d2tk_rect_t *rect)
{
	d2tk_mem_t *mem = &core->mem[core->curmem];
	const size_t ref = mem->offset;
	const size_t len = sizeof(d2tk_body_bbox_t);
	d2tk_body_t *body = _d2tk_append_request(core, len, D2TK_INSTR_BBOX);

	if(body)
	{
		body->bbox.hash = 0;
		body->bbox.cached = cached;
		body->bbox.container = container;
		body->bbox.dirty = false;
		body->bbox.clip.x0 = rect->x;
		body->bbox.clip.y0 = rect->y;
		body->bbox.clip.x1 = rect->x + rect->w;
		body->bbox.clip.y1 = rect->y + rect->h;
		body->bbox.clip.w = rect->w;
		body->bbox.clip.h = rect->h;

		core->ref.x = rect->x;
		core->ref.y = rect->y;

		_d2tk_append_advance(core, len);
		return ref;
	}

	return -1;
}

D2TK_API ssize_t
d2tk_core_bbox_push(d2tk_core_t *core, bool cached, const d2tk_rect_t *rect)
{
	return _d2tk_core_bbox_push(core, cached, false, rect);
}

D2TK_API ssize_t
d2tk_core_bbox_container_push(d2tk_core_t *core, bool cached,
	const d2tk_rect_t *rect)
{
	return _d2tk_core_bbox_push(core, cached, true, rect);
}

D2TK_API void
d2tk_core_bbox_pop(d2tk_core_t *core, ssize_t ref)
{
	d2tk_mem_t *mem = &core->mem[core->curmem];
	d2tk_com_t *com = (d2tk_com_t *)&mem->buf[ref];
	const size_t len = mem->offset - ref;

	com->size = len - sizeof(d2tk_com_t);
	// hash over instructions exclusive position
	com->body->bbox.hash = d2tk_hash(&com->body->bbox.clip.w,
		len - offsetof(d2tk_clip_t, w));

	core->ref.x = 0;
	core->ref.y = 0;
}

D2TK_API void
d2tk_core_begin_path(d2tk_core_t *core)
{
	_d2tk_append_simple(core, D2TK_INSTR_BEGIN_PATH);
}

D2TK_API void
d2tk_core_close_path(d2tk_core_t *core)
{
	_d2tk_append_simple(core, D2TK_INSTR_CLOSE_PATH);
}

D2TK_API void
d2tk_core_scissor(d2tk_core_t *core, const d2tk_rect_t *rect)
{
	const size_t len = sizeof(d2tk_body_scissor_t);
	d2tk_body_t *body = _d2tk_append_request(core, len, D2TK_INSTR_SCISSOR);

	if(body)
	{
		body->scissor.x = rect->x;
		body->scissor.y = rect->y;
		body->scissor.w = rect->w;
		body->scissor.h = rect->h;

		body->scissor.x -= core->ref.x;
		body->scissor.y -= core->ref.y;

		_d2tk_append_advance(core, len);
	}
}

D2TK_API void
d2tk_core_reset_scissor(d2tk_core_t *core)
{
	_d2tk_append_simple(core, D2TK_INSTR_RESET_SCISSOR);
}

D2TK_API void
d2tk_core_font_size(d2tk_core_t *core, d2tk_coord_t size)
{
	const size_t len = sizeof(d2tk_body_font_size_t);
	d2tk_body_t *body = _d2tk_append_request(core, len, D2TK_INSTR_FONT_SIZE);

	if(body)
	{
		body->font_size.size = size;

		_d2tk_append_advance(core, len);
	}
}

D2TK_API void
d2tk_core_font_face(d2tk_core_t *core, size_t sz, const char *face)
{
	const size_t len = sizeof(d2tk_body_font_face_t) + sz;
	d2tk_body_t *body = _d2tk_append_request(core, len, D2TK_INSTR_FONT_FACE);

	if(body)
	{
		memcpy(body->font_face.face, face, sz);
		body->font_face.face[sz] = '\0';

		_d2tk_append_advance(core, len);
	}
}

D2TK_API void
d2tk_core_text(d2tk_core_t *core, const d2tk_rect_t *rect, size_t sz,
	const char *text, d2tk_align_t align)
{
	const size_t len = sizeof(d2tk_body_text_t) + sz;
	d2tk_body_t *body = _d2tk_append_request(core, len, D2TK_INSTR_TEXT);

	if(body)
	{
		body->text.x = rect->x;
		body->text.y = rect->y;
		body->text.w = rect->w;
		body->text.h = rect->h;
		body->text.align = align;
		memcpy(body->text.text, text, sz);
		body->text.text[sz] = '\0';

		body->text.x -= core->ref.x;
		body->text.y -= core->ref.y;

		_d2tk_append_advance(core, len);
	}
}

D2TK_API void
d2tk_core_image(d2tk_core_t *core, const d2tk_rect_t *rect, size_t sz,
	const char *path, d2tk_align_t align)
{
	const size_t len = sizeof(d2tk_body_image_t) + sz;
	d2tk_body_t *body = _d2tk_append_request(core, len, D2TK_INSTR_IMAGE);

	if(body)
	{
		body->image.x = rect->x;
		body->image.y = rect->y;
		body->image.w = rect->w;
		body->image.h = rect->h;
		body->image.align = align;
		memcpy(body->image.path, path, sz);
		body->image.path[sz] = '\0';

		body->image.x -= core->ref.x;
		body->image.y -= core->ref.y;

		_d2tk_append_advance(core, len);
	}
}

D2TK_API void
d2tk_core_bitmap(d2tk_core_t *core, const d2tk_rect_t *rect, uint32_t w,
	uint32_t h, uint32_t stride, const uint32_t *argb, uint64_t rev,
	d2tk_align_t align)
{
	const size_t len = sizeof(d2tk_body_bitmap_t);
	d2tk_body_t *body = _d2tk_append_request(core, len, D2TK_INSTR_BITMAP);

	if(body)
	{
		body->bitmap.x = rect->x;
		body->bitmap.y = rect->y;
		body->bitmap.w = rect->w;
		body->bitmap.h = rect->h;
		body->bitmap.align = align;
		body->bitmap.surf.w = w;
		body->bitmap.surf.h = h;
		body->bitmap.surf.stride = stride;
		body->bitmap.surf.argb = argb;
		body->bitmap.surf.rev = rev;

		body->bitmap.x -= core->ref.x;
		body->bitmap.y -= core->ref.y;

		_d2tk_append_advance(core, len);
	}
}

D2TK_API void
d2tk_core_custom(d2tk_core_t *core, const d2tk_rect_t *rect, uint32_t size,
	const void *data, d2tk_core_custom_t custom)
{
	const size_t len = sizeof(d2tk_body_custom_t);
	d2tk_body_t *body = _d2tk_append_request(core, len, D2TK_INSTR_CUSTOM);

	if(body)
	{
		body->custom.x = rect->x;
		body->custom.y = rect->y;
		body->custom.w = rect->w;
		body->custom.h = rect->h;
		body->custom.size = size;
		body->custom.data = data;
		body->custom.custom = custom;

		body->custom.x -= core->ref.x;
		body->custom.y -= core->ref.y;

		_d2tk_append_advance(core, len);
	}
}

D2TK_API void
d2tk_core_stroke_width(d2tk_core_t *core, d2tk_coord_t width)
{
	const size_t len = sizeof(d2tk_body_stroke_width_t);
	d2tk_body_t *body = _d2tk_append_request(core, len, D2TK_INSTR_STROKE_WIDTH);

	if(body)
	{
		body->stroke_width.width = width;

		_d2tk_append_advance(core, len);
	}
}

D2TK_API int
d2tk_core_pre(d2tk_core_t *core, void *pctx)
{
	d2tk_mem_t *curmem = &core->mem[core->curmem];

	_d2tk_mem_reset(curmem);

	core->parent = d2tk_core_bbox_container_push(core, 0,
		&D2TK_RECT(0, 0, core->w, core->h));

	return core->driver->context(core->data, pctx);
}

static inline bool
_d2tk_com_equal_container(const d2tk_com_t *curcom, const d2tk_com_t *oldcom)
{
	const d2tk_body_bbox_t *curbbox = &curcom->body->bbox;
	const d2tk_body_bbox_t *oldbbox = &oldcom->body->bbox;

	if(  (curcom->instr == oldcom->instr)
		&& (curbbox->clip.x0 == oldbbox->clip.x0)
		&& (curbbox->clip.y0 == oldbbox->clip.y0) )
	{
		if(  (curcom->size == oldcom->size)
			&& (curbbox->hash == oldbbox->hash) )
		{
			return true;
		}
		else if(curbbox->container && oldbbox->container)
		{
			return true;
		}
	}

	return false;
}

static inline bool
_d2tk_com_equal(const d2tk_com_t *curcom, const d2tk_com_t *oldcom)
{
	const d2tk_body_bbox_t *curbbox = &curcom->body->bbox;
	const d2tk_body_bbox_t *oldbbox = &oldcom->body->bbox;

	if(  (curcom->instr == oldcom->instr)
		&& (curbbox->clip.x0 == oldbbox->clip.x0)
		&& (curbbox->clip.y0 == oldbbox->clip.y0)
		&& (curcom->size == oldcom->size)
		&& (curbbox->hash == oldbbox->hash) )
	{
		return true;
	}

	return false;
}

static inline void
_d2tk_diff(d2tk_core_t *core, d2tk_com_t *curcom_ref, d2tk_com_t *oldcom_ref)
{
	// look for (dis)appeared instructions
	d2tk_com_t *tmpcom = _d2tk_com_begin(curcom_ref);

	D2TK_COM_FOREACH(oldcom_ref, oldcom)
	{
		if(oldcom->instr != D2TK_INSTR_BBOX)
		{
			continue;
		}

		bool match = false;

		D2TK_COM_FOREACH_FROM(curcom_ref, tmpcom, curcom)
		{
			if(curcom->instr != D2TK_INSTR_BBOX)
			{
				continue;
			}

			// check for matching size, instruction, hash and position
			if(_d2tk_com_equal_container(curcom, oldcom))
			{
				for(d2tk_com_t *curcom2 = tmpcom;
					curcom2 != curcom;
					curcom2 = _d2tk_com_next(curcom2))
				{
					if(curcom2->instr != D2TK_INSTR_BBOX)
					{
						continue;
					}

#if D2TK_DEBUG
					d2tk_body_bbox_t *curbbox2 = &curcom2->body->bbox;

					fprintf(stderr,
						"\t   appeared (%i %i %i %i %i %i 0x%08"PRIx32")\n",
						curbbox2->clip.x0, curbbox2->clip.y0,
						curbbox2->clip.x1, curbbox2->clip.y1,
						curcom2->size, curcom2->instr,
						curbbox2->hash);
#endif

					_d2tk_bbox_mask(core, curcom2);
				}

				if(curcom->body->bbox.container && oldcom->body->bbox.container)
				{
#if D2TK_DEBUG
					fprintf(stderr, "\t   comparing nested containers\n");
#endif
					_d2tk_diff(core, curcom, oldcom);
				}

				match = true;
				tmpcom = _d2tk_com_next(curcom);

				break;
			}
		}

		if(!match)
		{
#if D2TK_DEBUG
			d2tk_body_bbox_t *oldbbox = &oldcom->body->bbox;

			fprintf(stderr,
				"\tdisappeared (%i %i %i %i %i %i 0x%08"PRIx32")\n",
				oldbbox->clip.x0, oldbbox->clip.y0,
				oldbbox->clip.x1, oldbbox->clip.y1,
				oldcom->size, oldcom->instr,
				oldbbox->hash);
#endif

			_d2tk_bbox_mask(core, oldcom);
		}
	}

	D2TK_COM_FOREACH_FROM(curcom_ref, tmpcom, curcom2)
	{
		if(curcom2->instr != D2TK_INSTR_BBOX)
		{
			continue;
		}

#if D2TK_DEBUG
		d2tk_body_bbox_t *curbbox2 = &curcom2->body->bbox;

		fprintf(stderr,
			"\t   appeared (%i %i %i %i %i %i 0x%08"PRIx32")\n",
			curbbox2->clip.x0, curbbox2->clip.y0,
			curbbox2->clip.x1, curbbox2->clip.y1,
			curcom2->size, curcom2->instr,
			curbbox2->hash);
#endif

		_d2tk_bbox_mask(core, curcom2);
	}
}

D2TK_API void
d2tk_core_post(d2tk_core_t *core)
{
	d2tk_mem_t *oldmem = &core->mem[!core->curmem];
	d2tk_mem_t *curmem = &core->mem[core->curmem];
	d2tk_bitmap_t *bitmap = &core->bitmap;

	d2tk_core_bbox_pop(core, core->parent);

	_d2tk_mem_compact(curmem);

	d2tk_com_t *curcom = _d2tk_mem_get_com(curmem);
	d2tk_com_t *oldcom = _d2tk_mem_get_com(oldmem);

	// reset num of clipping clips
	_d2tk_bitmap_reset(core);

	if(core->full_refresh)
	{
#if D2TK_DEBUG
		fprintf(stderr,
			"\tfull_refresh (%"PRIu32" %"PRIu32" %"PRIu32" %"PRIu32")\n",
			0, 0, core->w, core->h);
#endif

		_d2tk_sprites_free(core);
		_d2tk_memcaches_free(core);
	}
	else if(!_d2tk_com_equal(curcom, oldcom))
	{
		_d2tk_diff(core, curcom, oldcom);
	}

	if(bitmap->nfills || core->full_refresh)
	{
		const d2tk_clip_t *aoi = NULL;

		if(core->full_refresh)
		{
			d2tk_clip_t tmp;

			tmp.x0 = 0;
			tmp.y0 = 0;
			tmp.x1 = core->w;
			tmp.y1 = core->h;
			tmp.w = core->w;
			tmp.h = core->h;

			_d2tk_bitmap_fill(core, &tmp);
		}
		else
		{
			static d2tk_clip_t tmp;

			tmp.x0 = bitmap->x0;
			tmp.y0 = bitmap->y0;
			tmp.x1 = bitmap->x1;
			tmp.y1 = bitmap->y1;
			tmp.w = bitmap->x1 - bitmap->x0;
			tmp.h = bitmap->y1 - bitmap->y0;

			aoi = &tmp;
		}

#if D2TK_DEBUG
		fprintf(stderr, "\tnfills: %zu\n", bitmap->nfills);
#endif
		for(unsigned pass = 0; pass < 2; pass++)
		{
			core->driver->pre(core->data, core, core->w, core->h, pass);

			d2tk_com_t *curcom = _d2tk_mem_get_com(curmem);

			D2TK_COM_FOREACH(curcom, com)
			{
				d2tk_body_bbox_t *body = &com->body->bbox;

				if(pass == 0)
				{
					if(aoi && !body->dirty)
					{
						if(  ( (body->clip.x0 >= aoi->x1) || (aoi->x0 >= body->clip.x1) )
							|| ( (body->clip.y0 >= aoi->y1) || (aoi->y0 >= body->clip.y1) ) )
						{
							continue; // not in area-of-interest
						}

						if(!_d2tk_bitmap_query(core, body))
						{
							continue;
						}
					}
				}
				else if(pass == 1)
				{
					if(aoi && !body->dirty)
					{
						continue; // not in area-of-interest
					}
				}

				const d2tk_clip_t *clip = NULL;

				if(aoi)
				{
					static d2tk_clip_t tmp;

					// derive minimal intersecting rectangle
					tmp.x0 = aoi->x0 < body->clip.x0
						? body->clip.x0
						: aoi->x0;
					tmp.x1 = aoi->x1 > body->clip.x1
						? body->clip.x1
						: aoi->x1;
					tmp.y0 = aoi->y0 < body->clip.y0
						? body->clip.y0
						: aoi->y0;
					tmp.y1 = aoi->y1 > body->clip.y1
						? body->clip.y1
						: aoi->y1;
					tmp.w = tmp.x1 - tmp.x0;
					tmp.h = tmp.y1 - tmp.y0;

#if 0 // seems not to be needed
					if(pass == 0)
					{
						_d2tk_bitmap_fill(core, &tmp);
					}
#endif

					clip = &tmp;
				}

				/*
				fprintf(stderr, "=%li:%p= %i %i %i %i\n", i, (void *)body,
					body->clip.x0, body->clip.y0, body->clip.w, body->clip.h);
				if(clip)
				{
					fprintf(stderr, ":%li:%p: %i %i %i %i\n", i, (void *)body,
						clip->x0, clip->y0, clip->x1 - clip->x0, clip->y1 - clip->y0);
				}
				*/

				core->driver->process(core->data, core, com, body->clip.x0,
					body->clip.y0, clip, pass);
			}

			if(!core->driver->post(core->data, core, core->w, core->h, pass))
			{
				break; // does NOT need 2nd pass
			}
		}
	}

	core->driver->end(core->data, core, core->w, core->h);

	_d2tk_sprites_gc(core);
	_d2tk_memcaches_gc(core);

	core->full_refresh = false;
	core->curmem = !core->curmem;
}

D2TK_API d2tk_core_t *
d2tk_core_new(const d2tk_core_driver_t *driver, void *data)
{
	d2tk_core_t *core = calloc(1, sizeof(d2tk_core_t));
	if(!core)
	{
		return NULL;
	}

	core->driver = driver;
	core->data = data;

	_d2tk_mem_init(&core->mem[0], 8192);
	_d2tk_mem_init(&core->mem[1], 8192);

	{
		core->curmem = 0;

		_d2tk_mem_reset(&core->mem[core->curmem]);
		const ssize_t ref = d2tk_core_bbox_container_push(core, 0,
			&D2TK_RECT(0, 0, core->w, core->h));
		d2tk_core_bbox_pop(core, ref);
	}

	{
		core->curmem = 1;

		_d2tk_mem_reset(&core->mem[core->curmem]);
		const ssize_t ref = d2tk_core_bbox_container_push(core, 0,
			&D2TK_RECT(0, 0, core->w, core->h));
		d2tk_core_bbox_pop(core, ref);
	}

	core->curmem = 0;

	core->ttl.sprites = _D2TK_SPRITES_TTL;
	core->ttl.memcaches = _D2TK_MEMCACHES_TTL;

	return core;
}

D2TK_API void
d2tk_core_set_ttls(d2tk_core_t *core, uint32_t sprites, uint32_t memcaches)
{
	core->ttl.sprites  = sprites;
	core->ttl.memcaches = memcaches;
}

D2TK_API void
d2tk_core_free(d2tk_core_t *core)
{
	_d2tk_mem_deinit(&core->mem[0]);
	_d2tk_mem_deinit(&core->mem[1]);
	_d2tk_bitmap_deinit(&core->bitmap);
	_d2tk_sprites_free(core);
	_d2tk_memcaches_free(core);

	free(core);
}

D2TK_API void
d2tk_core_set_dimensions(d2tk_core_t *core, d2tk_coord_t w, d2tk_coord_t h)
{
	core->w = w;
	core->h = h;
	d2tk_core_set_full_refresh(core);
	_d2tk_bitmap_resize(core, w, h);
}

D2TK_API void
d2tk_core_get_dimensions(d2tk_core_t *core, d2tk_coord_t *w, d2tk_coord_t *h)
{
	if(w)
	{
		*w = core->w;
	}

	if(h)
	{
		*h = core->h;
	}
}

D2TK_API void
d2tk_core_set_full_refresh(d2tk_core_t *core)
{
	core->full_refresh = true;
}

int
d2tk_core_get_font_path(d2tk_core_t *core, const char *bundle_path,
	const char *rel_path, size_t abs_len, char *abs_path)
{
	int ret = 1;

#if D2TK_FONTCONFIG
	(void)core;
	(void)bundle_path;

	FcConfig *config = FcInitLoadConfigAndFonts();
	FcPattern *pat = FcNameParse((FcChar8 *)rel_path);
	FcConfigSubstitute(config, pat, FcMatchPattern);
	FcDefaultSubstitute(pat);

	//FcPatternPrint(pat);

	FcResult result;
	FcPattern *font = FcFontMatch(config, pat, &result);
	if(font)
	{
		FcChar8 *file = NULL;
		if(FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch)
		{
			snprintf(abs_path, abs_len, "%s", file);
			ret = 0;
		}

		FcPatternDestroy(font);
	}

	FcPatternDestroy(pat);

	FcConfigDestroy(config);
#else
	(void)core;
	snprintf(abs_path, abs_len, "%s%s.ttf", bundle_path, rel_path);
	ret = 0;
#endif

	return ret;
}
