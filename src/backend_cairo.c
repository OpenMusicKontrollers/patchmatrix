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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <cairo.h>
#include <cairo-ft.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough="
#pragma GCC diagnostic ignored "-Wshift-negative-value"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#pragma GCC diagnostic pop

#include "core_internal.h"
#include <d2tk/backend.h>
#include <d2tk/hash.h>

typedef enum _sprite_type_t {
	SPRITE_TYPE_NONE = 0,
	SPRITE_TYPE_SURF = 1,
	SPRITE_TYPE_FONT = 2
} sprite_type_t;

typedef struct _d2tk_backend_cairo_t d2tk_backend_cairo_t;

struct _d2tk_backend_cairo_t {
	cairo_t *pctx;
	cairo_t *ctx;
	char *bundle_path;
	FT_Library library;
	cairo_pattern_t *pat;
	d2tk_coord_t w;
	d2tk_coord_t h;
	cairo_surface_t *surf;
};

static void
d2tk_cairo_free(void *data)
{
	d2tk_backend_cairo_t *backend = data;

	if(backend->surf)
	{
		cairo_surface_destroy(backend->surf);
	}

	if(backend->ctx)
	{
		cairo_destroy(backend->ctx);
	}

	FT_Done_FreeType(backend->library);
	free(backend->bundle_path);
	free(backend);
}

static void *
d2tk_cairo_new(const char *bundle_path)
{
	d2tk_backend_cairo_t *backend = calloc(1, sizeof(d2tk_backend_cairo_t));
	if(!backend)
	{
		fprintf(stderr, "calloc failed\n");
		return NULL;
	}

	backend->bundle_path = strdup(bundle_path);
	FT_Init_FreeType(&backend->library);

	return backend;
}

static int
d2tk_cairo_context(void *data, void *pctx)
{
	d2tk_backend_cairo_t *backend = data;

	if(!pctx)
	{
		fprintf(stderr, "invalid cairo context\n");
		return 1;
	}

	backend->pctx = pctx;

	return 0;
}

static inline void
d2tk_cairo_pre(void *data, d2tk_core_t *core __attribute((unused)),
	d2tk_coord_t w, d2tk_coord_t h, unsigned pass)
{
	d2tk_backend_cairo_t *backend = data;

	if(pass == 0) // is this 1st pass ?
	{
		return;
	}

	if(!backend->ctx || !backend->surf || (w != backend->w) || (h != backend->h) )
	{
		cairo_surface_t *psurf = cairo_get_target(backend->pctx);

		if(backend->surf)
		{
			cairo_surface_destroy(backend->surf);
			backend->surf = NULL;
		}

		if(backend->ctx)
		{
			cairo_destroy(backend->ctx);
			backend->ctx = NULL;
		}

		backend->surf = cairo_surface_create_similar_image(psurf,
			CAIRO_FORMAT_ARGB32, w, h);
		backend->ctx = cairo_create(backend->surf);

		backend->w = w;
		backend->h = h;
	}

	cairo_t *ctx = backend->ctx;

	cairo_save(ctx);

	{
		d2tk_rect_t rect;
		uint32_t *pixels = d2tk_core_get_pixels(core, &rect);

		cairo_surface_t *surf = cairo_image_surface_create_for_data(
			(uint8_t *)pixels, CAIRO_FORMAT_ARGB32, w, h, w*sizeof(uint32_t));
		//FIXME reuse/update surfaces

		cairo_rectangle(ctx, rect.x, rect.y, rect.w, rect.h);
		cairo_clip(ctx);

		cairo_new_sub_path(ctx);
		cairo_set_source_surface(ctx, surf, 0, 0);
		cairo_paint(ctx);

		cairo_surface_finish(surf);
		cairo_surface_destroy(surf);
	}
}

