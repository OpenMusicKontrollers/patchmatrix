/*
 * Copyright (c) 2016-2020 Hanspeter Portner (dev@open-music-kontrollers.ch)
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the Artistic License 2.0 as published by
 * The Perl Foundation.
 *
 * This source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the iapplied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Artistic License 2.0 for more details.
 *
 * You should have received a copy of the Artistic License 2.0
 * along the source as a COPYING file. If not, obtain it from
 * http://www.perlfoundation.org/artistic_license_2_0.
 */

#include <patchmatrix_jack.h>
#include <patchmatrix_db.h>
#include <patchmatrix_nk.h>

const struct nk_color grid_line_color = {40, 40, 40, 255};
const struct nk_color grid_background_color = {30, 30, 30, 255};
const struct nk_color hilight_color = {200, 100, 0, 255};
const struct nk_color button_border_color = {100, 100, 100, 255};
const struct nk_color wire_color = {100, 100, 100, 255};
const struct nk_color grab_handle_color = {100, 100, 100, 255};
const struct nk_color toggle_color = {150, 150, 150, 255};

static int
_client_moveable(struct nk_context *ctx, app_t *app, client_t *client,
	struct nk_rect *bounds)
{
	struct nk_input *in = &ctx->input;
	
	const bool is_hovering = nk_input_is_mouse_hovering_rect(in, *bounds);

	if(client->moving)
	{
		if(nk_input_is_mouse_released(in, NK_BUTTON_LEFT))
		{
			client->moving = false;

#ifdef JACK_HAS_METADATA_API
			if(client->flags == (JackPortIsInput | JackPortIsOutput) )
			{
				char val [32];

				snprintf(val, 32, "%f", client->pos.x);
				jack_set_property(app->client, client->uuid, PATCHMATRIX__mainPositionX, val, XSD__float);

				snprintf(val, 32, "%f", client->pos.y);
				jack_set_property(app->client, client->uuid, PATCHMATRIX__mainPositionY, val, XSD__float);
			}
			else if(client->flags == JackPortIsInput)
			{
				char val [32];

				snprintf(val, 32, "%f", client->pos.x);
				jack_set_property(app->client, client->uuid, PATCHMATRIX__sinkPositionX, val, XSD__float);

				snprintf(val, 32, "%f", client->pos.y);
				jack_set_property(app->client, client->uuid, PATCHMATRIX__sinkPositionY, val, XSD__float);
			}
			else if(client->flags == JackPortIsOutput)
			{
				char val [32];

				snprintf(val, 32, "%f", client->pos.x);
				jack_set_property(app->client, client->uuid, PATCHMATRIX__sourcePositionX, val, XSD__float);

				snprintf(val, 32, "%f", client->pos.y);
				jack_set_property(app->client, client->uuid, PATCHMATRIX__sourcePositionY, val, XSD__float);
			}
#endif
		}
		else
		{
			client->pos.x += in->mouse.delta.x;
			client->pos.y += in->mouse.delta.y;
			bounds->x += in->mouse.delta.x;
			bounds->y += in->mouse.delta.y;

			// move connections together with client
			HASH_FOREACH(&app->conns, client_conn_itr)
			{
				client_conn_t *client_conn = *client_conn_itr;

				if(client_conn->source_client == client)
				{
					client_conn->pos.x += in->mouse.delta.x/2;
					client_conn->pos.y += in->mouse.delta.y/2;
				}

				if(client_conn->sink_client == client)
				{
					client_conn->pos.x += in->mouse.delta.x/2;
					client_conn->pos.y += in->mouse.delta.y/2;
				}
			}
		}
	}
	else if(is_hovering
		&& nk_input_is_mouse_pressed(in, NK_BUTTON_LEFT)
		&& nk_input_is_key_down(in, NK_KEY_CTRL) )
	{
		client->moving = true;
	}

	if  (is_hovering
		&& nk_input_is_mouse_pressed(in, NK_BUTTON_RIGHT) )
	{
		// consume mouse event
		in->mouse.buttons[NK_BUTTON_RIGHT].down = nk_false;
		in->mouse.buttons[NK_BUTTON_RIGHT].clicked = nk_false;

		return true;
	}

	return false;
}

static void
_client_connectors(struct nk_context *ctx, app_t *app, client_t *client,
	struct nk_vec2 dim, int is_hilighted)
{
	struct node_editor *nodedit = &app->nodedit;
	const struct nk_input *in = &ctx->input;
	struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
	const struct nk_vec2 scrolling = nodedit->scrolling;

	const float cw = 4.f * app->scale;

	struct nk_rect bounds = nk_rect(
		client->pos.x - dim.x/2 - scrolling.x,
		client->pos.y - dim.y/2 - scrolling.y,
		dim.x, dim.y);

	// output connector
	if(client->source_type & app->type)
	{
		const float cx = client->pos.x - scrolling.x + dim.x/2 + 2*cw;
		const float cy = client->pos.y - scrolling.y;
		const struct nk_rect outer = nk_rect(
			cx - cw, cy - cw,
			4*cw, 4*cw
		);

		// start linking process
		const bool has_click_body = nk_input_has_mouse_click_down_in_rect(in, NK_BUTTON_LEFT, bounds, nk_true);
		const bool has_click_handle = nk_input_has_mouse_click_down_in_rect(in, NK_BUTTON_LEFT, outer, nk_true);
		if(  ((has_click_body && !client->mixer_shm) || has_click_handle)
			&& !nk_input_is_key_down(in, NK_KEY_CTRL))
		{
			nodedit->linking.active = true;
			nodedit->linking.source_client = client;
		}

		const bool is_hovering_handle= nk_input_is_mouse_hovering_rect(in, outer);
		nk_fill_arc(canvas, cx, cy, cw, 0.f, 2*NK_PI,
			is_hilighted ? hilight_color : grab_handle_color);
		if(  (is_hovering_handle && !nodedit->linking.active)
			|| (nodedit->linking.active && (nodedit->linking.source_client == client)) )
		{
			nk_stroke_arc(canvas, cx, cy, 2*cw, 0.f, 2*NK_PI, 1.f, hilight_color);
		}

		// draw line from linked node slot to mouse position
		if(  nodedit->linking.active
			&& (nodedit->linking.source_client == client) )
		{
			struct nk_vec2 m = in->mouse.pos;

			nk_stroke_line(canvas, cx, cy, m.x, m.y, 1.f, hilight_color);
		}
	}

