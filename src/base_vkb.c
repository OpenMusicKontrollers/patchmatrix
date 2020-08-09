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

#include <stddef.h>
#include <string.h>
#include <errno.h>

#include "base_internal.h"

#include <libevdev/libevdev.h>
#include <libevdev/libevdev-uinput.h>

typedef struct _d2tk_atom_body_vkb_t d2tk_atom_body_vkb_t;

struct _d2tk_atom_body_vkb_t
{
	struct libevdev *dev;
	struct libevdev_uinput *uidev;
};

const size_t d2tk_atom_body_vkb_sz = sizeof(d2tk_atom_body_vkb_t);

static void
_fake_event(d2tk_atom_body_vkb_t *vkb, unsigned type, unsigned code, int value)
{
	if(vkb->uidev)
	{
		libevdev_uinput_write_event(vkb->uidev, type, code, value);
	}
}

static void
_fake_key_down(d2tk_atom_body_vkb_t *vkb, unsigned keycode)
{
	_fake_event(vkb, EV_KEY, keycode, 1);
	_fake_event(vkb, EV_SYN, SYN_REPORT, 0);
}

static void
_fake_key_up(d2tk_atom_body_vkb_t *vkb, unsigned keycode)
{
	_fake_event(vkb, EV_KEY, keycode, 0);
	_fake_event(vkb, EV_SYN, SYN_REPORT, 0);
}

typedef struct _keybtn_t keybtn_t;

struct _keybtn_t {
	const char *name;
	const char *altn;
	unsigned code;
	float rect [4];
};

#define W (1.f / 15.f)
#define W_2 (W / 2)
#define H (1.f / 6.f)

