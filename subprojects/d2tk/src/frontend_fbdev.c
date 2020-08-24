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

#include <fcntl.h>
#include <linux/fb.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>
#include <errno.h>
#include <inttypes.h>

#include <libevdev/libevdev.h>
#include <libudev.h>
#include <libinput.h>

#include <cairo.h>

#include "core_internal.h"
#include <d2tk/frontend_fbdev.h>

#include <d2tk/backend.h>

struct _d2tk_frontend_t {
	const d2tk_fbdev_config_t *config;
	bool done;
	struct udev *udev;
	struct libinput *li;
	struct {
		int fb;
	} fd;
	uint8_t *data;
	size_t screensize;
	struct fb_var_screeninfo vinfo;
	struct fb_fix_screeninfo finfo;
	struct {
		int32_t x;
		int32_t y;
	} mouse;
	struct {
		char character;
	} kbd;
	d2tk_base_t *base;
	void *ctx;
	cairo_t *cr;
};

static int
_open_restricted(const char *path, int flags, void *data __attribute__((unused)))
{
	const int fd = open(path, flags);

	return (fd < 0)
		? -errno
		: fd;
}

static void
_close_restricted(int fd, void *data __attribute__((unused)))
{
	close(fd);
}

static const struct libinput_interface iface = {
	.open_restricted = _open_restricted,
	.close_restricted = _close_restricted
};

static int
_d2tk_frontend_sync(d2tk_frontend_t *fbdev)
{
	int dummy = 0;

	if(ioctl(fbdev->fd.fb, FBIO_WAITFORVSYNC, &dummy))
	{
		fprintf(stderr, "Error waiting for VSYNC\n");
		return -1;
	}

	return 0;
}

static void
_d2tk_frontend_destroy(void *data)
{
	d2tk_frontend_t *fbdev = data;

	if(fbdev == NULL)
	{
		return;
	}

	munmap(fbdev->data, fbdev->screensize);
	fbdev->data = NULL;

	close(fbdev->fd.fb);
	libinput_unref(fbdev->li);
	udev_unref(fbdev->udev);
}

static cairo_surface_t *
_d2tk_frontend_create(d2tk_frontend_t *fbdev, const char *fb_device)
{
	cairo_surface_t *surface;

	fbdev->udev = udev_new();
	if(!fbdev->udev)
	{
		fprintf(stderr, "Error: udev_new\n");
		goto handle_allocate_error;
	}

	fbdev->li = libinput_udev_create_context(&iface, NULL, fbdev->udev);
	if(!fbdev->li)
	{
		fprintf(stderr, "Error: libinput_udev_create_context\n");
		goto handle_allocate_error;
	}
	libinput_udev_assign_seat(fbdev->li, "seat0");

	// Open the file for reading and writing
	fbdev->fd.fb = open(fb_device, O_RDWR);
	if(fbdev->fd.fb == -1) {
		fprintf(stderr, "Error: cannot open framebuffer fbdev\n");
		goto handle_allocate_error;
	}

	// Get variable screen information
	if(ioctl(fbdev->fd.fb, FBIOGET_VSCREENINFO, &fbdev->vinfo) == -1) {
		fprintf(stderr, "Error: reading variable information\n");
		goto handle_ioctl_error;
	}

	// Get fixed screen information
	if(ioctl(fbdev->fd.fb, FBIOGET_FSCREENINFO, &fbdev->finfo) == -1) {
		fprintf(stderr, "Error reading fixed information\n");
		goto handle_ioctl_error;
	}

	// Map the fbdev to memory
	fbdev->data = (uint8_t *)mmap(0, fbdev->finfo.smem_len,
			PROT_READ | PROT_WRITE, MAP_SHARED,
			fbdev->fd.fb, 0);
	if((intptr_t)fbdev->data == -1) {
		fprintf(stderr, "Error: failed to map framebuffer fbdev to memory\n");
		goto handle_ioctl_error;
	}


	/* Create the cairo surface which will be used to draw to */
	surface = cairo_image_surface_create_for_data(fbdev->data,
			CAIRO_FORMAT_RGB24,
			fbdev->vinfo.xres_virtual,
			fbdev->vinfo.yres_virtual,
			fbdev->finfo.smem_len / fbdev->vinfo.yres_virtual);

	cairo_surface_set_user_data(surface, NULL, fbdev,
			&_d2tk_frontend_destroy);

	return surface;

handle_ioctl_error:
	close(fbdev->fd.fb);
	libinput_unref(fbdev->li);
	udev_unref(fbdev->udev);
handle_allocate_error:
	free(fbdev);
	exit(1);
}