	// input connector
	if(client->sink_type & app->type)
	{
		const float cx = client->mixer_shm
			? client->pos.x - scrolling.x
			: client->pos.x - scrolling.x - dim.x/2 - 2*cw;
		const float cy = client->mixer_shm
			? client->pos.y - scrolling.y - dim.y/2 - 2*cw
			: client->pos.y - scrolling.y;
		const struct nk_rect outer = nk_rect(
			cx - cw, cy - cw,
			4*cw, 4*cw
		);

		const bool is_hovering_body = nk_input_is_mouse_hovering_rect(in, bounds);
		const bool is_hovering_handle = nk_input_is_mouse_hovering_rect(in, outer);
		nk_fill_arc(canvas, cx, cy, cw, 0.f, 2*NK_PI,
			is_hilighted ? hilight_color : grab_handle_color);
		if(  (is_hovering_handle || is_hovering_body)
			&& nodedit->linking.active)
		{
			nk_stroke_arc(canvas, cx, cy, 2*cw, 0.f, 2*NK_PI, 1.f, hilight_color);
		}

		if(  nk_input_is_mouse_released(in, NK_BUTTON_LEFT)
			&& (is_hovering_handle || is_hovering_body)
			&& nodedit->linking.active)
		{
			nodedit->linking.active = false;

			client_t *src = nodedit->linking.source_client;
			if(src)
			{
				client_conn_t *client_conn = _client_conn_find(app, src, client);
				if(!client_conn) // does not yet exist
					client_conn = _client_conn_add(app, src, client);
				if(client_conn)
				{
					client_conn->type |= app->type;

					if(nk_input_is_key_down(in, NK_KEY_CTRL)) // automatic connection
					{
						unsigned i = 0;
						HASH_FOREACH(&src->sources, source_port_itr)
						{
							port_t *source_port = *source_port_itr;
							if(source_port->type != app->type)
								continue;

							unsigned j = 0;
							HASH_FOREACH(&client->sinks, sink_port_itr)
							{
								port_t *sink_port = *sink_port_itr;
								if(sink_port->type != app->type)
									continue;

								bool do_connect = false;
								if(  (!source_port->designation || !sink_port->designation)
									&& (i == j) )
								{
									// try do be smart
									do_connect = true;
								}
								else if( (source_port->designation && sink_port->designation)
									&& (source_port->designation == sink_port->designation) )
								{
									// connect matching designations
									do_connect = true;
								}

								if(do_connect)
									jack_connect(app->client, source_port->name, sink_port->name);

								j++;
							}

							i++;
						}
					}
				}
			}
		}
	}
}

