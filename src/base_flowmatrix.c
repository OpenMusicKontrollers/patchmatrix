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
#include <string.h>

#include "base_internal.h"

typedef struct _d2tk_atom_body_flow_t d2tk_atom_body_flow_t;

struct _d2tk_atom_body_flow_t {
	d2tk_coord_t x;
	d2tk_coord_t y;
	d2tk_coord_t lx;
	d2tk_coord_t ly;
	float exponent;
	d2tk_id_t src_id;
	d2tk_id_t dst_id;
};

struct _d2tk_flowmatrix_t {
	d2tk_base_t *base;
	d2tk_id_t id;
	const d2tk_rect_t *rect;
	d2tk_atom_body_flow_t *atom_body;
	float scale;
	d2tk_coord_t cx;
	d2tk_coord_t cy;
	size_t ref;
	d2tk_coord_t w;
	d2tk_coord_t h;
	d2tk_coord_t dd;
	d2tk_coord_t r;
	d2tk_coord_t s;
	bool src_conn;
	bool dst_conn;
	d2tk_pos_t src_pos;
	d2tk_pos_t dst_pos;
};

struct _d2tk_flowmatrix_node_t {
	d2tk_flowmatrix_t *flowmatrix;
	d2tk_rect_t rect;
};

struct _d2tk_flowmatrix_arc_t {
	d2tk_flowmatrix_t *flowmatrix;
	unsigned x;
	unsigned y;
	unsigned N;
	unsigned M;
	unsigned NM;
	unsigned k;
	d2tk_coord_t c;
	d2tk_coord_t c_2;
	d2tk_coord_t c_4;
	d2tk_coord_t xo;
	d2tk_coord_t yo;
	d2tk_rect_t rect;
};

const size_t d2tk_atom_body_flow_sz = sizeof(d2tk_atom_body_flow_t);
const size_t d2tk_flowmatrix_sz = sizeof(d2tk_flowmatrix_t);
const size_t d2tk_flowmatrix_node_sz = sizeof(d2tk_flowmatrix_node_t);
const size_t d2tk_flowmatrix_arc_sz = sizeof(d2tk_flowmatrix_arc_t);

static d2tk_coord_t
_d2tk_flowmatrix_abs_x(d2tk_flowmatrix_t *flowmatrix, d2tk_coord_t rel_x)
{
	return flowmatrix->cx + rel_x * flowmatrix->scale;
}

static d2tk_coord_t
_d2tk_flowmatrix_abs_y(d2tk_flowmatrix_t *flowmatrix, d2tk_coord_t rel_y)
{
	return flowmatrix->cy + rel_y * flowmatrix->scale;
}

static void
_d2tk_flowmatrix_connect(d2tk_base_t *base, d2tk_flowmatrix_t *flowmatrix,
	const d2tk_pos_t *src_pos, const d2tk_pos_t *dst_pos)
{
	const d2tk_style_t *style = d2tk_base_get_style(base);

	d2tk_pos_t dst;
	if(!dst_pos) // connect to mouse pointer
	{
		d2tk_base_get_mouse_pos(base, &dst.x, &dst.y);
	}

	const d2tk_hash_dict_t dict [] = {
		{ flowmatrix, sizeof(d2tk_flowmatrix_t) },
		{ src_pos, sizeof(d2tk_pos_t) },
		{ dst_pos ? dst_pos : &dst, sizeof(d2tk_pos_t) },
		{ style, sizeof(d2tk_style_t) },
		{ NULL, 0 }
	};
	const uint64_t hash = d2tk_hash_dict(dict);

	d2tk_core_t *core = base->core;
	D2TK_CORE_WIDGET(core, hash, widget)
	{
		const d2tk_coord_t w = flowmatrix->w;
		const d2tk_coord_t r = flowmatrix->r;
		const d2tk_coord_t x = _d2tk_flowmatrix_abs_x(flowmatrix, src_pos->x) + w/2 + r;
		const d2tk_coord_t y = _d2tk_flowmatrix_abs_y(flowmatrix, src_pos->y);

		if(dst_pos)
		{
			dst.x = _d2tk_flowmatrix_abs_x(flowmatrix, dst_pos->x) - w/2 - r;
			dst.y = _d2tk_flowmatrix_abs_y(flowmatrix, dst_pos->y);
		}

		const d2tk_coord_t x0 = (x < dst.x) ? x : dst.x;
		const d2tk_coord_t y0 = (y < dst.y) ? y : dst.y;
		const d2tk_coord_t x1 = (x > dst.x) ? x : dst.x;
		const d2tk_coord_t y1 = (y > dst.y) ? y : dst.y;

		const d2tk_rect_t bnd = {
			.x = x0 - 1,
			.y = y0 - 1,
			.w = x1 - x0 + 2,
			.h = y1 - y0 + 2
		};

		const d2tk_triple_t triple = D2TK_TRIPLE_FOCUS;

		const size_t ref = d2tk_core_bbox_push(core, false, &bnd);

		d2tk_core_begin_path(core);
		d2tk_core_move_to(core, x, y);
		d2tk_core_line_to(core, dst.x, dst.y);
		d2tk_core_color(core, style->stroke_color[triple]);
		d2tk_core_stroke_width(core, style->border_width);
		d2tk_core_stroke(core);

		d2tk_core_bbox_pop(core, ref);
	}
}