static const keybtn_t keybtns [] = {
	// row 1
	{
		.name = "Esc",
		.code = KEY_ESC,
		.rect = { 0*W, 0*H, W + W_2, H }
	},
	{
		.name = "F1",
		.code = KEY_F1,
		.rect = { 1*W + W_2, 0*H, W, H }
	},
	{
		.name = "F2",
		.code = KEY_F2,
		.rect = { 2*W + W_2, 0*H, W, H }
	},
	{
		.name = "F3",
		.code = KEY_F3,
		.rect = { 3*W + W_2, 0*H, W, H }
	},
	{
		.name = "F4",
		.code = KEY_F4,
		.rect = { 4*W + W_2, 0*H, W, H }
	},
	{
		.name = "F5",
		.code = KEY_F5,
		.rect = { 5*W + W_2, 0*H, W, H }
	},
	{
		.name = "F6",
		.code = KEY_F6,
		.rect = { 6*W + W_2, 0*H, W, H }
	},
	{
		.name = "F7",
		.code = KEY_F7,
		.rect = { 7*W + W_2, 0*H, W, H }
	},
	{
		.name = "F8",
		.code = KEY_F8,
		.rect = { 8*W + W_2, 0*H, W, H }
	},
	{
		.name = "F9",
		.code = KEY_F9,
		.rect = { 9*W + W_2, 0*H, W, H }
	},
	{
		.name = "F10",
		.code = KEY_F10,
		.rect = { 10*W + W_2, 0*H, W, H }
	},
	{
		.name = "F11",
		.code = KEY_F11,
		.rect = { 11*W + W_2, 0*H, W, H }
	},
	{
		.name = "F12",
		.code = KEY_F12,
		.rect = { 12*W + W_2, 0*H, W, H }
	},
	{
		.name = "Delete",
		.code = KEY_DELETE,
		.rect = { 13*W + W_2, 0*H, W + W_2, H }
	},

	// row 2
	{
		.name = "`",
		.altn = "~",
		.code = KEY_GRAVE,
		.rect = { 0*W, 1*H, W, H }
	},
	{
		.name = "1",
		.altn = "!",
		.code = KEY_1,
		.rect = { 1*W, 1*H, W, H }
	},
	{
		.name = "2",
		.altn = "@",
		.code = KEY_2,
		.rect = { 2*W, 1*H, W, H }
	},
	{
		.name = "3",
		.altn = "#",
		.code = KEY_3,
		.rect = { 3*W, 1*H, W, H }
	},
	{
		.name = "4",
		.altn = "$",
		.code = KEY_4,
		.rect = { 4*W, 1*H, W, H }
	},
	{
		.name = "5",
		.altn = "%",
		.code = KEY_5,
		.rect = { 5*W, 1*H, W, H }
	},
	{
		.name = "6",
		.altn = "^",
		.code = KEY_6,
		.rect = { 6*W, 1*H, W, H }
	},
	{
		.name = "7",
		.altn = "&",
		.code = KEY_7,
		.rect = { 7*W, 1*H, W, H }
	},
	{
		.name = "8",
		.altn = "*",
		.code = KEY_8,
		.rect = { 8*W, 1*H, W, H }
	},
	{
		.name = "9",
		.altn = "(",
		.code = KEY_9,
		.rect = { 9*W, 1*H, W, H }
	},
	{
		.name = "0",
		.altn = ")",
		.code = KEY_0,
		.rect = { 10*W, 1*H, W, H }
	},
	{
		.name = "-",
		.altn = "_",
		.code = KEY_MINUS,
		.rect = { 11*W, 1*H, W, H }
	},
	{
		.name = "=",
		.altn = "+",
		.code = KEY_EQUAL,
		.rect = { 12*W, 1*H, W, H }
	},
	{
		.name = "Back",
		.code = KEY_BACKSPACE,
		.rect = { 13*W, 1*H, 2*W, H }
	},

	// row 3
	{
		.name = "Tab",
		.code = KEY_TAB,
		.rect = { 0*W, 2*H, W + W_2, H }
	},
	{
		.name = "Q",
		.code = KEY_Q,
		.rect = { 1*W + W_2, 2*H, W, H }
	},
	{
		.name = "W",
		.code = KEY_W,
		.rect = { 2*W + W_2, 2*H, W, H }
	},
	{
		.name = "E",
		.code = KEY_E,
		.rect = { 3*W + W_2, 2*H, W, H }
	},
	{
		.name = "R",
		.code = KEY_R,
		.rect = { 4*W + W_2, 2*H, W, H }
	},
	{
		.name = "T",
		.code = KEY_T,
		.rect = { 5*W + W_2, 2*H, W, H }
	},
	{
		.name = "Y",
		.code = KEY_Y,
		.rect = { 6*W + W_2, 2*H, W, H }
	},
	{
		.name = "U",
		.code = KEY_U,
		.rect = { 7*W + W_2, 2*H, W, H }
	},
	{
		.name = "I",
		.code = KEY_I,
		.rect = { 8*W + W_2, 2*H, W, H }
	},
	{
		.name = "O",
		.code = KEY_O,
		.rect = { 9*W + W_2, 2*H, W, H }
	},
	{
		.name = "P",
		.code = KEY_P,
		.rect = { 10*W + W_2, 2*H, W, H }
	},
	{
		.name = "[",
		.altn = "{",
		.code = KEY_LEFTBRACE,
		.rect = { 11*W + W_2, 2*H, W, H }
	},
	{
		.name = "]",
		.altn = "}",
		.code = KEY_RIGHTBRACE,
		.rect = { 12*W + W_2, 2*H, W, H }
	},
	{
		.name = "\\",
		.altn = "|",
		.code = KEY_BACKSLASH,
		.rect = { 13*W + W_2, 2*H, 2*W - W_2, H }
	},

	// row 4
	{
		.name = "Caps",
		.code = KEY_CAPSLOCK,
		.rect = { 0*W, 3*H, W*2, H }
	},
	{
		.name = "A",
		.code = KEY_A,
		.rect = { 2*W, 3*H, W, H }
	},
	{
		.name = "S",
		.code = KEY_S,
		.rect = { 3*W, 3*H, W, H }
	},
	{
		.name = "D",
		.code = KEY_D,
		.rect = { 4*W, 3*H, W, H }
	},
	{
		.name = "F",
		.code = KEY_F,
		.rect = { 5*W, 3*H, W, H }
	},
	{
		.name = "G",
		.code = KEY_G,
		.rect = { 6*W, 3*H, W, H }
	},
	{
		.name = "H",
		.code = KEY_H,
		.rect = { 7*W, 3*H, W, H }
	},
	{
		.name = "J",
		.code = KEY_J,
		.rect = { 8*W, 3*H, W, H }
	},
	{
		.name = "K",
		.code = KEY_K,
		.rect = { 9*W, 3*H, W, H }
	},
	{
		.name = "L",
		.code = KEY_L,
		.rect = { 10*W, 3*H, W, H }
	},
	{
		.name = ";",
		.altn = ":",
		.code = KEY_SEMICOLON,
		.rect = { 11*W, 3*H, W, H }
	},
	{
		.name = "'",
		.altn = "\"",
		.code = KEY_APOSTROPHE,
		.rect = { 12*W, 3*H, W, H }
	},
	{
		.name = "Enter",
		.code = KEY_ENTER,
		.rect = { 13*W, 3*H, W*2, H }
	},

	// row 5
	{
		.name = "Shift",
		.code = KEY_LEFTSHIFT,
		.rect = { 0*W, 4*H, W*2 + W_2, H }
	},
	{
		.name = "Z",
		.code = KEY_Z,
		.rect = { 2*W + W_2, 4*H, W, H }
	},
	{
		.name = "X",
		.code = KEY_X,
		.rect = { 3*W + W_2, 4*H, W, H }
	},
	{
		.name = "C",
		.code = KEY_C,
		.rect = { 4*W + W_2, 4*H, W, H }
	},
	{
		.name = "V",
		.code = KEY_V,
		.rect = { 5*W + W_2, 4*H, W, H }
	},
	{
		.name = "B",
		.code = KEY_B,
		.rect = { 6*W + W_2, 4*H, W, H }
	},
	{
		.name = "N",
		.code = KEY_N,
		.rect = { 7*W + W_2, 4*H, W, H }
	},
	{
		.name = "M",
		.code = KEY_M,
		.rect = { 8*W + W_2, 4*H, W, H }
	},
	{
		.name = ",",
		.altn = "<",
		.code = KEY_COMMA,
		.rect = { 9*W + W_2, 4*H, W, H }
	},
	{
		.name = ".",
		.altn = ">",
		.code = KEY_DOT,
		.rect = { 10*W + W_2, 4*H, W, H }
	},
	{
		.name = "/",
		.altn = "?",
		.code = KEY_SLASH,
		.rect = { 11*W + W_2, 4*H, W, H }
	},
	{
		.name = "Shift",
		.code = KEY_RIGHTSHIFT,
		.rect = { 12*W + W_2, 4*H, 3*W - W_2, H }
	},

	// row 6
	{
		.name = "Fn",
		.code = KEY_FN,
		.rect = { 0*W, 5*H, W, H }
	},
	{
		.name = "Ctrl",
		.code = KEY_LEFTCTRL,
		.rect = { 1*W, 5*H, W, H }
	},
	{
		.name = "Mta",
		.code = KEY_LEFTMETA,
		.rect = { 2*W, 5*H, W, H }
	},
	{
		.name = "Alt",
		.code = KEY_LEFTALT,
		.rect = { 3*W, 5*H, W, H }
	},
	{
		.name = "Space",
		.code = KEY_SPACE,
		.rect = { 4*W, 5*H, 5*W, H }
	},
	{
		.name = "Alt",
		.code = KEY_RIGHTALT,
		.rect = { 9*W, 5*H, W, H }
	},
	{
		.name = "Mta",
		.code = KEY_RIGHTMETA,
		.rect = { 10*W, 5*H, W, H }
	},
	{
		.name = "Ctrl",
		.code = KEY_RIGHTCTRL,
		.rect = { 11*W, 5*H, W, H }
	},
	{
		.name = "Hom",
		.code = KEY_HOME,
		.rect = { 12*W, 5*H, W, H }
	},
	{
		.name = "End",
		.code = KEY_END,
		.rect = { 13*W, 5*H, W, H }
	},
	{
		.name = "Ins",
		.code = KEY_INSERT,
		.rect = { 14*W, 5*H, W, H }
	},

	{ // sentinel
		.name = NULL
	}
};