static void
node_editor_mixer(struct nk_context *ctx, app_t *app, client_t *client)
{
	const bool editable = (client->source_type == app->type)
		|| (client->sink_type == app->type);

	struct node_editor *nodedit = &app->nodedit;
	struct nk_input *in = &ctx->input;
	struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
	const struct nk_vec2 scrolling = nodedit->scrolling;

	mixer_shm_t *shm = client->mixer_shm;
	if(atomic_load_explicit(&shm->closing, memory_order_acquire))
		return;

	const float ps = 32.f * app->scale;
	const unsigned nx = shm->nsinks;
	const unsigned ny = shm->nsources;

	client->dim.x = nx * ps;
	client->dim.y = ny * ps;

	struct nk_rect bounds = nk_rect(
		client->pos.x - client->dim.x/2 - scrolling.x,
		client->pos.y - client->dim.y/2 - scrolling.y,
		client->dim.x, client->dim.y);

	if(_client_moveable(ctx, app, client, &bounds))
	{
		sem_post(&shm->done);
	}

	client->hovered = nk_input_is_mouse_hovering_rect(in, bounds)
		&& !nodedit->linking.active;
	const bool is_hilighted = client->hilighted || client->hovered || client->moving;

	nk_layout_space_push(ctx, nk_layout_space_rect_to_local(ctx, bounds));

	struct nk_rect body;
	const enum nk_widget_layout_states states = nk_widget(&body, ctx);
	if(states != NK_WIDGET_INVALID)
	{
		struct nk_style_button *style = &ctx->style.button;

		struct nk_color fill_col = style->hover.data.color;
		struct nk_color stroke_col = style->border_color;
		struct nk_color hilight_col = is_hilighted ? hilight_color : style->border_color;
		struct nk_color wire_col = wire_color;
		struct nk_color toggle_col = toggle_color;

		if(!editable)
		{
			fill_col.a /= 3;
			stroke_col.a /= 3;
			hilight_col.a /= 3;
			wire_col.a /= 3;
			toggle_col.a /= 3;
		}

		nk_fill_rect(canvas, body, style->rounding, fill_col);

		for(float x = ps; x < body.w; x += ps)
		{
			nk_stroke_line(canvas,
				body.x + x, body.y,
				body.x + x, body.y + body.h,
				style->border, stroke_col);
		}

		for(float y = ps; y < body.h; y += ps)
		{
			nk_stroke_line(canvas,
				body.x, body.y + y,
				body.x + body.w, body.y + y,
				style->border, stroke_col);
		}

		float x = body.x + ps/2;
		for(unsigned i = 0; i < nx; i++)
		{
			float y = body.y + ps/2;
			for(unsigned j = 0; j < ny; j++)
			{
				int32_t mBFS = atomic_load_explicit(&shm->jgains[j][i], memory_order_acquire);

				const struct nk_rect tile = nk_rect(x - ps/2, y - ps/2, ps, ps);

				const struct nk_mouse_button *btn = &in->mouse.buttons[NK_BUTTON_LEFT];;
				const bool left_mouse_down = btn->down;
				const bool left_mouse_click_in_tile = nk_input_has_mouse_click_down_in_rect(in,
					NK_BUTTON_LEFT, tile, nk_true);
				const bool mouse_hovering_over_tile = nk_input_is_mouse_hovering_rect(in, tile);

				int32_t dd = 0;

				if(editable)
				{
					if(left_mouse_down && left_mouse_click_in_tile && !client->moving)
					{
						const float dx = in->mouse.delta.x;
						const float dy = in->mouse.delta.y;
						dd = fabs(dx) > fabs(dy) ? dx : -dy;
					}
					else if(mouse_hovering_over_tile)
					{
						if(in->mouse.scroll_delta.y != 0.f) // has scrolling
						{
							dd = in->mouse.scroll_delta.y;
							in->mouse.scroll_delta.y = 0.f;
						}
					}

					if(dd != 0)
					{
#if 0
						if( (dd > 0) && (mBFS == -3600) ) // disabled
						{
							mBFS = 0; // jump to 0 dBFS
						}
						else
#endif
						{
							const bool has_shift = nk_input_is_key_down(in, NK_KEY_SHIFT);
							const float mul = has_shift ? 10.f : 100.f;
							mBFS = NK_CLAMP(-3600, mBFS + dd*mul, 3600);
						}

						atomic_store_explicit(&shm->jgains[j][i], mBFS, memory_order_release);
					}
				}

				const float dBFS = mBFS / 100.f;

				if(mouse_hovering_over_tile && !client->moving)
				{
					char tmp [32];

					const struct nk_user_font *font = ctx->style.font;

					const float fh = font->height;

					{
						const size_t tmp_len = snprintf(tmp, 32, "[%u-%u]", i+1, j+1); //FIXME use port names
						const float fw = font->width(font->userdata, font->height, tmp, tmp_len);
						const float fy = body.y + body.h + fh/2;
						const struct nk_rect body2 = {
							.x = body.x + (body.w - fw)/2,
							.y = fy,
							.w = fw,
							.h = fh
						};
						nk_draw_text(canvas, body2, tmp, tmp_len, font,
							style->normal.data.color, style->text_normal);
					}

					{
						const size_t tmp_len = snprintf(tmp, 32, "%+2.2f dBFS", dBFS);
						const float fw = font->width(font->userdata, font->height, tmp, tmp_len);
						const float fy = body.y + body.h + fh + fh/2;
						const struct nk_rect body2 = {
							.x = body.x + (body.w - fw)/2,
							.y = fy,
							.w = fw,
							.h = fh
						};
						nk_draw_text(canvas, body2, tmp, tmp_len, font,
							style->normal.data.color, style->text_normal);
					}
				}

				if(mBFS > -3600)
				{
					const float alpha = (dBFS + 36.f) / 72.f;
					const float beta = NK_PI/2;

					nk_stroke_arc(canvas,
						x, y, 10.f * app->scale,
						beta + 0.2f*NK_PI, beta + 1.8f*NK_PI,
						1.f,
						wire_col);
					nk_stroke_arc(canvas,
						x, y, 7.f * app->scale,
						beta + 0.2f*NK_PI, beta + (0.2f + alpha*1.6f)*NK_PI,
						2.f,
						toggle_col);
				}

				y += ps;
			}

			x += ps;
		}

		nk_stroke_rect(canvas, body, style->rounding, style->border, hilight_col);
	}

	_client_connectors(ctx, app, client, nk_vec2(bounds.w, bounds.h), is_hilighted);
	app->animating = true;
}