static inline void
_d2tk_frontend_close(d2tk_frontend_t *fbdev)
{
	fbdev->done = true;
}

static inline void
_d2tk_frontend_expose(d2tk_frontend_t *fbdev)
{
	d2tk_base_t *base = fbdev->base;

	d2tk_coord_t w;
	d2tk_coord_t h;
	d2tk_base_get_dimensions(base, &w, &h);

	do
	{
		if(d2tk_base_pre(base, fbdev->cr) == 0)
		{
			fbdev->config->expose(fbdev->config->data, w, h);

			d2tk_base_post(base);
		}
	} while(d2tk_base_get_again(base));

	_d2tk_frontend_sync(fbdev);
}

D2TK_API int
d2tk_frontend_step(d2tk_frontend_t *fbdev)
{
	libinput_dispatch(fbdev->li);

	struct libinput_event *ev;
	while ( ( ev = libinput_get_event(fbdev->li) ) )
	{
		switch(libinput_event_get_type(ev))
		{
			case LIBINPUT_EVENT_NONE:
			{
				fprintf(stdout, "got no event\n");
			} break;

			case LIBINPUT_EVENT_DEVICE_ADDED:
			case LIBINPUT_EVENT_DEVICE_REMOVED:
			{
				fprintf(stdout, "got device event\n");
			} break;

			case LIBINPUT_EVENT_KEYBOARD_KEY:
			{
				struct libinput_event_keyboard *evk =
					libinput_event_get_keyboard_event(ev);

				const bool state = libinput_event_keyboard_get_key_state(evk)
					== LIBINPUT_KEY_STATE_PRESSED
					? true
					: false;

				const uint32_t keycode = libinput_event_keyboard_get_key(evk);

				switch(keycode)
				{
					case KEY_LEFTSHIFT:
					case KEY_RIGHTSHIFT:
					{
						d2tk_base_set_modmask(fbdev->base, D2TK_MODMASK_SHIFT, state);
					} break;
					case KEY_LEFTCTRL:
					case KEY_RIGHTCTRL:
					{
						d2tk_base_set_modmask(fbdev->base, D2TK_MODMASK_CTRL, state);
					} break;
					case KEY_LEFTALT:
					case KEY_RIGHTALT:
					{
						d2tk_base_set_modmask(fbdev->base, D2TK_MODMASK_ALT, state);
					} break;

					case KEY_LEFT:
					{
						d2tk_base_set_keymask(fbdev->base, D2TK_KEYMASK_LEFT, state);
					} break;
					case KEY_RIGHT:
					{
						d2tk_base_set_keymask(fbdev->base, D2TK_KEYMASK_RIGHT, state);
					} break;
					case KEY_UP:
					{
						d2tk_base_set_keymask(fbdev->base, D2TK_KEYMASK_UP, state);
					} break;
					case KEY_DOWN:
					{
						d2tk_base_set_keymask(fbdev->base, D2TK_KEYMASK_DOWN, state);
					} break;

					case KEY_ENTER:
					case KEY_KPENTER:
					{
						d2tk_base_set_keymask(fbdev->base, D2TK_KEYMASK_ENTER, state);
					} break;
					case KEY_DELETE:
					{
						d2tk_base_set_keymask(fbdev->base, D2TK_KEYMASK_DEL, state);
					} break;
					case KEY_BACKSPACE:
					{
						d2tk_base_set_keymask(fbdev->base, D2TK_KEYMASK_BACKSPACE, state);
					} break;

					case KEY_MINUS:
					case KEY_KPMINUS:
					{
						if(state)
						{
							fbdev->kbd.character = '-';
						}
					} break;
					case KEY_KPPLUS:
					{
						if(state)
						{
							fbdev->kbd.character = '+';
						}
					} break;

					case KEY_1:
					case KEY_KP1:
					{
						if(state)
						{
							fbdev->kbd.character = '1';
						}
					} break;
					case KEY_2:
					case KEY_KP2:
					{
						if(state)
						{
							fbdev->kbd.character = '2';
						}
					} break;
					case KEY_3:
					case KEY_KP3:
					{
						if(state)
						{
							fbdev->kbd.character = '3';
						}
					} break;
					case KEY_4:
					case KEY_KP4:
					{
						if(state)
						{
							fbdev->kbd.character = '4';
						}
					} break;
					case KEY_5:
					case KEY_KP5:
					{
						if(state)
						{
							fbdev->kbd.character = '5';
						}
					} break;
					case KEY_6:
					case KEY_KP6:
					{
						if(state)
						{
							fbdev->kbd.character = '6';
						}
					} break;
					case KEY_7:
					case KEY_KP7:
					{
						if(state)
						{
							fbdev->kbd.character = '7';
						}
					} break;
					case KEY_8:
					case KEY_KP8:
					{
						if(state)
						{
							fbdev->kbd.character = '8';
						}
					} break;
					case KEY_9:
					case KEY_KP9:
					{
						if(state)
						{
							fbdev->kbd.character = '9';
						}
					} break;
					case KEY_0:
					case KEY_KP0:
					{
						if(state)
						{
							fbdev->kbd.character = '0';
						}
					} break;

					case KEY_Y:
					{
						if(state)
						{
							fbdev->kbd.character = 'y';
						}
					} break;
					case KEY_Z:
					{
						if(state)
						{
							fbdev->kbd.character = 'z';
						}
					} break;
				}

				if(fbdev->kbd.character)
				{
					d2tk_base_append_utf8(fbdev->base, fbdev->kbd.character);
					fbdev->kbd.character = 0;
				}
			} break;

			case LIBINPUT_EVENT_POINTER_MOTION:
			{
				struct libinput_event_pointer *evp =
					libinput_event_get_pointer_event(ev);

				fbdev->mouse.x += libinput_event_pointer_get_dx(evp);
				fbdev->mouse.y += libinput_event_pointer_get_dy(evp);

				d2tk_clip_int32(0, &fbdev->mouse.x, fbdev->vinfo.xres_virtual);
				d2tk_clip_int32(0, &fbdev->mouse.y, fbdev->vinfo.yres_virtual);

				d2tk_base_set_mouse_pos(fbdev->base, fbdev->mouse.x, fbdev->mouse.y);
			} break;
			case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
			{
				struct libinput_event_pointer *evp =
					libinput_event_get_pointer_event(ev);

				fbdev->mouse.x = libinput_event_pointer_get_absolute_x_transformed(evp,
					fbdev->vinfo.xres_virtual);
				fbdev->mouse.y = libinput_event_pointer_get_absolute_y_transformed(evp,
					fbdev->vinfo.yres_virtual);

				d2tk_clip_int32(0, &fbdev->mouse.x, fbdev->vinfo.xres_virtual);
				d2tk_clip_int32(0, &fbdev->mouse.y, fbdev->vinfo.yres_virtual);

				d2tk_base_set_mouse_pos(fbdev->base, fbdev->mouse.x, fbdev->mouse.y);
			} break;
			case LIBINPUT_EVENT_POINTER_BUTTON:
			{
				struct libinput_event_pointer *evp =
					libinput_event_get_pointer_event(ev);

				const bool state = libinput_event_pointer_get_button_state(evp)
					== LIBINPUT_BUTTON_STATE_PRESSED
					? true
					: false;

				switch(libinput_event_pointer_get_button(evp))
				{
					case BTN_LEFT:
					{
						d2tk_base_set_butmask(fbdev->base, D2TK_BUTMASK_LEFT, state);
					} break;
					case BTN_MIDDLE:
					{
						d2tk_base_set_butmask(fbdev->base, D2TK_BUTMASK_MIDDLE, state);
					} break;
					case BTN_RIGHT:
					{
						d2tk_base_set_butmask(fbdev->base, D2TK_BUTMASK_RIGHT, state);
					} break;
				}
			} break;
			case LIBINPUT_EVENT_POINTER_AXIS:
			{
				struct libinput_event_pointer *evp =
					libinput_event_get_pointer_event(ev);

				int32_t dx = 0;
				int32_t dy = 0;

				switch(libinput_event_pointer_get_axis_source(evp))
				{
					case LIBINPUT_POINTER_AXIS_SOURCE_FINGER:
					case LIBINPUT_POINTER_AXIS_SOURCE_CONTINUOUS:
					{
						dx = libinput_event_pointer_get_axis_value(evp,
							LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL);
						dy = libinput_event_pointer_get_axis_value(evp,
							LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);

						dx /= 100.f; //FIXME
						dy /= 100.f; //FIXME
					} break;

					case LIBINPUT_POINTER_AXIS_SOURCE_WHEEL:
					case LIBINPUT_POINTER_AXIS_SOURCE_WHEEL_TILT:
					{
						dx = libinput_event_pointer_get_axis_value_discrete(evp,
							LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL);
						dy = libinput_event_pointer_get_axis_value_discrete(evp,
							LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
					} break;
				}

				d2tk_base_add_mouse_scroll(fbdev->base, -dx, -dy);
			} break;

			case LIBINPUT_EVENT_TOUCH_DOWN:
			{
				struct libinput_event_touch *evt =
					libinput_event_get_touch_event(ev);

				const int32_t slot= libinput_event_touch_get_seat_slot(evt);
				(void)slot; //FIXME

				fbdev->mouse.x = libinput_event_touch_get_x_transformed(evt,
					fbdev->vinfo.xres_virtual);
				fbdev->mouse.y = libinput_event_touch_get_y_transformed(evt,
					fbdev->vinfo.yres_virtual);

				d2tk_clip_int32(0, &fbdev->mouse.x, fbdev->vinfo.xres_virtual);
				d2tk_clip_int32(0, &fbdev->mouse.y, fbdev->vinfo.yres_virtual);

				d2tk_base_set_mouse_pos(fbdev->base, fbdev->mouse.x, fbdev->mouse.y);
				d2tk_base_set_butmask(fbdev->base, D2TK_BUTMASK_LEFT, true);
			} break;
			case LIBINPUT_EVENT_TOUCH_UP:
			{
				struct libinput_event_touch *evt =
					libinput_event_get_touch_event(ev);

				const int32_t slot= libinput_event_touch_get_seat_slot(evt);
				(void)slot; //FIXME

				d2tk_base_set_butmask(fbdev->base, D2TK_BUTMASK_LEFT, false);
			} break;
			case LIBINPUT_EVENT_TOUCH_MOTION:
			{
				struct libinput_event_touch *evt =
					libinput_event_get_touch_event(ev);

				const int32_t slot= libinput_event_touch_get_seat_slot(evt);
				(void)slot; //FIXME

				fbdev->mouse.x = libinput_event_touch_get_x_transformed(evt,
					fbdev->vinfo.xres_virtual);
				fbdev->mouse.y = libinput_event_touch_get_y_transformed(evt,
					fbdev->vinfo.yres_virtual);

				d2tk_clip_int32(0, &fbdev->mouse.x, fbdev->vinfo.xres_virtual);
				d2tk_clip_int32(0, &fbdev->mouse.y, fbdev->vinfo.yres_virtual);

				d2tk_base_set_mouse_pos(fbdev->base, fbdev->mouse.x, fbdev->mouse.y);
			} break;
			case LIBINPUT_EVENT_TOUCH_CANCEL:
			case LIBINPUT_EVENT_TOUCH_FRAME:
			{
				// nothing to do
			} break;

			case LIBINPUT_EVENT_TABLET_TOOL_AXIS:
			case LIBINPUT_EVENT_TABLET_TOOL_PROXIMITY:
			case LIBINPUT_EVENT_TABLET_TOOL_TIP:
			case LIBINPUT_EVENT_TABLET_TOOL_BUTTON:
			{
				struct libinput_event_tablet_tool *evtt =
					libinput_event_get_tablet_tool_event(ev);

				fprintf(stdout, "got tablet tool event\n");
				//FIXME
				(void)evtt;
			} break;

			case LIBINPUT_EVENT_TABLET_PAD_BUTTON:
			case LIBINPUT_EVENT_TABLET_PAD_RING:
			case LIBINPUT_EVENT_TABLET_PAD_STRIP:
#if D2TK_INPUT_1_15
			case LIBINPUT_EVENT_TABLET_PAD_KEY:
#endif
			{
				struct libinput_event_tablet_pad *evtp =
					libinput_event_get_tablet_pad_event(ev);

				fprintf(stdout, "got tablet pad event\n");
				//FIXME
				(void)evtp;
			} break;

			case LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN:
			case LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE:
			case LIBINPUT_EVENT_GESTURE_SWIPE_END:
			{
				struct libinput_event_gesture *evg =
					libinput_event_get_gesture_event(ev);

				fprintf(stdout, "got gesture swipe event\n");
				//FIXME
				(void)evg;
			} break;

			case LIBINPUT_EVENT_GESTURE_PINCH_BEGIN:
			case LIBINPUT_EVENT_GESTURE_PINCH_UPDATE:
			case LIBINPUT_EVENT_GESTURE_PINCH_END:
			{
				struct libinput_event_gesture *evg =
					libinput_event_get_gesture_event(ev);

				fprintf(stdout, "got gesture pinch event\n");
				//FIXME
				(void)evg;
			} break;

			case LIBINPUT_EVENT_SWITCH_TOGGLE:
			{
				struct libinput_event_switch *evs =
					libinput_event_get_switch_event(ev);

				fprintf(stdout, "got switch event\n");
				//FIXME
				(void)evs;
			} break;
		}

		libinput_event_destroy(ev);
	}

	_d2tk_frontend_expose(fbdev);

	return fbdev->done;
}

D2TK_API int
d2tk_frontend_poll(d2tk_frontend_t *fbdev __attribute__((unused)),
	double timeout __attribute__((unused)))
{
	//FIXME not implemented, yet
	return 0;
}

D2TK_API int
d2tk_frontend_get_file_descriptor(d2tk_frontend_t *dpugl __attribute__((unused)))
{
	//FIXME not implemented, yet
	return -1;
}

D2TK_API void
d2tk_frontend_run(d2tk_frontend_t *fbdev, const sig_atomic_t *done)
{
	const unsigned step = 1000000000 / 24;
	struct timespec to;
	clock_gettime(CLOCK_MONOTONIC, &to);

	while(!*done)
	{
		if(clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &to, NULL))
		{
			continue;
		}

		to.tv_nsec += step;
		while(to.tv_nsec >= 1000000000)
		{
			to.tv_sec += 1;
			to.tv_nsec -= 1000000000;
		}

		if(d2tk_frontend_step(fbdev))
		{
			break;
		}
	}
}