D2TK_API d2tk_flowmatrix_t *
d2tk_flowmatrix_begin(d2tk_base_t *base, const d2tk_rect_t *rect, d2tk_id_t id,
	d2tk_flowmatrix_t *flowmatrix)
{
	memset(flowmatrix, 0x0, sizeof(d2tk_flowmatrix_t));

	flowmatrix->base = base;
	flowmatrix->id = id;
	flowmatrix->rect = rect;
	flowmatrix->atom_body = _d2tk_base_get_atom(base, id, D2TK_ATOM_FLOW, NULL);
	flowmatrix->scale = exp2f(flowmatrix->atom_body->exponent); //FIXME cache this instead
	flowmatrix->cx = flowmatrix->rect->x + flowmatrix->rect->w / 2
		- flowmatrix->atom_body->x * flowmatrix->scale;
	flowmatrix->cy = flowmatrix->rect->y + flowmatrix->rect->h / 2
		- flowmatrix->atom_body->y * flowmatrix->scale;

	flowmatrix->w = flowmatrix->scale * 150; //FIXME
	flowmatrix->h = flowmatrix->scale * 25; //FIXME
	flowmatrix->dd = flowmatrix->scale * 40; //FIXME
	flowmatrix->r = flowmatrix->scale * 4; //FIXME
	flowmatrix->s = flowmatrix->scale * 20; //FIXME

	d2tk_core_t *core = base->core;
	flowmatrix->ref = d2tk_core_bbox_container_push(core, false, flowmatrix->rect);

	return flowmatrix;
}

D2TK_API bool
d2tk_flowmatrix_not_end(d2tk_flowmatrix_t *flowmatrix)
{
	return flowmatrix ? true : false;
}