static void
node_editor_monitor(struct nk_context *ctx, app_t *app, client_t *client)
{
	if(  !(client->source_type & app->type)
		&& !(client->sink_type & app->type) )
	{
		return;
	}

	struct node_editor *nodedit = &app->nodedit;
	struct nk_input *in = &ctx->input;
	struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
	const struct nk_vec2 scrolling = nodedit->scrolling;

	monitor_shm_t *shm = client->monitor_shm;
	if(atomic_load_explicit(&shm->closing, memory_order_acquire))
		return;

	const float ps = 24.f * app->scale;
	const unsigned ny = atomic_load_explicit(&shm->nsinks, memory_order_acquire);

	client->dim.x = 6 * ps;
	client->dim.y = ny * ps;

	struct nk_rect bounds = nk_rect(
		client->pos.x - client->dim.x/2 - scrolling.x,
		client->pos.y - client->dim.y/2 - scrolling.y,
		client->dim.x, client->dim.y);

	if(_client_moveable(ctx, app, client, &bounds))
	{
		sem_post(&shm->done);
	}

	client->hovered = nk_input_is_mouse_hovering_rect(in, bounds)
		&& !nodedit->linking.active;
	const bool is_hilighted = client->hilighted || client->hovered || client->moving;

	nk_layout_space_push(ctx, nk_layout_space_rect_to_local(ctx, bounds));

	struct nk_rect body;
	const enum nk_widget_layout_states states = nk_widget(&body, ctx);
	if(states != NK_WIDGET_INVALID)
	{
		struct nk_style_button *style = &ctx->style.button;

		nk_fill_rect(canvas, body, style->rounding, style->hover.data.color);

		if(client->sink_type == TYPE_AUDIO)
		{
			for(unsigned j = 0; j < ny; j++)
			{
				const int32_t mBFS = atomic_load_explicit(&shm->jgains[j], memory_order_relaxed);
				const float dBFS = mBFS / 100.f;

				struct nk_rect orig = nk_rect(body.x, body.y + j*ps, body.w, ps);
				struct nk_rect tile = orig;
				struct nk_rect outline;
				const float mx1 = 58.f / 70.f;
				const float mx2 = 12.f / 70.f;
				const uint8_t alph = 0x7f;
				const float e = (dBFS + 64.f) / 70.f;
				const float peak = NK_CLAMP(0.f, e, 1.f);

				{
					const float dbfs = NK_MIN(peak, mx1);
					const uint8_t dcol = 0xff * dbfs / mx1;
					const struct nk_color left = nk_rgba(0x00, 0xff, 0xff, alph);
					const struct nk_color bottom = left;
					const struct nk_color right = nk_rgba(dcol, 0xff, 0xff-dcol, alph);
					const struct nk_color top = right;

					const float ox = ctx->style.font->height/2 + ctx->style.property.border + ctx->style.property.padding.x;
					const float oy = ctx->style.property.border + ctx->style.property.padding.y;
					tile.x += ox;
					tile.y += oy;
					tile.w -= 2*ox;
					tile.h -= 2*oy;
					outline = tile;
					tile.w *= dbfs;

					nk_fill_rect_multi_color(canvas, tile, left, top, right, bottom);
				}

				// > 6dBFS
				if(peak > mx1)
				{
					const float dbfs = peak- mx1;
					const uint8_t dcol = 0xff * dbfs / mx2;
					const struct nk_color left = nk_rgba(0xff, 0xff, 0x00, alph);
					const struct nk_color bottom = left;
					const struct nk_color right = nk_rgba(0xff, 0xff - dcol, 0x00, alph);
					const struct nk_color top = right;

					tile= outline;
					tile.x += tile.w * mx1;
					tile.w *= dbfs;
					nk_fill_rect_multi_color(canvas, tile, left, top, right, bottom);
				}

				// draw 6dBFS lines from -60 to +6
				for(unsigned i = 4; i <= 70; i += 6)
				{
					const bool is_zero = (i == 64);
					const float dx = outline.w * i / 70.f;

					const float x0 = outline.x + dx;
					const float y0 = is_zero ? orig.y + 2.f : outline.y;

					const float border = (is_zero ? 2.f : 1.f) * ctx->style.window.group_border;

					const float x1 = x0;
					const float y1 = is_zero ? orig.y + orig.h - 2.f : outline.y + outline.h;

					nk_stroke_line(canvas, x0, y0, x1, y1, border, ctx->style.window.group_border_color);
				}

				nk_stroke_rect(canvas, outline, 0.f, ctx->style.window.group_border, ctx->style.window.group_border_color);
			}
		}
		else if(client->sink_type == TYPE_MIDI)
		{
			for(unsigned j = 0; j < ny; j++)
			{
				const int32_t cvel = atomic_load_explicit(&shm->jgains[j], memory_order_relaxed);
				const float vel = cvel / 100.f;

				struct nk_rect orig = nk_rect(body.x, body.y + j*ps, body.w, ps);
				struct nk_rect tile = orig;
				struct nk_rect outline;
				const float mx1 = 1.f;
				const uint8_t alph = 0x7f;
				const float e = vel / 127.f;
				const float peak = NK_CLAMP(0.f, e, 1.f);

				{
					const float dbfs = NK_MIN(peak, mx1);
					const uint8_t dcol = 0xff * dbfs / mx1;
					const struct nk_color left = nk_rgba(0x00, 0xff, 0xff, alph);
					const struct nk_color bottom = left;
					const struct nk_color right = nk_rgba(dcol, 0xff, 0xff-dcol, alph);
					const struct nk_color top = right;

					const float ox = ctx->style.font->height/2 + ctx->style.property.border + ctx->style.property.padding.x;
					const float oy = ctx->style.property.border + ctx->style.property.padding.y;
					tile.x += ox;
					tile.y += oy;
					tile.w -= 2*ox;
					tile.h -= 2*oy;
					outline = tile;
					tile.w *= dbfs;

					nk_fill_rect_multi_color(canvas, tile, left, top, right, bottom);
				}

				// draw lines
				for(unsigned i = 0; i <= 127; i += 16)
				{
					const float dx = outline.w * i / 127.f;

					const float x0 = outline.x + dx;
					const float y0 = outline.y;

					const float border = 1.f * ctx->style.window.group_border;

					const float x1 = x0;
					const float y1 = outline.y + outline.h;

					nk_stroke_line(canvas, x0, y0, x1, y1, border, ctx->style.window.group_border_color);
				}

				nk_stroke_rect(canvas, outline, 0.f, ctx->style.window.group_border, ctx->style.window.group_border_color);
			}
		}

		nk_stroke_rect(canvas, body, style->rounding, style->border,
			is_hilighted ? hilight_color : style->border_color);
	}

	_client_connectors(ctx, app, client, nk_vec2(bounds.w, bounds.h), is_hilighted);
	app->animating = true;
}