static int
_vkb_deinit(d2tk_atom_body_vkb_t *vkb)
{
	if(vkb->uidev)
	{
		libevdev_uinput_destroy(vkb->uidev);
		vkb->uidev = NULL;
	}

	if(vkb->dev)
	{
		libevdev_free(vkb->dev);
		vkb->dev = NULL;
	}

	return 0;
}

static int
_vkb_init(d2tk_atom_body_vkb_t *vkb)
{
	if(vkb->dev)
	{
		return 0; // already initialized
	}

	vkb->dev = libevdev_new();
	if(!vkb->dev)
	{
		fprintf(stderr, "libevdev_new: %s\n", strerror(errno));
		return 1;;
	}

	libevdev_set_name(vkb->dev, "D2TK keyboard");
	libevdev_enable_event_type(vkb->dev, EV_SYN);
	libevdev_enable_event_code(vkb->dev, EV_SYN, SYN_REPORT, NULL);
	libevdev_enable_event_type(vkb->dev, EV_KEY);

	for(const keybtn_t *keybtn = keybtns; keybtn->name; keybtn++)
	{
		libevdev_enable_event_code(vkb->dev, EV_KEY, keybtn->code, NULL);
	}

	vkb->uidev = NULL;
	libevdev_uinput_create_from_device(vkb->dev, LIBEVDEV_UINPUT_OPEN_MANAGED,
		&vkb->uidev);
	if(!vkb->uidev)
	{
		fprintf(stderr, "libevdev_uinput_create_from_device: %s\n", strerror(errno));
		return 1;
	}

	return 0;
}