D2TK_API d2tk_flowmatrix_t *
d2tk_flowmatrix_next(d2tk_flowmatrix_t *flowmatrix)
{
	d2tk_base_t *base = flowmatrix->base;
	float *exponent = &flowmatrix->atom_body->exponent;
	const float old_exponent = *exponent;

	const d2tk_state_t state = d2tk_base_is_active_hot(base, flowmatrix->id,
		flowmatrix->rect, D2TK_FLAG_SCROLL_Y);

	if(d2tk_state_is_scroll_down(state))
	{
		*exponent -= 0.125f; //FIXME
	}
	else if(d2tk_state_is_scroll_up(state))
	{
		*exponent += 0.125f; //FIXME
	}
	else if(d2tk_state_is_motion(state))
	{
		const d2tk_coord_t adx = base->mouse.dx / flowmatrix->scale;
		const d2tk_coord_t ady = base->mouse.dy / flowmatrix->scale;

		flowmatrix->atom_body->x -= adx;
		flowmatrix->atom_body->y -= ady;
	}

	d2tk_clip_float(-2.f, exponent, 1.f); //FIXME

	if(*exponent != old_exponent)
	{
		const d2tk_coord_t ox = (base->mouse.x - flowmatrix->cx) / flowmatrix->scale;
		const d2tk_coord_t oy = (base->mouse.y - flowmatrix->cy) / flowmatrix->scale;

		const float scale = exp2f(*exponent);

		const d2tk_coord_t fx = base->mouse.x - (ox * scale);
		const d2tk_coord_t fy = base->mouse.y - (oy * scale);

		flowmatrix->atom_body->x = (flowmatrix->rect->x + flowmatrix->rect->w / 2 - fx) / scale;
		flowmatrix->atom_body->y = (flowmatrix->rect->y + flowmatrix->rect->h / 2 - fy) / scale;

		d2tk_base_set_again(base);
	}

	if(flowmatrix->src_conn)
	{
		if(flowmatrix->dst_conn)
		{
			_d2tk_flowmatrix_connect(base, flowmatrix, &flowmatrix->src_pos,
				&flowmatrix->dst_pos);
		}
		else
		{
			_d2tk_flowmatrix_connect(base, flowmatrix, &flowmatrix->src_pos, NULL);

			// invalidate dst_id
			flowmatrix->atom_body->dst_id = 0;
		}
	}

	d2tk_core_t *core = base->core;
	d2tk_core_bbox_pop(core, flowmatrix->ref);

	return NULL;
}

static void
_d2tk_flowmatrix_next_pos(d2tk_flowmatrix_t *flowmatrix, d2tk_pos_t *pos)
{
	flowmatrix->atom_body->lx += 150; //FIXME
	flowmatrix->atom_body->ly += 25; //FIXME

	pos->x = flowmatrix->atom_body->lx;
	pos->y = flowmatrix->atom_body->ly;
}

D2TK_API void
d2tk_flowmatrix_set_src(d2tk_flowmatrix_t *flowmatrix, d2tk_id_t id,
	const d2tk_pos_t *pos)
{
	flowmatrix->src_conn = true;
	flowmatrix->atom_body->src_id = id;

	if(pos)
	{
		flowmatrix->src_pos = *pos;
	}
}

D2TK_API void
d2tk_flowmatrix_set_dst(d2tk_flowmatrix_t *flowmatrix, d2tk_id_t id,
	const d2tk_pos_t *pos)
{
	flowmatrix->dst_conn = true;
	flowmatrix->atom_body->dst_id = id;

	if(pos)
	{
		flowmatrix->dst_pos = *pos;
	}
}

D2TK_API d2tk_id_t
d2tk_flowmatrix_get_src(d2tk_flowmatrix_t *flowmatrix, d2tk_pos_t *pos)
{
	if(pos)
	{
		*pos = flowmatrix->src_pos;
	}

	return flowmatrix->atom_body->src_id;
}

D2TK_API d2tk_id_t
d2tk_flowmatrix_get_dst(d2tk_flowmatrix_t *flowmatrix, d2tk_pos_t *pos)
{
	if(pos)
	{
		*pos = flowmatrix->dst_pos;
	}

	return flowmatrix->atom_body->dst_id;
}