static unsigned
_client_num_sources(client_t *client, port_type_t type)
{
	if(client->source_type & type)
	{
		unsigned num = 0;

		HASH_FOREACH(&client->sources, port_itr)
		{
			port_t *port = *port_itr;

			if(port->type & type)
				num += 1;
		}

		return num;
	}

	return 0;
}

static unsigned
_client_num_sinks(client_t *client, port_type_t type)
{
	if(client->sink_type & type)
	{
		unsigned num = 0;

		HASH_FOREACH(&client->sinks, port_itr)
		{
			port_t *port = *port_itr;

			if(port->type & type)
				num += 1;
		}

		return num;
	}

	return 0;
}

static void
node_editor_client(struct nk_context *ctx, app_t *app, client_t *client)
{
	const bool editable = (client->source_type & app->type)
		|| (client->sink_type & app->type);

	struct node_editor *nodedit = &app->nodedit;
	const struct nk_input *in = &ctx->input;
	struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
	const struct nk_vec2 scrolling = nodedit->scrolling;

	client->dim.x = 200.f * app->scale;
	client->dim.y = app->dy;

	struct nk_rect bounds = nk_rect(
		client->pos.x - client->dim.x/2 - scrolling.x,
		client->pos.y - client->dim.y/2 - scrolling.y,
		client->dim.x, client->dim.y);

	if(_client_moveable(ctx, app, client, &bounds))
	{
		// nothing
	}

	client->hovered = nk_input_is_mouse_hovering_rect(in, bounds)
		&& !nodedit->linking.active;
	const bool is_hilighted = client->hilighted || client->hovered || client->moving;

	nk_layout_space_push(ctx, nk_layout_space_rect_to_local(ctx, bounds));

	struct nk_rect body;
	const enum nk_widget_layout_states states = nk_widget(&body, ctx);
	if(states != NK_WIDGET_INVALID)
	{
		struct nk_style_button *style = &ctx->style.button;
		const struct nk_user_font *font = ctx->style.font;

		struct nk_color fill_col = style->hover.data.color;
		struct nk_color stroke_col = is_hilighted ? hilight_color : style->border_color;

		if(!editable)
		{
			fill_col.a /= 3;
			stroke_col.a /= 3;
		}

		nk_fill_rect(canvas, body, style->rounding, fill_col);
		nk_stroke_rect(canvas, body, style->rounding, style->border, stroke_col);

		const float fh = font->height;
		const float fy = body.y + (body.h - fh)/2;
		{
			const char *client_name = client->pretty_name ? client->pretty_name : client->name;
			const size_t client_name_len = strlen(client_name);
			const float fw = font->width(font->userdata, font->height, client_name, client_name_len);
			const struct nk_rect body2 = {
				.x = body.x + (body.w - fw)/2,
				.y = fy,
				.w = fw,
				.h = fh
			};
			nk_draw_text(canvas, body2, client_name, client_name_len, font,
				style->normal.data.color, style->text_normal);
		}

		const unsigned nsources = _client_num_sources(client, app->type);
		const unsigned nsinks = _client_num_sinks(client, app->type);

		if(nsources)
		{
			char nums [32];
			snprintf(nums, 32, "%u", nsources);

			const size_t nums_len = strlen(nums);
			const float fw = font->width(font->userdata, font->height, nums, nums_len);
			const struct nk_rect body2 = {
				.x = body.x + body.w - fw - 4.f,
				.y = fy,
				.w = fw,
				.h = fh
			};
			nk_draw_text(canvas, body2, nums, nums_len, font,
				style->normal.data.color, style->text_normal);
		}

		if(nsinks)
		{
			char nums [32];
			snprintf(nums, 32, "%u", nsinks);

			const size_t nums_len = strlen(nums);
			const float fw = font->width(font->userdata, font->height, nums, nums_len);
			const struct nk_rect body2 = {
				.x = body.x + 4.f,
				.y = fy,
				.w = fw,
				.h = fh
			};
			nk_draw_text(canvas, body2, nums, nums_len, font,
				style->normal.data.color, style->text_normal);
		}
	}

	_client_connectors(ctx, app, client, nk_vec2(bounds.w, bounds.h), is_hilighted);
}