static inline bool
d2tk_cairo_post(void *data, d2tk_core_t *core __attribute__((unused)),
	d2tk_coord_t w __attribute__((unused)), d2tk_coord_t h __attribute__((unused)),
	unsigned pass)
{
	d2tk_backend_cairo_t *backend = data;
	cairo_t *ctx = backend->ctx;

	if(pass == 0) // is this 1st pass ?
	{
		return true; // do enter 2nd pass
	}

#ifdef D2TK_DEBUG //FIXME needs multiple buffers to work
	{
		d2tk_rect_t rect;
		uint32_t *pixels = d2tk_core_get_pixels(core, &rect);

		// brighten up the pixels for proper hilighting
		for(d2tk_coord_t y = 0, Y = 0; y < h; y++, Y+=w)
		{
			for(d2tk_coord_t x = 0; x < w; x++)
			{
				if(pixels[Y + x])
				{
					pixels[Y + x] = 0x5f005f5f; // premultiplied ARGB
				}
			}
		}

		cairo_surface_t *surf = cairo_image_surface_create_for_data(
			(uint8_t *)pixels, CAIRO_FORMAT_ARGB32, w, h, w*sizeof(uint32_t));
		//FIXME reuse/update surface

		cairo_rectangle(ctx, 0, 0, w, h);
		cairo_clip(ctx);

		cairo_new_sub_path(ctx);
		cairo_set_source_surface(ctx, surf, 0, 0);
		cairo_paint(ctx);

		cairo_surface_finish(surf);
		cairo_surface_destroy(surf);
	}
#endif

	cairo_restore(ctx);

	return false; // do NOT enter 3rd pass
}

static inline void
d2tk_cairo_end(void *data, d2tk_core_t *core __attribute__((unused)),
	d2tk_coord_t w, d2tk_coord_t h)
{
	d2tk_backend_cairo_t *backend = data;
	cairo_t *ctx = backend->pctx;

	cairo_surface_flush(backend->surf);

	// copy to parent surface
	cairo_rectangle(ctx, 0, 0, w, h);
	cairo_clip(ctx);

	cairo_new_sub_path(ctx);
	cairo_set_source_surface(ctx, backend->surf, 0, 0);
	cairo_paint(ctx);
}

static inline void
_d2tk_cairo_buf_free(void *data)
{
	void *buf = data;

	free(buf);
}

static inline void
_d2tk_cairo_img_free(void *data)
{
	uint8_t *pixels = data;

	stbi_image_free(pixels);
}

static void
_d2tk_cairo_font_face_destroy(cairo_font_face_t *face)
{
	while(cairo_font_face_get_reference_count(face) > 1)
	{
		cairo_font_face_destroy(face);
	}
	cairo_font_face_destroy(face);
}

static inline void
d2tk_cairo_sprite_free(void *data __attribute__((unused)),
	uint8_t type, uintptr_t body)
{
	switch((sprite_type_t)type)
	{
		case SPRITE_TYPE_SURF:
		{
			cairo_surface_t *surf = (cairo_surface_t *)body;

			cairo_surface_finish(surf);
			cairo_surface_destroy(surf);
		} break;
		case SPRITE_TYPE_FONT:
		{
			cairo_font_face_t *face = (cairo_font_face_t *)body;

			_d2tk_cairo_font_face_destroy(face);
		} break;
		case SPRITE_TYPE_NONE:
		{
			// nothing to do
		} break;
	}
}

static inline void
_d2tk_cairo_free_font_face(void *data)
{
	FT_Face face = data;

	FT_Done_Face(face);
}

static inline void
_d2tk_cairo_surf_draw(cairo_t *ctx, cairo_surface_t *surf, d2tk_coord_t xo,
	d2tk_coord_t yo, d2tk_align_t align, const d2tk_rect_t *rect)
{
	const int W = cairo_image_surface_get_width(surf);
	const int H = cairo_image_surface_get_height(surf);

	d2tk_coord_t w = W;
	d2tk_coord_t h = H;
	float scale = 1.f;

	if(h != rect->h)
	{
		scale = (float)rect->h / h;
		w *= scale;
		h = rect->h;
	}

	if(w > rect->w)
	{
		const float scale_t = (float)rect->w / w;
		scale *= scale_t;
		h *= scale_t;
		w = rect->w;
	}

	d2tk_coord_t x = rect->x + xo;
	d2tk_coord_t y = rect->y + yo;

	if(align & D2TK_ALIGN_LEFT)
	{
		x += 0;
	}
	else if(align & D2TK_ALIGN_CENTER)
	{
		x += rect->w / 2;
		x -= w / 2;
	}
	else if(align & D2TK_ALIGN_RIGHT)
	{
		x += rect->w;
		x -= w;
	}

	if(align & D2TK_ALIGN_TOP)
	{
		y += 0;
	}
	else if(align & D2TK_ALIGN_MIDDLE)
	{
		y += rect->h / 2;
		y -= h / 2;
	}
	else if(align & D2TK_ALIGN_BOTTOM)
	{
		y += rect->h;
		y -= h;
	}

	const float scale_1 = 1.f / scale;
	cairo_matrix_t matrix;
	cairo_matrix_init_scale(&matrix, scale_1, scale_1);
	cairo_matrix_translate(&matrix, -x, -y);

	cairo_rectangle(ctx, x, y, w, h);
	cairo_clip(ctx);

	cairo_new_sub_path(ctx);
	cairo_set_source_surface(ctx, surf, 0, 0);
	cairo_pattern_set_matrix(cairo_get_source(ctx), &matrix);
	cairo_paint(ctx);
}