D2TK_API d2tk_flowmatrix_node_t *
d2tk_flowmatrix_node_begin(d2tk_base_t *base, d2tk_flowmatrix_t *flowmatrix,
	d2tk_pos_t *pos, d2tk_flowmatrix_node_t *node)
{
	node->flowmatrix = flowmatrix;

	// derive initial position
	if( (pos->x == 0) && (pos->y == 0) )
	{
		_d2tk_flowmatrix_next_pos(flowmatrix, pos);
	}

	const d2tk_coord_t x = _d2tk_flowmatrix_abs_x(flowmatrix, pos->x);
	const d2tk_coord_t y = _d2tk_flowmatrix_abs_y(flowmatrix, pos->y);
	const d2tk_coord_t w = flowmatrix->w;
	const d2tk_coord_t h = flowmatrix->h;

	node->rect.x = x - w/2;
	node->rect.y = y - h/2;
	node->rect.w = w;
	node->rect.h = h;

	d2tk_core_t *core = base->core;
	d2tk_coord_t cw;
	d2tk_coord_t ch;

	d2tk_core_get_dimensions(core, &cw, &ch);
	if(  (node->rect.x >= cw)
			|| (node->rect.y >= ch)
			|| (node->rect.x <= -node->rect.w)
			|| (node->rect.y <= -node->rect.h) )
	{
		return NULL;
	}

	const d2tk_style_t *style = d2tk_base_get_style(base);

	const d2tk_hash_dict_t dict [] = {
		{ flowmatrix, sizeof(d2tk_flowmatrix_t) },
		{ pos, sizeof(d2tk_pos_t) },
		{ node, sizeof(d2tk_flowmatrix_node_t) },
		{ style, sizeof(d2tk_style_t) },
		{ NULL, 0 }
	};
	const uint64_t hash = d2tk_hash_dict(dict);

	D2TK_CORE_WIDGET(core, hash, widget)
	{
		const d2tk_coord_t r = flowmatrix->r;
		const d2tk_coord_t r2 = r*2;
		const d2tk_triple_t triple = D2TK_TRIPLE_NONE; //FIXME

		// sink connection point
		{
			const d2tk_coord_t x0 = node->rect.x - r;

			const d2tk_rect_t bnd = {
				.x = x0 - r,
				.y = y - r,
				.w = r2,
				.h = r2
			};

			const size_t ref = d2tk_core_bbox_push(core, true, &bnd);

			d2tk_core_begin_path(core);
			d2tk_core_arc(core, x0, y, r, 0, 360, true);
			d2tk_core_color(core, style->fill_color[triple]);
			d2tk_core_stroke_width(core, 0);
			d2tk_core_fill(core);

			d2tk_core_begin_path(core);
			d2tk_core_arc(core, x0, y, r, 0, 360, true);
			d2tk_core_color(core, style->stroke_color[triple]);
			d2tk_core_stroke_width(core, style->border_width);
			d2tk_core_stroke(core);

			d2tk_core_bbox_pop(core, ref);
		}

		// source connection point
		{
			const d2tk_coord_t x0 = node->rect.x + node->rect.w + r;

			const d2tk_rect_t bnd = {
				.x = x0 - r,
				.y = y - r,
				.w = r2,
				.h = r2
			};

			const size_t ref = d2tk_core_bbox_push(core, true, &bnd);

			d2tk_core_begin_path(core);
			d2tk_core_arc(core, x0, y, r, 0, 360, true);
			d2tk_core_color(core, style->fill_color[triple]);
			d2tk_core_stroke_width(core, 0);
			d2tk_core_fill(core);

			d2tk_core_begin_path(core);
			d2tk_core_arc(core, x0, y, r, 0, 360, true);
			d2tk_core_color(core, style->stroke_color[triple]);
			d2tk_core_stroke_width(core, style->border_width);
			d2tk_core_stroke(core);

			d2tk_core_bbox_pop(core, ref);
		}
	}

	return node;
}

D2TK_API bool
d2tk_flowmatrix_node_not_end(d2tk_flowmatrix_node_t *node)
{
	return node ? true : false;
}

D2TK_API d2tk_flowmatrix_node_t *
d2tk_flowmatrix_node_next(d2tk_flowmatrix_node_t *node, d2tk_pos_t *pos,
	const d2tk_state_t *state)
{
	d2tk_flowmatrix_t *flowmatrix = node->flowmatrix;
	d2tk_base_t *base = flowmatrix->base;

	if(d2tk_state_is_motion(*state))
	{
		const d2tk_coord_t adx = base->mouse.dx / flowmatrix->scale;
		const d2tk_coord_t ady = base->mouse.dy / flowmatrix->scale;

		pos->x += adx;
		pos->y += ady;

		d2tk_base_set_again(base);
	}

	return NULL;
}

D2TK_API const d2tk_rect_t *
d2tk_flowmatrix_node_get_rect(d2tk_flowmatrix_node_t *node)
{
	return &node->rect;
}