static void
node_editor_client_conn(struct nk_context *ctx, app_t *app,
	client_conn_t *client_conn, port_type_t port_type)
{
	if(  (client_conn->type != app->type)
		&& !(client_conn->type & port_type) )
	{
		return;
	}

	struct node_editor *nodedit = &app->nodedit;
	struct nk_input *in = &ctx->input;
	struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
	const struct nk_vec2 scrolling = nodedit->scrolling;

	client_t *src = client_conn->source_client;
	client_t *snk = client_conn->sink_client;

	if(!src || !snk)
		return;

	const unsigned nx = _client_num_sources(client_conn->source_client, port_type);
	const unsigned ny = _client_num_sinks(client_conn->sink_client, port_type);

	if( (nx == 0) || (ny == 0) )
	{
		return;
	}

	const float ps = 16.f * app->scale;
	const float pw = nx * ps;
	const float ph = ny * ps;
	struct nk_rect bounds = nk_rect(
		client_conn->pos.x - scrolling.x - pw/2,
		client_conn->pos.y - scrolling.y - ph/2,
		pw, ph
	);

	const int is_hovering = nk_input_is_mouse_hovering_rect(in, bounds)
		&& !nodedit->linking.active;

	if(client_conn->moving)
	{
		if(nk_input_is_mouse_released(in, NK_BUTTON_LEFT))
		{
			client_conn->moving = false;
		}
		else
		{
			client_conn->pos.x += in->mouse.delta.x;
			client_conn->pos.y += in->mouse.delta.y;
			bounds.x += in->mouse.delta.x;
			bounds.y += in->mouse.delta.y;
		}
	}
	else if(is_hovering
		&& nk_input_is_mouse_pressed(in, NK_BUTTON_LEFT)
		&& nk_input_is_key_down(in, NK_KEY_CTRL) )
	{
		client_conn->moving = true;
	}
	else if(is_hovering
		&& nk_input_is_mouse_pressed(in, NK_BUTTON_RIGHT) )
	{
		// consume mouse event
		in->mouse.buttons[NK_BUTTON_RIGHT].down = nk_false;
		in->mouse.buttons[NK_BUTTON_RIGHT].clicked = nk_false;

		unsigned count = 0;
		HASH_FOREACH(&client_conn->conns, port_conn_itr)
		{
			port_conn_t *port_conn = *port_conn_itr;

			if( (port_conn->source_port->type & app->type) && (port_conn->sink_port->type & app->type) )
			{
				jack_disconnect(app->client, port_conn->source_port->name, port_conn->sink_port->name);
				count += 1;
			}
		}

		if(count == 0) // is empty matrix, demask for current type
			client_conn->type &= ~(app->type);
	}

	const bool is_hilighted = client_conn->source_client->hovered
		|| client_conn->sink_client->hovered
		|| is_hovering || client_conn->moving;

	if(is_hilighted)
	{
		client_conn->source_client->hilighted = true;
		client_conn->sink_client->hilighted = true;
	}

	const float cs = 4.f * app->scale;

	{
		const float cx = client_conn->pos.x - scrolling.x;
		const float cxr = cx + pw/2;
		const float cy = client_conn->pos.y - scrolling.y;
		const float cyl = cy - ph/2;
		const struct nk_color col = is_hilighted ? hilight_color : grab_handle_color;

		const float l0x = src->pos.x - scrolling.x + src->dim.x/2 + cs*2;
		const float l0y = src->pos.y - scrolling.y;
		const float l1x = snk->mixer_shm
			? snk->pos.x - scrolling.x
			: snk->pos.x - scrolling.x - snk->dim.x/2 - cs*2;
		const float l1y = snk->mixer_shm
			? snk->pos.y - scrolling.y - snk->dim.y/2 - cs*2
			: snk->pos.y - scrolling.y;

		const float bend = 50.f * app->scale;
		nk_stroke_curve(canvas,
			l0x, l0y,
			l0x + bend, l0y,
			cx, cyl - bend,
			cx, cyl,
			1.f, col);
		nk_stroke_curve(canvas,
			cxr, cy,
			cxr + bend, cy,
			snk->mixer_shm ? l1x : l1x - bend, snk->mixer_shm ? l1y - bend : l1y,
			l1x, l1y,
			1.f, col);

		nk_fill_arc(canvas, cx, cyl, cs, 2*M_PI/2, 4*M_PI/2, col);
		nk_fill_arc(canvas, cxr, cy, cs, 3*M_PI/2, 5*M_PI/2, col);
	}

	nk_layout_space_push(ctx, nk_layout_space_rect_to_local(ctx, bounds));

	struct nk_rect body;
	const enum nk_widget_layout_states states = nk_widget(&body, ctx);
	if(states != NK_WIDGET_INVALID)
	{
		struct nk_style_button *style = &ctx->style.button;

		nk_fill_rect(canvas, body, style->rounding, style->normal.data.color);

		for(float x = ps; x < body.w; x += ps)
		{
			nk_stroke_line(canvas,
				body.x + x, body.y,
				body.x + x, body.y + body.h,
				style->border, style->border_color);
		}

		for(float y = ps; y < body.h; y += ps)
		{
			nk_stroke_line(canvas,
				body.x, body.y + y,
				body.x + body.w, body.y + y,
				style->border, style->border_color);
		}

		nk_stroke_rect(canvas, body, style->rounding, style->border,
			is_hilighted ? hilight_color : style->border_color);

		float x = body.x + ps/2;
		HASH_FOREACH(&client_conn->source_client->sources, source_port_itr)
		{
			port_t *source_port = *source_port_itr;

			if(!(source_port->type & port_type))
				continue;

			float y = body.y + ps/2;
			HASH_FOREACH(&client_conn->sink_client->sinks, sink_port_itr)
			{
				port_t *sink_port = *sink_port_itr;

				if(!(sink_port->type & port_type))
					continue;

				port_conn_t *port_conn = _port_conn_find(client_conn, source_port, sink_port);

				if(port_conn)
				{
					const bool is_automation = !strcmp(sink_port->short_name, "automation");

					if(is_automation)
					{
						nk_stroke_arc(canvas, x, y, cs, 0.f, 2*NK_PI, 1.f, toggle_color);
					}
					else // !is_automation
					{
						nk_fill_arc(canvas, x, y, cs, 0.f, 2*NK_PI, toggle_color);
					}
				}

				const struct nk_rect tile = nk_rect(x - ps/2, y - ps/2, ps, ps);

				if(  nk_input_is_mouse_hovering_rect(in, tile)
					&& is_hovering
					&& !client_conn->moving)
				{
					const char *source_name = source_port->pretty_name
						? source_port->pretty_name
						: source_port->short_name;

					const char *sink_name = sink_port->pretty_name
						? sink_port->pretty_name
						: sink_port->short_name;

					char tmp [128];
					const size_t tmp_len = snprintf(tmp, 128,
						"%s || %s", source_name, sink_name);

					const struct nk_user_font *font = ctx->style.font;

					const float fh = font->height;
					const float fy = body.y + body.h + fh/2;
					const float fw = font->width(font->userdata, font->height, tmp, tmp_len);
					const struct nk_rect body2 = {
						.x = body.x + (body.w - fw)/2,
						.y = fy,
						.w = fw,
						.h = fh
					};
					nk_draw_text(canvas, body2, tmp, tmp_len, font,
						style->normal.data.color, style->text_normal);

					float dd = 0.f;
					if(in->mouse.scroll_delta.y != 0.f) // has scrolling
					{
						dd = in->mouse.scroll_delta.y;
						in->mouse.scroll_delta.y = 0.f;
					}

					if(nk_input_is_mouse_pressed(in, NK_BUTTON_LEFT) || (dd != 0.f) )
					{
						if(port_conn)
							jack_disconnect(app->client, source_port->name, sink_port->name);
						else
							jack_connect(app->client, source_port->name, sink_port->name);
					}
				}

				y += ps;
			}

			x += ps;
		}
	}
}