static int
_vkb_event(d2tk_atom_event_type_t event, void *data)
{
	d2tk_atom_body_vkb_t *vkb = data;

	switch(event)
	{
		case D2TK_ATOM_EVENT_DEINIT:
		{
			return _vkb_deinit(vkb);
		} break;

		case D2TK_ATOM_EVENT_PROBE:
			// fall-through
		case D2TK_ATOM_EVENT_NONE:
			// fall-through
		default:
		{
			// nothing to do
		} break;
	}

	return 0;
}

D2TK_API d2tk_state_t
d2tk_base_vkb(d2tk_base_t *base, d2tk_id_t id, const d2tk_rect_t *rect)
{
	d2tk_atom_body_vkb_t *vkb = _d2tk_base_get_atom(base, id, D2TK_ATOM_VKB,
	_vkb_event);

	_vkb_init(vkb);

	for(const keybtn_t *keybtn = keybtns; keybtn->name; keybtn++)
	{
		const d2tk_rect_t bnd = {
			.x = rect->x + keybtn->rect[0]*rect->w,
			.y = rect->y + keybtn->rect[1]*rect->h,
			.w = keybtn->rect[2]*rect->w,
			.h = keybtn->rect[3]*rect->h
		};

		const char *lbl = d2tk_base_get_modmask(base, D2TK_MODMASK_SHIFT, false)
				&& keybtn->altn
			? keybtn->altn
			: keybtn->name;

		const unsigned idx = keybtn - keybtns;
		const d2tk_id_t itrid = D2TK_ID_IDX(idx);
		const d2tk_id_t subid = (itrid << 32) | id;
		const d2tk_state_t state = d2tk_base_button_label(base, subid,
			-1, lbl, D2TK_ALIGN_CENTERED, &bnd);

		if(d2tk_state_is_down(state))
		{
			_fake_key_down(vkb, keybtn->code);
		}
		else if(d2tk_state_is_up(state))
		{
			_fake_key_up(vkb, keybtn->code);
		}
	}

	return D2TK_STATE_NONE;
}