D2TK_API d2tk_flowmatrix_arc_t *
d2tk_flowmatrix_arc_begin(d2tk_base_t *base, d2tk_flowmatrix_t *flowmatrix,
	unsigned N, unsigned M, const d2tk_pos_t *src, const d2tk_pos_t *dst,
	d2tk_pos_t *pos, d2tk_flowmatrix_arc_t *arc)
{
	memset(arc, 0x0, sizeof(d2tk_flowmatrix_arc_t));

	// derive initial position
	if( (pos->x == 0) && (pos->y == 0) )
	{
		pos->x = (src->x + dst->x) / 2;
		pos->y = (src->y + dst->y) / 2;
	}

	arc->flowmatrix = flowmatrix;
	arc->x = 0;
	arc->y = 0;
	arc->N = N+1;
	arc->M = M+1;
	arc->NM = (N+1)*(M+1);
	arc->k = 0;

	const d2tk_coord_t x = _d2tk_flowmatrix_abs_x(flowmatrix, pos->x);
	const d2tk_coord_t y = _d2tk_flowmatrix_abs_y(flowmatrix, pos->y);
	const d2tk_coord_t s = flowmatrix->s;
	arc->c = M_SQRT2 * s;
	arc->c_2 = arc->c / 2;
	arc->c_4 = arc->c / 4;

	const d2tk_coord_t x0 = x - M*arc->c_2;
	const d2tk_coord_t x1 = x;
	const d2tk_coord_t x2 = x + (N - M)*arc->c_2;
	const d2tk_coord_t x3 = x + N*arc->c_2;

	const d2tk_coord_t y0 = y;
	const d2tk_coord_t y1 = y + M*arc->c_2;
	const d2tk_coord_t y2 = y + N*arc->c_2;
	const d2tk_coord_t y3 = y + (N+M)*arc->c_2;

	arc->xo = x1 - arc->c_2;
	arc->yo = y0 + arc->c_4;
	arc->rect.x = arc->xo;
	arc->rect.y = arc->yo;
	arc->rect.w = arc->c;
	arc->rect.h = arc->c_2;

	const d2tk_style_t *style = d2tk_base_get_style(base);

	const d2tk_hash_dict_t dict [] = {
		{ flowmatrix, sizeof(d2tk_flowmatrix_t) },
		{ &N, sizeof(unsigned) },
		{ &M, sizeof(unsigned) },
		{ src, sizeof(d2tk_pos_t) },
		{ dst, sizeof(d2tk_pos_t) },
		{ pos, sizeof(d2tk_pos_t) },
		{ arc, sizeof(d2tk_flowmatrix_arc_t) },
		{ style, sizeof(d2tk_style_t) },
		{ NULL, 0 }
	};
	const uint64_t hash = d2tk_hash_dict(dict);

	d2tk_core_t *core = base->core;
	D2TK_CORE_WIDGET(core, hash, widget)
	{
		const d2tk_coord_t r = flowmatrix->r;
		const d2tk_coord_t ox = flowmatrix->w/2;
		const d2tk_coord_t dd = flowmatrix->dd;
		const d2tk_triple_t triple = D2TK_TRIPLE_NONE; //FIXME
		const d2tk_coord_t b = style->border_width;
		const d2tk_coord_t b2 = b*2;

		const d2tk_coord_t xs = _d2tk_flowmatrix_abs_x(flowmatrix, src->x) + ox + r;
		const d2tk_coord_t ys = _d2tk_flowmatrix_abs_y(flowmatrix, src->y);
		const d2tk_coord_t xp = x;
		const d2tk_coord_t yp = y - r;
		const d2tk_coord_t xd = _d2tk_flowmatrix_abs_x(flowmatrix, dst->x) - ox - r;
		const d2tk_coord_t yd = _d2tk_flowmatrix_abs_y(flowmatrix, dst->y);

		// sink arc
		{
			const d2tk_coord_t x0 = xs;
			const d2tk_coord_t x1 = xs + dd;
			const d2tk_coord_t x2 = xp - dd;
			const d2tk_coord_t x3 = xp;

			const d2tk_coord_t y0 = ys;
			const d2tk_coord_t y1 = ys;
			const d2tk_coord_t y2 = yp;
			const d2tk_coord_t y3 = yp;

			d2tk_coord_t xa = INT32_MAX;
			d2tk_coord_t xb = INT32_MIN;
			if(x0 < xa) xa = x0;
			if(x1 < xa) xa = x1;
			if(x2 < xa) xa = x2;
			if(x3 < xa) xa = x3;
			if(x0 > xb) xb = x0;
			if(x1 > xb) xb = x1;
			if(x2 > xb) xb = x2;
			if(x3 > xb) xb = x3;

			d2tk_coord_t ya = INT32_MAX;
			d2tk_coord_t yb = INT32_MIN;
			if(y0 < ya) ya = y0;
			if(y1 < ya) ya = y1;
			if(y2 < ya) ya = y2;
			if(y3 < ya) ya = y3;
			if(y0 > yb) yb = y0;
			if(y1 > yb) yb = y1;
			if(y2 > yb) yb = y2;
			if(y3 > yb) yb = y3;

			const d2tk_rect_t bnd = {
				.x = xa - b,
				.y = ya - b,
				.w = xb - xa + b2,
				.h = yb - ya + b2
			};

			const size_t ref = d2tk_core_bbox_push(core, false, &bnd);

			d2tk_core_begin_path(core);
			d2tk_core_move_to(core, x0, y0);
			d2tk_core_curve_to(core, x1, y1, x2, y2, x3, y3);
			d2tk_core_color(core, style->stroke_color[triple]);
			d2tk_core_stroke_width(core, b);
			d2tk_core_stroke(core);

			d2tk_core_bbox_pop(core, ref);
		}

		// source arc
		{
			const d2tk_coord_t x0 = xp;
			const d2tk_coord_t x1 = xp + dd;
			const d2tk_coord_t x2 = xd - dd;
			const d2tk_coord_t x3 = xd;

			const d2tk_coord_t y0 = yp;
			const d2tk_coord_t y1 = yp;
			const d2tk_coord_t y2 = yd;
			const d2tk_coord_t y3 = yd;

			d2tk_coord_t xa = INT32_MAX;
			d2tk_coord_t xb = INT32_MIN;
			if(x0 < xa) xa = x0;
			if(x1 < xa) xa = x1;
			if(x2 < xa) xa = x2;
			if(x3 < xa) xa = x3;
			if(x0 > xb) xb = x0;
			if(x1 > xb) xb = x1;
			if(x2 > xb) xb = x2;
			if(x3 > xb) xb = x3;

			d2tk_coord_t ya = INT32_MAX;
			d2tk_coord_t yb = INT32_MIN;
			if(y0 < ya) ya = y0;
			if(y1 < ya) ya = y1;
			if(y2 < ya) ya = y2;
			if(y3 < ya) ya = y3;
			if(y0 > yb) yb = y0;
			if(y1 > yb) yb = y1;
			if(y2 > yb) yb = y2;
			if(y3 > yb) yb = y3;

			const d2tk_rect_t bnd = {
				.x = xa - b,
				.y = ya - b,
				.w = xb - xa + b2,
				.h = yb - ya + b2
			};

			const size_t ref = d2tk_core_bbox_push(core, false, &bnd);

			d2tk_core_begin_path(core);
			d2tk_core_move_to(core, x0, y0);
			d2tk_core_curve_to(core, x1, y1, x2, y2, x3, y3);
			d2tk_core_color(core, style->stroke_color[triple]);
			d2tk_core_stroke_width(core, b);
			d2tk_core_stroke(core);

			d2tk_core_bbox_pop(core, ref);
		}

		// matrix
		{
			const d2tk_rect_t bnd = {
				.x = x0,
				.y = y0,
				.w = x3 - x0,
				.h = y3 - y0
			};

			const size_t ref = d2tk_core_bbox_push(core, true, &bnd);

			// matrix bounding box
			d2tk_core_begin_path(core);
			d2tk_core_move_to(core, x0, y1);
			d2tk_core_line_to(core, x1, y0);
			d2tk_core_line_to(core, x3, y2);
			d2tk_core_line_to(core, x2, y3);
			d2tk_core_close_path(core);
			d2tk_core_color(core, style->fill_color[triple]);
			d2tk_core_stroke_width(core, 0);
			d2tk_core_fill(core);

			// grid lines
			for(unsigned j = 0, o = 0;
				j < M + 1;
				j++, o += arc->c_2)
			{
				d2tk_core_begin_path(core);
				d2tk_core_move_to(core, x0 + o, y1 - o);
				d2tk_core_line_to(core, x2 + o, y3 - o);
				d2tk_core_color(core, style->stroke_color[triple]);
				d2tk_core_stroke_width(core, style->border_width);
				d2tk_core_stroke(core);
			}

			// grid lines
			for(unsigned i = 0, o = 0;
				i < N + 1;
				i++, o += arc->c_2)
			{
				d2tk_core_begin_path(core);
				d2tk_core_move_to(core, x0 + o, y1 + o);
				d2tk_core_line_to(core, x1 + o, y0 + o);
				d2tk_core_color(core, style->stroke_color[triple]);
				d2tk_core_stroke_width(core, style->border_width);
				d2tk_core_stroke(core);
			}

			d2tk_core_bbox_pop(core, ref);
		}

		// connection point
		{
			const d2tk_coord_t r2 = r*2;
			const d2tk_rect_t bnd = {
				.x = xp - r,
				.y = yp - r,
				.w = r2,
				.h = r2
			};

			const size_t ref = d2tk_core_bbox_push(core, true, &bnd);

			d2tk_core_begin_path(core);
			d2tk_core_arc(core, xp, yp, r, 0, 360, true);
			d2tk_core_color(core, style->fill_color[triple]);
			d2tk_core_stroke_width(core, 0);
			d2tk_core_fill(core);

			d2tk_core_begin_path(core);
			d2tk_core_arc(core, xp, yp, r, 0, 360, true);
			d2tk_core_color(core, style->stroke_color[triple]);
			d2tk_core_stroke_width(core, style->border_width);
			d2tk_core_stroke(core);

			d2tk_core_bbox_pop(core, ref);
		}
	}

	return arc;
}