static inline void
d2tk_cairo_process(void *data, d2tk_core_t *core, const d2tk_com_t *com,
	d2tk_coord_t xo, d2tk_coord_t yo, const d2tk_clip_t *clip, unsigned pass)
{
	d2tk_backend_cairo_t *backend = data;
	cairo_t *ctx = backend->ctx;

	const d2tk_instr_t instr = com->instr;
	switch(instr)
	{
		case D2TK_INSTR_LINE_TO:
		{
			const d2tk_body_line_to_t *body = &com->body->line_to;

			cairo_line_to(ctx, body->x + xo, body->y + yo);
		} break;
		case D2TK_INSTR_MOVE_TO:
		{
			const d2tk_body_move_to_t *body = &com->body->move_to;

			cairo_move_to(ctx, body->x + xo, body->y + yo);
		} break;
		case D2TK_INSTR_RECT:
		{
			const d2tk_body_rect_t *body = &com->body->rect;

			cairo_rectangle(ctx, body->x + xo, body->y + yo, body->w, body->h);
		} break;
		case D2TK_INSTR_ROUNDED_RECT:
		{
			const d2tk_body_rounded_rect_t *body = &com->body->rounded_rect;

			const d2tk_coord_t x = body->x + xo;
			const d2tk_coord_t y = body->y + yo;
			const d2tk_coord_t w = body->w;
			const d2tk_coord_t h = body->h;
			const d2tk_coord_t r = body->r;

			if(r > 0)
			{
				static const float mul = M_PI / 180.0;

				cairo_new_sub_path(ctx);
				cairo_arc(ctx, x + w - r, y + r, r, -90 * mul, 0 * mul);
				cairo_arc(ctx, x + w - r, y + h - r, r, 0 * mul, 90 * mul);
				cairo_arc(ctx, x + r, y + h - r, r, 90 * mul, 180 * mul);
				cairo_arc(ctx, x + r, y + r, r, 180 * mul, 270 * mul);
				cairo_close_path(ctx);
			}
			else
			{
				cairo_rectangle(ctx, x, y, w, h);
			}
		} break;
		case D2TK_INSTR_ARC:
		{
			const d2tk_body_arc_t *body = &com->body->arc;

			static const float mul = M_PI / 180;
			const float a = body->a * mul;
			const float b = body->b * mul;

			if(body->cw)
			{
				cairo_arc(ctx, body->x + xo, body->y + yo, body->r, a, b);
			}
			else
			{
				cairo_arc_negative(ctx, body->x + xo, body->y + yo, body->r, a, b);
			}
		} break;
		case D2TK_INSTR_CURVE_TO:
		{
			const d2tk_body_curve_to_t *body = &com->body->curve_to;

			cairo_curve_to(ctx,
				body->x1 + xo, body->y1 + yo,
				body->x2 + xo, body->y2 + yo,
				body->x3 + xo, body->y3 + yo);
		} break;
		case D2TK_INSTR_COLOR:
		{
			const d2tk_body_color_t *body = &com->body->color;

			const float r = ( (body->rgba >> 24) & 0xff) * 0x1p-8;
			const float g = ( (body->rgba >> 16) & 0xff) * 0x1p-8;
			const float b = ( (body->rgba >>  8) & 0xff) * 0x1p-8;
			const float a = ( (body->rgba >>  0) & 0xff) * 0x1p-8;

			cairo_set_source_rgba(ctx, r, g, b, a);
		} break;
		case D2TK_INSTR_LINEAR_GRADIENT:
		{
			const d2tk_body_linear_gradient_t *body = &com->body->linear_gradient;

			const float r0 = ( (body->rgba[0] >> 24) & 0xff) * 0x1p-8;
			const float g0 = ( (body->rgba[0] >> 16) & 0xff) * 0x1p-8;
			const float b0 = ( (body->rgba[0] >>  8) & 0xff) * 0x1p-8;
			const float a0 = ( (body->rgba[0] >>  0) & 0xff) * 0x1p-8;

			const float r1 = ( (body->rgba[1] >> 24) & 0xff) * 0x1p-8;
			const float g1 = ( (body->rgba[1] >> 16) & 0xff) * 0x1p-8;
			const float b1 = ( (body->rgba[1] >>  8) & 0xff) * 0x1p-8;
			const float a1 = ( (body->rgba[1] >>  0) & 0xff) * 0x1p-8;

			const d2tk_coord_t x0 = body->p[0].x + xo;
			const d2tk_coord_t y0 = body->p[0].y + yo;
			const d2tk_coord_t x1 = body->p[1].x + xo;
			const d2tk_coord_t y1 = body->p[1].y + yo;

			assert(backend->pat == NULL);

			backend->pat = cairo_pattern_create_linear(x0, y0, x1, y1);
			cairo_pattern_add_color_stop_rgba(backend->pat, 0.f, r0, g0, b0, a0);
			cairo_pattern_add_color_stop_rgba(backend->pat, 1.f, r1, g1, b1, a1);

			cairo_set_source(ctx, backend->pat);
		} break;
		case D2TK_INSTR_ROTATE:
		{
			const d2tk_body_rotate_t *body = &com->body->rotate;

			static const float mul = M_PI / 180;
			const float rad = body->deg * mul;

			cairo_rotate(ctx, rad);
		} break;
		case D2TK_INSTR_STROKE:
		{
			cairo_stroke(ctx);
		} break;
		case D2TK_INSTR_FILL:
		{
			cairo_fill(ctx);

			if(backend->pat)
			{
				cairo_pattern_destroy(backend->pat);
				backend->pat = NULL;
			}
		} break;
		case D2TK_INSTR_SAVE:
		{
			cairo_save(ctx);
		} break;
		case D2TK_INSTR_RESTORE:
		{
			cairo_restore(ctx);
		} break;
		case D2TK_INSTR_BBOX:
		{
			const d2tk_body_bbox_t *body = &com->body->bbox;

			if(pass == 0)
			{
				if(body->cached)
				{
					uintptr_t *sprite = d2tk_core_get_sprite(core, body->hash, SPRITE_TYPE_SURF);
					assert(sprite);

					if(!*sprite)
					{
#ifdef D2TK_DEBUG
						//fprintf(stderr, "\tcreating sprite\n");
#endif
						const size_t stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32,
							body->clip.w);
						const size_t bufsz = stride * body->clip.h;
						void *buf = calloc(1, bufsz);
						cairo_surface_t *surf = cairo_image_surface_create_for_data(buf,
							CAIRO_FORMAT_ARGB32, body->clip.w, body->clip.h, stride);
						assert(surf);

						const cairo_user_data_key_t key = { 0 };
						cairo_surface_set_user_data(surf, &key, buf, _d2tk_cairo_buf_free);

						d2tk_backend_cairo_t backend2 = *backend;
						backend2.ctx = cairo_create(surf);

						D2TK_COM_FOREACH_CONST(com, bbox)
						{
							d2tk_cairo_process(&backend2, core, bbox, 0, 0, clip, pass);
						}

						cairo_surface_flush(surf);
						cairo_destroy(backend2.ctx);

						*sprite = (uintptr_t)surf;
					}
					else
					{
#ifdef D2TK_DEBUG
					//fprintf(stderr, "\texisting sprite\n");
#endif
					}
				}
				else // !body->cached
				{
					// render directly
					D2TK_COM_FOREACH_CONST(com, bbox)
					{
						d2tk_cairo_process(backend, core, bbox, body->clip.x0, body->clip.y0, clip, pass);
					}
				}
			}
			else if(pass == 1)
			{
				cairo_save(ctx);
				if(clip)
				{
					cairo_rectangle(ctx, clip->x0, clip->y0, clip->w, clip->h);
					cairo_clip(ctx);
				}

				if(body->cached)
				{
					uintptr_t *sprite = d2tk_core_get_sprite(core, body->hash, SPRITE_TYPE_SURF);
					assert(sprite && *sprite);

					cairo_surface_t *surf = (cairo_surface_t *)*sprite;
					assert(surf);

					// paint pre-rendered sprite
					cairo_new_sub_path(ctx);
					cairo_set_source_surface(ctx, surf, body->clip.x0, body->clip.y0);
					cairo_paint(ctx);
				}
				else // !body->cached
				{
					// render directly
					D2TK_COM_FOREACH_CONST(com, bbox)
					{
						d2tk_cairo_process(backend, core, bbox, body->clip.x0, body->clip.y0, clip, pass);
					}
				}

				cairo_restore(ctx);
			}
		} break;
		case D2TK_INSTR_BEGIN_PATH:
		{
			cairo_new_sub_path(ctx);
		} break;
		case D2TK_INSTR_CLOSE_PATH:
		{
			cairo_close_path(ctx);
		} break;
		case D2TK_INSTR_SCISSOR:
		{
			const d2tk_body_scissor_t *body = &com->body->scissor;

			cairo_rectangle(ctx, body->x + xo, body->y + yo, body->w, body->h);
			cairo_clip(ctx);
		} break;
		case D2TK_INSTR_RESET_SCISSOR:
		{
			cairo_reset_clip(ctx);
		} break;
		case D2TK_INSTR_FONT_FACE:
		{
			const d2tk_body_font_face_t *body = &com->body->font_face;

			const uint64_t hash = d2tk_hash(body->face, strlen(body->face));
			uintptr_t *sprite = d2tk_core_get_sprite(core, hash, SPRITE_TYPE_FONT);
			assert(sprite);

			if(!*sprite)
			{
				char ft_path [1024];
				d2tk_core_get_font_path(core, backend->bundle_path, body->face,
					sizeof(ft_path), ft_path);

				FT_Face ft_face = NULL;
				FT_New_Face(backend->library, ft_path, 0, &ft_face);
				assert(ft_face);

				cairo_font_face_t *face = cairo_ft_font_face_create_for_ft_face(ft_face, 0);
				const cairo_user_data_key_t key = { 0 };
				cairo_font_face_set_user_data(face, &key, ft_face, _d2tk_cairo_free_font_face);

				*sprite = (uintptr_t)face;
			}

			cairo_font_face_t *face = (cairo_font_face_t *)*sprite;
			assert(face);

			cairo_set_font_face(ctx, face);
		} break;
		case D2TK_INSTR_FONT_SIZE:
		{
			const d2tk_body_font_size_t *body = &com->body->font_size;

			cairo_set_font_size(ctx, body->size);
		} break;
		case D2TK_INSTR_TEXT:
		{
			const d2tk_body_text_t *body = &com->body->text;

			cairo_text_extents_t extents;
			cairo_text_extents(ctx, body->text, &extents);
			int32_t x = -extents.x_bearing;

			if(body->align & D2TK_ALIGN_LEFT)
			{
				x += body->x;
			}
			else if(body->align & D2TK_ALIGN_CENTER)
			{
				x += body->x + body->w / 2;
				x -= extents.width / 2;
			}
			else if(body->align & D2TK_ALIGN_RIGHT)
			{
				x += body->x + body->w;
				x -= extents.width;
			}

			cairo_font_extents_t font_extents;
			cairo_font_extents(ctx, &font_extents);
			double y = 0;

			if(body->align & D2TK_ALIGN_TOP)
			{
				y += body->y + font_extents.ascent;
			}
			else if(body->align & D2TK_ALIGN_MIDDLE)
			{
				y += body->y + body->h / 2 + font_extents.descent;
			}
			else if(body->align & D2TK_ALIGN_BOTTOM)
			{
				y += body->y + body->h;
			}

			cairo_move_to(ctx, x + xo, y + yo);
			cairo_show_text(ctx, body->text);
		} break;
		case D2TK_INSTR_IMAGE:
		{
			const d2tk_body_image_t *body = &com->body->image;

			const uint64_t hash = d2tk_hash(body->path, strlen(body->path));
			uintptr_t *sprite = d2tk_core_get_sprite(core, hash, SPRITE_TYPE_SURF);
			assert(sprite);

			if(!*sprite)
			{
				char *img_path = NULL;
				assert(asprintf(&img_path, "%s%s", backend->bundle_path, body->path) != -1);
				assert(img_path);

				int W, H, N;
				stbi_set_unpremultiply_on_load(1);
				stbi_convert_iphone_png_to_rgb(1);
				uint8_t *pixels = stbi_load(img_path, &W, &H, &N, 4);
				free(img_path);
				assert(pixels );

				// bitswap and premultiply pixel data
				for(unsigned i = 0; i < W*H*sizeof(uint32_t); i += sizeof(uint32_t))
				{
					// get alpha channel
					const uint8_t a = pixels[i+3];

					// premultiply with alpha channel
					const uint8_t r = ( (uint16_t)pixels[i+0] * a ) >> 8;
					const uint8_t g = ( (uint16_t)pixels[i+1] * a ) >> 8;
					const uint8_t b = ( (uint16_t)pixels[i+2] * a ) >> 8;

					// merge and byteswap to correct endianness
					uint32_t *pix = (uint32_t *)&pixels[i];
					*pix = (a << 24) | (r << 16) | (g << 8) | b;
				}

				cairo_surface_t *surf = cairo_image_surface_create_for_data(pixels,
					CAIRO_FORMAT_ARGB32, W, H, W*sizeof(uint32_t));

				const cairo_user_data_key_t key = { 0 };
				cairo_surface_set_user_data(surf, &key, pixels, _d2tk_cairo_img_free);

				*sprite = (uintptr_t)surf;
			}

			cairo_surface_t *surf = (cairo_surface_t *)*sprite;
			assert(surf);

			_d2tk_cairo_surf_draw(ctx, surf, xo, yo, body->align,
				&D2TK_RECT(body->x, body->y, body->w, body->h));
		} break;
		case D2TK_INSTR_BITMAP:
		{
			const d2tk_body_bitmap_t *body = &com->body->bitmap;

			const uint64_t hash = d2tk_hash(&body->surf, sizeof(body->surf));
			uintptr_t *sprite = d2tk_core_get_sprite(core, hash, SPRITE_TYPE_SURF);
			assert(sprite);

			if(!*sprite)
			{
				cairo_surface_t *surf = cairo_image_surface_create_for_data(
					(uint8_t *)body->surf.argb, CAIRO_FORMAT_ARGB32,
					body->surf.w, body->surf.h, body->surf.stride);

				*sprite = (uintptr_t)surf;
			}

			cairo_surface_t *surf = (cairo_surface_t *)*sprite;
			assert(surf);

			_d2tk_cairo_surf_draw(ctx, surf, xo, yo, body->align,
				&D2TK_RECT(body->x, body->y, body->w, body->h));
		} break;
		case D2TK_INSTR_CUSTOM:
		{
			const d2tk_body_custom_t *body = &com->body->custom;

			const uint64_t hash = d2tk_hash(body->data, body->size);
			uintptr_t *sprite = d2tk_core_get_sprite(core, hash, SPRITE_TYPE_SURF);
			assert(sprite);

			if(!*sprite)
			{
				const size_t stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32,
					body->w);
				const size_t bufsz = stride * body->h;
				void *buf = calloc(1, bufsz);
				cairo_surface_t *surf = cairo_image_surface_create_for_data(buf,
					CAIRO_FORMAT_ARGB32, body->w, body->h, stride);
				assert(surf);

				const cairo_user_data_key_t key = { 0 };
				cairo_surface_set_user_data(surf, &key, buf, _d2tk_cairo_buf_free);

				cairo_t *ctx2 = cairo_create(surf);

				body->custom(ctx2, body->size, body->data);

				cairo_surface_flush(surf);
				cairo_destroy(ctx2);

				*sprite = (uintptr_t)surf;
			}

			cairo_surface_t *surf = (cairo_surface_t *)*sprite;
			assert(surf);

			_d2tk_cairo_surf_draw(ctx, surf, xo, yo, D2TK_ALIGN_LEFT | D2TK_ALIGN_TOP,
				&D2TK_RECT(body->x, body->y, body->w, body->h));
		} break;
		case D2TK_INSTR_STROKE_WIDTH:
		{
			const d2tk_body_stroke_width_t *body = &com->body->stroke_width;

			cairo_set_line_width(ctx, body->width);
		} break;
		default:
		{
			fprintf(stderr, "%s: unknown command (%i)\n", __func__, com->instr);
		} break;
	}
}

const d2tk_core_driver_t d2tk_core_driver = {
	.new = d2tk_cairo_new,
	.free = d2tk_cairo_free,
	.context = d2tk_cairo_context,
	.pre = d2tk_cairo_pre,
	.process = d2tk_cairo_process,
	.post = d2tk_cairo_post,
	.end = d2tk_cairo_end,
	.sprite_free = d2tk_cairo_sprite_free
};