static inline void
_expose(struct nk_context *ctx, struct nk_rect wbounds, void *data)
{
	app_t *app = data;

	app->animating = false;

	app->scale = nk_pugl_get_scale(&app->win);
	app->dy = 20.f * app->scale;

	const struct nk_input *in = &ctx->input;
	struct node_editor *nodedit = &app->nodedit;

	const char *window_name = "base";
	if(nk_begin(ctx, window_name, wbounds, NK_WINDOW_NO_SCROLLBAR))
	{
		struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);

		nk_menubar_begin(ctx);
		{
			struct nk_style_button *style = &ctx->style.button;

#ifdef JACK_HAS_METADATA_API
			nk_layout_row_dynamic(ctx, app->dy, 4);
#else
			nk_layout_row_dynamic(ctx, app->dy, 2);
#endif
			const bool is_audio = app->type == TYPE_AUDIO;
			if(is_audio)
				nk_style_push_color(ctx, &style->border_color, hilight_color);
			if(nk_button_image_label(ctx, app->icons.audio, port_labels[TYPE_AUDIO], NK_TEXT_RIGHT))
				app->type = TYPE_AUDIO;
			if(is_audio)
				nk_style_pop_color(ctx);

			const bool is_midi = app->type == TYPE_MIDI;
			if(is_midi)
				nk_style_push_color(ctx, &style->border_color, hilight_color);
			if(nk_button_image_label(ctx, app->icons.midi, port_labels[TYPE_MIDI], NK_TEXT_RIGHT))
				app->type = TYPE_MIDI;
			if(is_midi)
				nk_style_pop_color(ctx);

#ifdef JACK_HAS_METADATA_API
			const bool is_cv = app->type == TYPE_CV;
			if(is_cv)
				nk_style_push_color(ctx, &style->border_color, hilight_color);
			if(nk_button_image_label(ctx, app->icons.cv, port_labels[TYPE_CV], NK_TEXT_RIGHT))
				app->type = TYPE_CV;
			if(is_cv)
				nk_style_pop_color(ctx);

			const bool is_osc = app->type == TYPE_OSC;
			if(is_osc)
				nk_style_push_color(ctx, &style->border_color, hilight_color);
			if(nk_button_image_label(ctx, app->icons.osc, port_labels[TYPE_OSC], NK_TEXT_RIGHT))
				app->type = TYPE_OSC;
			if(is_osc)
				nk_style_pop_color(ctx);
#endif
		}
		nk_menubar_end(ctx);

		const struct nk_rect total_space = nk_window_get_content_region(ctx);
		const float total_h = total_space.h
			- app->dy
			- 2*ctx->style.window.group_padding.y;

		/* allocate complete window space */
		nk_layout_space_begin(ctx, NK_STATIC, total_h,
			_hash_size(&app->clients) + _hash_size(&app->conns));
		{
			const struct nk_rect old_clip = canvas->clip;
			const struct nk_rect space_bounds= nk_layout_space_bounds(ctx);
			nk_push_scissor(canvas, space_bounds);

			// window content scrolling
			if(  nk_input_is_mouse_hovering_rect(in, space_bounds)
				&& nk_input_is_mouse_down(in, NK_BUTTON_MIDDLE))
			{
				nodedit->scrolling.x -= in->mouse.delta.x;
				nodedit->scrolling.y -= in->mouse.delta.y;
			}

			const struct nk_vec2 scrolling = nodedit->scrolling;

			{
				/* display grid */
				struct nk_rect ssize = nk_layout_space_bounds(ctx);
				ssize.h -= ctx->style.window.group_padding.y;
				const float grid_size = 28.0f * app->scale;

				nk_fill_rect(canvas, ssize, 0.f, grid_background_color);

				for(float x = fmod(ssize.x - scrolling.x, grid_size);
					x < ssize.w;
					x += grid_size)
				{
					nk_stroke_line(canvas, x + ssize.x, ssize.y, x + ssize.x, ssize.y + ssize.h,
						1.0f, grid_line_color);
				}

				for(float y = fmod(ssize.y - scrolling.y, grid_size);
					y < ssize.h;
					y += grid_size)
				{
					nk_stroke_line(canvas, ssize.x, y + ssize.y, ssize.x + ssize.w, y + ssize.y,
						1.0f, grid_line_color);
				}
			}

			HASH_FOREACH(&app->clients, client_itr)
			{
				client_t *client = *client_itr;

				if(client->mixer_shm)
					node_editor_mixer(ctx, app, client);
				else if(client->monitor_shm)
					node_editor_monitor(ctx, app, client);
				else
					node_editor_client(ctx, app, client);

				client->hilighted = false;
			}

			HASH_FOREACH(&app->conns, client_conn_itr)
			{
				client_conn_t *client_conn = *client_conn_itr;

				node_editor_client_conn(ctx, app, client_conn, app->type);
			}

			/* reset linking connection */
			if(  nodedit->linking.active
				&& nk_input_is_mouse_released(in, NK_BUTTON_LEFT))
			{
				nodedit->linking.active = false;
			}

			// contextual menu
			if(
#ifdef JACK_HAS_METADATA_API
				(app->type != TYPE_OSC) && (app->type != TYPE_CV) &&
#endif
				nk_contextual_begin(ctx, 0, nk_vec2(100, 220), nk_window_get_bounds(ctx)))
			{
				nk_layout_row_dynamic(ctx, app->dy, 1);
				if(nk_contextual_item_label(ctx, "Mixer 1x1", NK_TEXT_LEFT))
					_mixer_spawn(app, 1, 1);
				if(nk_contextual_item_label(ctx, "Mixer 2x2", NK_TEXT_LEFT))
					_mixer_spawn(app, 2, 2);
				if(nk_contextual_item_label(ctx, "Mixer 4x4", NK_TEXT_LEFT))
					_mixer_spawn(app, 4, 4);
				if(nk_contextual_item_label(ctx, "Mixer 8x8", NK_TEXT_LEFT))
					_mixer_spawn(app, 8, 8);
				if(nk_contextual_item_label(ctx, "Monitor x1", NK_TEXT_LEFT))
					_monitor_spawn(app, 1);
				if(nk_contextual_item_label(ctx, "Monitor x2", NK_TEXT_LEFT))
					_monitor_spawn(app, 2);
				if(nk_contextual_item_label(ctx, "Monitor x4", NK_TEXT_LEFT))
					_monitor_spawn(app, 4);
				if(nk_contextual_item_label(ctx, "Monitor x8", NK_TEXT_LEFT))
					_monitor_spawn(app, 8);

				nk_contextual_end(ctx);
			}

			nk_push_scissor(canvas, old_clip);
		}
		nk_layout_space_end(ctx);

		{
			nk_layout_row_dynamic(ctx, app->dy, 6);
			const int32_t buffer_size = nk_propertyi(ctx, "BufferSize: ", 1, app->buffer_size, 48000, 1, 0);
			if(buffer_size != app->buffer_size)
			{
				const bool lower = buffer_size < app->buffer_size;

				int32_t bufsz = 1;

				while(bufsz < buffer_size)
					bufsz <<= 1;

				if(lower)
					bufsz >>= 1;

				jack_set_buffer_size (app->client, bufsz);
			}

			nk_labelf(ctx, NK_TEXT_CENTERED, "SampleRate: %"PRIi32, app->sample_rate);

			if(nk_button_label(ctx,
				app->freewheel ? "FreeWheel: true" : "FreeWheel: false"))
			{
				jack_set_freewheel(app->client, !app->freewheel);
			}

			nk_labelf(ctx, NK_TEXT_CENTERED, "RealTime: %s", app->realtime? "true" : "false");

			char tmp [32];
			snprintf(tmp, 32, "XRuns: %"PRIi32, app->xruns);
			if(nk_button_label(ctx, tmp))
			{
				app->xruns = 0;
			}

			nk_label(ctx, "PatchMatrix: "PATCHMATRIX_VERSION, NK_TEXT_RIGHT);
		}
	}
	nk_end(ctx);
}