D2TK_API bool
d2tk_flowmatrix_arc_not_end(d2tk_flowmatrix_arc_t *arc)
{
	return arc->k < arc->NM - 1;
}

D2TK_API d2tk_flowmatrix_arc_t *
d2tk_flowmatrix_arc_next(d2tk_flowmatrix_arc_t *arc, d2tk_pos_t *pos,
	const d2tk_state_t *state)
{
	d2tk_flowmatrix_t *flowmatrix = arc->flowmatrix;
	d2tk_base_t *base = flowmatrix->base;

	if(d2tk_state_is_motion(*state))
	{
		const d2tk_coord_t adx = base->mouse.dx / flowmatrix->scale;
		const d2tk_coord_t ady = base->mouse.dy / flowmatrix->scale;

		pos->x += adx;
		pos->y += ady;

		d2tk_base_set_again(base);
	}

	{
		++arc->k;

		if(++arc->x % arc->N)
		{
			// nothing to do
		}
		else // overflow
		{
			arc->x = 0;
			++arc->y;
		}

		arc->rect.x = arc->xo + (arc->x - arc->y)*arc->c_2;
		arc->rect.y = arc->yo + (arc->x + arc->y)*arc->c_2;
		arc->rect.w = arc->c;
		arc->rect.h = arc->c_2;

		if(arc->y == (arc->M - 1)) // source label
		{
			arc->rect.x -= arc->rect.w*1 + arc->c_2;
			arc->rect.y -= arc->c_4;
			arc->rect.w *= 2;
		}
		else if(arc->x == (arc->N - 1)) // sink label
		{
			arc->rect.x += arc->c_2;
			arc->rect.y -= arc->c_4;
			arc->rect.w *= 2;
		}
	}

	return arc;
}

D2TK_API unsigned
d2tk_flowmatrix_arc_get_index(d2tk_flowmatrix_arc_t *arc)
{
	return arc->k;
}

D2TK_API unsigned
d2tk_flowmatrix_arc_get_index_x(d2tk_flowmatrix_arc_t *arc)
{
	return arc->x;
}

D2TK_API unsigned
d2tk_flowmatrix_arc_get_index_y(d2tk_flowmatrix_arc_t *arc)
{
	return arc->y;
}

D2TK_API const d2tk_rect_t *
d2tk_flowmatrix_arc_get_rect(d2tk_flowmatrix_arc_t *arc __attribute__((unused)))
{
	return &arc->rect;
}