D2TK_API void
d2tk_frontend_redisplay(d2tk_frontend_t *fbdev __attribute__((unused)))
{
	// not supported
}

D2TK_API int
d2tk_frontend_set_size(d2tk_frontend_t *fbdev __attribute__((unused)),
	d2tk_coord_t w __attribute__((unused)), d2tk_coord_t h __attribute__((unused)))
{
	// not supported

	return 0;
}

D2TK_API int
d2tk_frontend_get_size(d2tk_frontend_t *fbdev, d2tk_coord_t *w, d2tk_coord_t *h)
{
	if(w)
	{
		*w = fbdev->vinfo.xres_virtual;
	}

	if(h)
	{
		*h = fbdev->vinfo.yres_virtual;
	}

	return 0;
}

D2TK_API void
d2tk_frontend_free(d2tk_frontend_t *fbdev)
{
	if(fbdev->cr)
	{
		cairo_destroy(fbdev->cr);
	}

	if(fbdev->ctx)
	{
		if(fbdev->base)
		{
			d2tk_base_free(fbdev->base);
		}
		d2tk_core_driver.free(fbdev->ctx);
	}

	//FIXME fbdevDestroy(fbdev->view);

	free(fbdev);
}

D2TK_API d2tk_frontend_t *
d2tk_fbdev_new(const d2tk_fbdev_config_t *config)
{
	d2tk_frontend_t *fbdev = calloc(1, sizeof(d2tk_frontend_t));
	if(!fbdev)
	{
		goto fail;
	}

	fbdev->config = config;

	cairo_surface_t *surf = _d2tk_frontend_create(fbdev, fbdev->config->fb_device);
	fbdev->cr = cairo_create(surf);

	fbdev->ctx = d2tk_core_driver.new(config->bundle_path);

	if(!fbdev->ctx)
	{
		goto fail;
	}

	fbdev->base = d2tk_base_new(&d2tk_core_driver, fbdev->ctx);
	if(!fbdev->base)
	{
		goto fail;
	}

	d2tk_base_set_dimensions(fbdev->base,
		fbdev->vinfo.xres_virtual,
		fbdev->vinfo.yres_virtual);

	return fbdev;

fail:
	if(fbdev)
	{
		if(fbdev->ctx)
		{
			d2tk_core_driver.free(fbdev->ctx);
		}

		//FIXME

		free(fbdev);
	}

	return NULL;
}

D2TK_API d2tk_base_t *
d2tk_frontend_get_base(d2tk_frontend_t *fbdev)
{
	return fbdev->base;
}

D2TK_API float
d2tk_frontend_get_scale()
{
	return 1.f;
}