static struct nk_image
_icon_load(app_t *app, const char *path)
{
	return nk_pugl_icon_load(&app->win, path);
}

static void
_icon_unload(app_t *app, struct nk_image img)
{
	nk_pugl_icon_unload(&app->win, img);
}

int
_ui_init(app_t *app)
{
	// UI
	nk_pugl_config_t *cfg = &app->win.cfg;
	cfg->width = 1280;
	cfg->height = 720;
	cfg->resizable = true;
	cfg->ignore = false;
	cfg->class = "PatchMatrix";
	cfg->title = "PatchMatrix";
	cfg->parent = 0;
	cfg->threads = true;
	cfg->data = app;
	cfg->expose = _expose;
	cfg->font.face = PATCHMATRIX_DATA_DIR"/Cousine-Regular.ttf";
	cfg->font.size = 13;

	app->type = TYPE_AUDIO;
	app->designation = DESIGNATION_NONE;

	XInitThreads(); // for nk_pugl_async_redisplay

	nk_pugl_init(&app->win);
	nk_pugl_show(&app->win);

	// adjust styling
	struct nk_style *style = &app->win.ctx.style;
	style->button.border_color = button_border_color;

	app->icons.audio= _icon_load(app, PATCHMATRIX_DATA_DIR"audio.png");
	app->icons.midi = _icon_load(app, PATCHMATRIX_DATA_DIR"midi.png");
#ifdef JACK_HAS_METADATA_API
	app->icons.cv = _icon_load(app, PATCHMATRIX_DATA_DIR"cv.png");
	app->icons.osc = _icon_load(app, PATCHMATRIX_DATA_DIR"osc.png");
#endif

	return 0;
}

void
_ui_deinit(app_t *app)
{
	_icon_unload(app, app->icons.audio);
	_icon_unload(app, app->icons.midi);
#ifdef JACK_HAS_METADATA_API
	_icon_unload(app, app->icons.cv);
	_icon_unload(app, app->icons.osc);
#endif

	nk_pugl_hide(&app->win);
	nk_pugl_shutdown(&app->win);
}

void
_ui_signal(app_t *app)
{
	if(!atomic_load_explicit(&app->done, memory_order_acquire))
		nk_pugl_async_redisplay(&app->win);
}
