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

#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <vterm.h>
#include <pty.h>
#include <utmp.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <poll.h>

#include "base_internal.h"

#define DEFAULT_FG 0xddddddff
#define DEFAULT_BG 0x222222ff

#define DEFAULT_FG_LIGHT 0xdddddd7f
#define DEFAULT_BG_LIGHT 0x2222227f

#define NROWS_MAX 512
#define NCOLS_MAX 512

#define MAX(x, y) (x > y ? y : x)

#define FONT_CODE_LIGHT   "FiraCode:light"
#define FONT_CODE_REGULAR "FiraCode:regular"
#define FONT_CODE_MEDIUM  "FiraCode:medium"
#define FONT_CODE_BOLD    "FiraCode:bold"

typedef struct _col_t col_t;
typedef struct _cell_t cell_t;
typedef struct _d2tk_atom_body_pty_t d2tk_atom_body_pty_t;
typedef struct _d2tk_pty_t d2tk_pty_t;

struct _col_t {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

struct _cell_t {
	char lbl [8];
	size_t lbl_len;
	bool bold;
	bool italic;
	bool reverse;
	bool cursor;
	uint32_t fg;
	uint32_t bg;
};

struct _d2tk_atom_body_pty_t {
	d2tk_coord_t height;

	d2tk_coord_t ncols;
	d2tk_coord_t nrows;
	unsigned bell;

	int fd;
	pid_t kid;

	VTerm *vterm;
	VTermScreen *screen;
	VTermState *state;

	uint32_t dark;
	uint32_t light;

	bool cursor_visible;
	int cursor_shape;

	col_t max_red;
	col_t max_green;
	col_t max_blue;

	cell_t cells [NROWS_MAX][NCOLS_MAX];
};

struct _d2tk_pty_t {
	d2tk_state_t state;
	d2tk_atom_body_pty_t *vpty;
};

const size_t d2tk_atom_body_pty_sz = sizeof(d2tk_atom_body_pty_t);
const size_t d2tk_pty_sz = sizeof(d2tk_pty_t);

static inline uint32_t
_term_dark(d2tk_atom_body_pty_t *vpty)
{
	return vpty->dark;
}

static inline uint32_t
_term_light(d2tk_atom_body_pty_t *vpty)
{
	return vpty->light;
}

static inline int
_term_read(d2tk_atom_body_pty_t *vpty,
	void (*cb)(const char *buf, size_t len, void *data), void *data)
{
	char buf [4096];
	int count = 0;

	while(true)
	{
		const ssize_t len = read(vpty->fd, buf, sizeof(buf));

		if(len == -1)
		{
			if(errno != EAGAIN)
			{
				//FIXME fprintf(stderr, "read failed '%s'\n", strerror(errno));
			}
			break;
		}

		if(len == 0)
		{
			break;;
		}

		cb(buf, len, data);
		count += 1;
	}

	return count;
}

static inline int
_term_done(d2tk_atom_body_pty_t *vpty)
{
	if(vpty->kid == 0)
	{
		return 1;
	}

	if(waitpid(vpty->kid, NULL, WNOHANG) == vpty->kid)
	{
		vpty->kid = 0;
		return 1;
	}

	return 0;
}

static void
_term_output(const char *buf, size_t len, void *data)
{
	d2tk_atom_body_pty_t *vpty = data;

	const ssize_t writ = write(vpty->fd, buf, len);

	if(writ == -1)
	{
		fprintf(stderr, "write failed '%s'\n", strerror(errno));
		return;
	}
}

static int
_screen_settermprop(VTermProp prop, VTermValue *val, void *data)
{
	d2tk_atom_body_pty_t *vpty = data;

	switch(prop)
	{
		case VTERM_PROP_CURSORVISIBLE: // bool
		{
			vpty->cursor_visible = val->boolean;
		} break;
		case VTERM_PROP_CURSORSHAPE: // number
		{
			vpty->cursor_shape = val->number;
		} break;

		default:
		{
			// nothing to do
		} break;
	}

	return 0;
}

static int
_screen_resize(int nrows, int ncols, void *data)
{
	d2tk_atom_body_pty_t *vpty = data;

	if( (nrows == vpty->nrows) && (ncols == vpty->ncols) )
	{
		return 0;
	}

	const struct winsize winsize = {
		.ws_row = nrows,
		.ws_col = ncols,
		.ws_xpixel = 0,
		.ws_ypixel = 0
	};

	if(ioctl(vpty->fd, TIOCSWINSZ, &winsize) == -1)
	{
		fprintf(stderr, "ioctl failed '%s'\n", strerror(errno));
		return 1;
	}

	vpty->nrows = nrows;
	vpty->ncols = ncols;

	return 0;
}

static int
_screen_bell(void *data)
{
	d2tk_atom_body_pty_t *vpty = data;

	vpty->bell += 1;

	return 0;
}

static const VTermScreenCallbacks screen_callbacks = {
	.settermprop = _screen_settermprop,
	.bell = _screen_bell,
  .resize = _screen_resize
};

typedef struct _clone_data_t clone_data_t;

struct _clone_data_t {
	int master;
	int slave;
	int stderr_save_fileno;
	char **argv;
};

static int
_clone(void *data)
{
	clone_data_t *clone_data = data;

	close(clone_data->master);

	if(login_tty(clone_data->slave) == -1)
	{
		_exit(1);
	}

	fcntl(clone_data->stderr_save_fileno, F_SETFD,
		fcntl(clone_data->stderr_save_fileno, F_GETFD) | FD_CLOEXEC);
	FILE *stderr_save = fdopen(clone_data->stderr_save_fileno, "a");

	/* Restore the ISIG signals back to defaults */
	signal(SIGINT,  SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGSTOP, SIG_DFL);
	signal(SIGCONT, SIG_DFL);

	putenv("TERM=xterm-256color");
	//putenv("COLORTERM=truecolor");

	execvp(clone_data->argv[0], clone_data->argv);
	fprintf(stderr_save, "cannot exec(%s) - %s\n", clone_data->argv[0], strerror(errno));
	_exit(EXIT_FAILURE);

	return 0;
}

static int
_forkpty(int *amaster, char *name, const struct termios *termp,
	const struct winsize *winp, char **argv, int stderr_save_fileno)
{
	clone_data_t clone_data = {
		.master = 0,
		.slave = 0,
		.argv = argv,
		.stderr_save_fileno = stderr_save_fileno
	};
	
	if(openpty(&clone_data.master, &clone_data.slave, name, termp, winp) == -1)
	{
    return -1;
	}

#if D2TK_CLONE == 1
#	define STACK_SIZE (1024 * 1024)
	uint8_t *stack = mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
	if(stack == MAP_FAILED)
	{
		close(clone_data.master);
		close(clone_data.slave);
		return -1;
	}

	uint8_t *stack_top = stack + STACK_SIZE;
	const int flags = CLONE_FS | CLONE_IO | CLONE_VFORK | CLONE_VM;
	const int pid = clone(_clone, stack_top, flags, &clone_data);
#	undef STACK_SIZE
#elif D2TK_VFORK == 1
	const int pid = vfork();
#else
	const int pid = fork();
#endif

	switch(pid)
	{
		case -1:
		{
			close(clone_data.master);
			close(clone_data.slave);
		} return -1;

#if D2TK_CLONE == 0
    case 0: //child
		{
			// everything is done in _clone
		} return _clone(&clone_data);
#endif

		default: // parent
		{
			*amaster = clone_data.master;
			close(clone_data.slave);
		} return pid;
	}
}

static int
_term_init(d2tk_atom_body_pty_t *vpty, char **argv,
	d2tk_coord_t height, d2tk_coord_t ncols, d2tk_coord_t nrows)
{
	vpty->height = height;
	vpty->nrows = nrows;
	vpty->ncols = ncols;

	struct termios termios = {
		.c_iflag = ICRNL|IXON,
		.c_oflag = OPOST|ONLCR
#ifdef TAB0
			|TAB0
#endif
			,
		.c_cflag = CS8|CREAD,
		.c_lflag = ISIG|ICANON|IEXTEN|ECHO|ECHOE|ECHOK,
		/* c_cc later */
	};

#ifdef IUTF8
	termios.c_iflag |= IUTF8;
#endif
#ifdef NL0
	termios.c_oflag |= NL0;
#endif
#ifdef CR0
	termios.c_oflag |= CR0;
#endif
#ifdef BS0
	termios.c_oflag |= BS0;
#endif
#ifdef VT0
	termios.c_oflag |= VT0;
#endif
#ifdef FF0
	termios.c_oflag |= FF0;
#endif
#ifdef ECHOCTL
	termios.c_lflag |= ECHOCTL;
#endif
#ifdef ECHOKE
	termios.c_lflag |= ECHOKE;
#endif

	cfsetspeed(&termios, 115200);

	termios.c_cc[VINTR]    = 0x1f & 'C';
	termios.c_cc[VQUIT]    = 0x1f & '\\';
	termios.c_cc[VERASE]   = 0x7f;
	termios.c_cc[VKILL]    = 0x1f & 'U';
	termios.c_cc[VEOF]     = 0x1f & 'D';
	termios.c_cc[VEOL]     = _POSIX_VDISABLE;
	termios.c_cc[VEOL2]    = _POSIX_VDISABLE;
	termios.c_cc[VSTART]   = 0x1f & 'Q';
	termios.c_cc[VSTOP]    = 0x1f & 'S';
	termios.c_cc[VSUSP]    = 0x1f & 'Z';
	termios.c_cc[VREPRINT] = 0x1f & 'R';
	termios.c_cc[VWERASE]  = 0x1f & 'W';
	termios.c_cc[VLNEXT]   = 0x1f & 'V';
	termios.c_cc[VMIN]     = 1;
	termios.c_cc[VTIME]    = 0;

	const struct winsize winsize = {
		.ws_row = vpty->nrows,
		.ws_col = vpty->ncols,
		.ws_xpixel = 0,
		.ws_ypixel = 0
	};

	const int stderr_save_fileno = dup(STDERR_FILENO);

	vpty->kid = _forkpty(&vpty->fd, NULL, &termios, &winsize, argv, stderr_save_fileno);
	if(vpty->kid == -1)
	{
		vpty->kid = 0;
		return 1;
	}

  fcntl(vpty->fd, F_SETFL, fcntl(vpty->fd, F_GETFL) | O_NONBLOCK);
	close(stderr_save_fileno);

	vpty->vterm = vterm_new(vpty->nrows, vpty->ncols);
	vterm_set_utf8(vpty->vterm, 1);
	vterm_output_set_callback(vpty->vterm, _term_output, vpty);

	vpty->state = vterm_obtain_state(vpty->vterm);

	vpty->screen = vterm_obtain_screen(vpty->vterm);
	vterm_screen_set_callbacks(vpty->screen, &screen_callbacks, vpty);
	vterm_screen_reset(vpty->screen, 1);

	return 0;
}

static int
_term_probe(d2tk_atom_body_pty_t *vpty)
{
	int again = 0;

	struct pollfd fds = {
		.fd = vpty->fd,
		.events = POLLIN
	};

	switch(poll(&fds, 1, 0))
	{
		case -1:
		{
			//printf("[%s] error: %s\n", __func__, strerror(errno));
		} break;
		case 0:
		{
			//printf("[%s] timeout\n", __func__);
		} break;
		default:
		{
			//printf("[%s] ready\n", __func__);
			again = 1;
		} break;
	}

	return again;
}

static int
_term_deinit(d2tk_atom_body_pty_t *vpty)
{
	if(!vpty)
	{
		return 1;
	}

	if(vpty->kid != 0)
	{
		kill(vpty->kid, SIGTERM);

#define RETRIES 100 // over the course of 100 ms
		for(int i = 0; i < RETRIES; i++)
		{
			if(waitpid(vpty->kid, NULL, WNOHANG) == vpty->kid)
			{
				vpty->kid = 0;
				break;
			}

			usleep(1000000 / RETRIES);
		}
#undef RETRIES
	}

	if(vpty->kid !=  0)
	{
		fprintf(stderr, "[%s] sending SIGKILL to pid %i\n", __func__, vpty->kid);
		kill(vpty->kid, SIGKILL);
		waitpid(vpty->kid, NULL, 0);
		vpty->kid = 0;
	}

	if(vpty->vterm)
	{
		vterm_free(vpty->vterm);
	}

	memset(vpty, 0x0, sizeof(d2tk_atom_body_pty_t));

	return 0;
}

static int
_term_event(d2tk_atom_event_type_t event, void *data)
{
	d2tk_atom_body_pty_t *vpty = data;

	switch(event)
	{
		case D2TK_ATOM_EVENT_PROBE:
		{
			return _term_probe(vpty);
		} break;
		case D2TK_ATOM_EVENT_DEINIT:
		{
			return _term_deinit(vpty);
		} break;

		case D2TK_ATOM_EVENT_NONE:
			// fall-through
		default:
		{
			// nothing to do
		} break;
	}

	return 0;
}

static inline void
_term_set_dark_light(d2tk_atom_body_pty_t *vpty, uint32_t fg,
	int32_t r1, int32_t g1, int32_t b1)
{
	const int32_t r0 = (vpty->light >> 24) & 0xff;
	const int32_t g0 = (vpty->light >> 16) & 0xff;
	const int32_t b0 = (vpty->light >>  8) & 0xff;
	const int32_t d0 = (r0 - g0) + (r0 - b0);

	const int32_t d1 = (r1 - g1) + (r1 - b1);

	if(d1 > d0)
	{
		vpty->light = fg;
		vpty->dark = ( (r1 >> 1) << 24)
			| ( (g1 >> 1) << 16)
			| ( (b1 >> 1) << 8)
			| 0xff;
	}
}

static inline void
_term_set_max_red(d2tk_atom_body_pty_t *vpty,
	int32_t r1, int32_t g1, int32_t b1)
{
	const int32_t r0 = vpty->max_red.r;
	const int32_t g0 = vpty->max_red.g;
	const int32_t b0 = vpty->max_red.b;
	const int32_t d0 = (r0 - g0) + (r0 - b0);

	const int32_t d1 = (r1 - g1) + (r1 - b1);

	if(d1 > d0)
	{
		vpty->max_red.r = r1;
		vpty->max_red.g = g1;
		vpty->max_red.b = b1;
	}
}

static inline void
_term_set_max_green(d2tk_atom_body_pty_t *vpty,
	int32_t r1, int32_t g1, int32_t b1)
{
	const int32_t r0 = vpty->max_green.r;
	const int32_t g0 = vpty->max_green.g;
	const int32_t b0 = vpty->max_green.b;
	const int32_t d0 = (g0 - r0) + (g0 - b0);

	const int32_t d1 = (g1 - r1) + (g1 - b1);

	if(d1 > d0)
	{
		vpty->max_green.r = r1;
		vpty->max_green.g = g1;
		vpty->max_green.b = b1;
	}
}

static inline void
_term_set_max_blue(d2tk_atom_body_pty_t *vpty,
	int32_t r1, int32_t g1, int32_t b1)
{
	const int32_t r0 = vpty->max_blue.r;
	const int32_t g0 = vpty->max_blue.g;
	const int32_t b0 = vpty->max_blue.b;
	const int32_t d0 = (b0 - r0) + (b0 - g0);

	const int32_t d1 = (b1 - r1) + (b1 - g1);

	if(d1 > d0)
	{
		vpty->max_blue.r = r1;
		vpty->max_blue.g = g1;
		vpty->max_blue.b = b1;
	}
}

static inline void
_term_set_colors(d2tk_atom_body_pty_t *vpty, uint32_t fg)
{
	const int32_t r1 = (fg >> 24) & 0xff;
	const int32_t g1 = (fg >> 16) & 0xff;
	const int32_t b1 = (fg >>  8) & 0xff;

	_term_set_dark_light(vpty, fg, r1, g1, b1);
	_term_set_max_red(vpty, r1, g1, b1);
	_term_set_max_green(vpty, r1, g1, b1);
	_term_set_max_blue(vpty, r1, g1, b1);
}

static inline void
_term_update(d2tk_atom_body_pty_t *vpty)
{
	VTermPos cursor;
	VTermPos pos;

	memset(&cursor, 0x0, sizeof(cursor));
	vterm_state_get_cursorpos(vpty->state, &cursor);

	memset(vpty->cells, 0x0, sizeof(vpty->cells));

	for(int y = 0; y < vpty->nrows; y++)
	{
		pos.row = y;

		for(int x = 0; x < vpty->ncols; x++)
		{
			cell_t *tar = &vpty->cells[y][x];

			pos.col = x;

			VTermScreenCell cell;
			memset(&cell, 0x0, sizeof(cell));
			vterm_screen_get_cell(vpty->screen, pos, &cell);

			if( cell.chars[0] && (cell.width == 1) )
			{
				if(cell.chars[0] != ' ')
				{
					const char *tail = utf8catcodepoint(tar->lbl,
						cell.chars[0], sizeof(tar->lbl));

					tar->lbl_len = tail - tar->lbl;
				}
			}

			if(cell.attrs.bold)
			{
				tar->bold = true;
			}

			if(cell.attrs.italic)
			{
				tar->italic = true;
			}

			uint32_t fg_rgba = 0x0;
			uint32_t bg_rgba = 0x0;

			if(VTERM_COLOR_IS_RGB(&cell.fg))
			{
				const VTermColor tmp = cell.fg;

				fg_rgba = (tmp.rgb.red << 24)
					| (tmp.rgb.green << 16)
					| (tmp.rgb.blue << 8)
					| 0xff;
			}
			else if(VTERM_COLOR_IS_INDEXED(&cell.fg))
			{
				VTermColor tmp = cell.fg;
				vterm_screen_convert_color_to_rgb(vpty->screen, &tmp);

				fg_rgba = (tmp.rgb.red << 24)
					| (tmp.rgb.green << 16)
					| (tmp.rgb.blue << 8)
					| 0xff;
			}
			else if(VTERM_COLOR_IS_DEFAULT_FG(&cell.fg))
			{
				fg_rgba = DEFAULT_FG;
			}
			else if(VTERM_COLOR_IS_DEFAULT_BG(&cell.fg))
			{
				fg_rgba = DEFAULT_BG;
			}

			if(VTERM_COLOR_IS_RGB(&cell.bg))
			{
				const VTermColor tmp = cell.bg;

				bg_rgba = (tmp.rgb.red << 24)
					| (tmp.rgb.green << 16)
					| (tmp.rgb.blue << 8)
					| 0xff;
			}
			else if(VTERM_COLOR_IS_INDEXED(&cell.bg))
			{
				VTermColor tmp = cell.bg;
				vterm_screen_convert_color_to_rgb(vpty->screen, &tmp);

				bg_rgba = (tmp.rgb.red << 24)
					| (tmp.rgb.green << 16)
					| (tmp.rgb.blue << 8)
					| 0xff;
			}
			else if(VTERM_COLOR_IS_DEFAULT_FG(&cell.bg))
			{
				bg_rgba = 0xffffffff;
			}
			else if(VTERM_COLOR_IS_DEFAULT_BG(&cell.bg))
			{
				bg_rgba = 0x000000ff;
			}

			tar->cursor = (y == cursor.row) && (x == cursor.col) && vpty->cursor_visible;
			tar->reverse = cell.attrs.reverse;
			tar->fg = fg_rgba;
			tar->bg = bg_rgba;

			_term_set_colors(vpty, fg_rgba);
		}
	}
}

static void
_term_resize(d2tk_atom_body_pty_t *vpty, d2tk_coord_t ncols,
	d2tk_coord_t nrows)
{
	if( (nrows != vpty->nrows) || (ncols != vpty->ncols) )
	{
		vterm_set_size(vpty->vterm, nrows, ncols);
	}
}

static inline void
_term_input_cb(const char *buf, size_t len, void *data)
{
	d2tk_atom_body_pty_t *vpty = data;

	vterm_input_write(vpty->vterm, buf, len);
}

static inline void
_term_input(d2tk_atom_body_pty_t *vpty)
{
	if(_term_read(vpty, _term_input_cb, vpty) )
	{
		_term_update(vpty);
	}
}

static inline d2tk_state_t
_term_behave(d2tk_base_t *base, d2tk_atom_body_pty_t *vpty,
	d2tk_id_t id, const d2tk_rect_t *rect)
{
	const d2tk_state_t state = d2tk_base_is_active_hot(base, id, rect,
		D2TK_FLAG_NONE);

	VTermModifier mod = VTERM_MOD_NONE;

	if(d2tk_state_is_focused(state))
	{
		if(d2tk_base_get_keymask(base, D2TK_KEYMASK_UP, true))
		{
			vterm_keyboard_key(vpty->vterm, VTERM_KEY_UP, mod);
		}
		if(d2tk_base_get_keymask(base, D2TK_KEYMASK_DOWN, true))
		{
			vterm_keyboard_key(vpty->vterm, VTERM_KEY_DOWN, mod);
		}
		if(d2tk_base_get_keymask(base, D2TK_KEYMASK_LEFT, true))
		{
			vterm_keyboard_key(vpty->vterm, VTERM_KEY_LEFT, mod);
		}
		if(d2tk_base_get_keymask(base, D2TK_KEYMASK_RIGHT, true))
		{
			vterm_keyboard_key(vpty->vterm, VTERM_KEY_RIGHT, mod);
		}

		if(d2tk_base_get_keymask(base, D2TK_KEYMASK_INS, true))
		{
			vterm_keyboard_key(vpty->vterm, VTERM_KEY_INS, mod);
		}
		if(d2tk_base_get_keymask(base, D2TK_KEYMASK_DEL, true))
		{
			vterm_keyboard_key(vpty->vterm, VTERM_KEY_DEL, mod);
		}
		if(d2tk_base_get_keymask(base, D2TK_KEYMASK_HOME, true))
		{
			vterm_keyboard_key(vpty->vterm, VTERM_KEY_HOME, mod);
		}
		if(d2tk_base_get_keymask(base, D2TK_KEYMASK_END, true))
		{
			vterm_keyboard_key(vpty->vterm, VTERM_KEY_END, mod);
		}
		if(d2tk_base_get_keymask(base, D2TK_KEYMASK_PAGEUP, true))
		{
			vterm_keyboard_key(vpty->vterm, VTERM_KEY_PAGEUP, mod);
		}
		if(d2tk_base_get_keymask(base, D2TK_KEYMASK_PAGEDOWN, true))
		{
			vterm_keyboard_key(vpty->vterm, VTERM_KEY_PAGEDOWN, mod);
		}

		{
			ssize_t len = 0;
			const utf8_int32_t *utf8 = NULL;

			d2tk_base_get_utf8(base, &len, &utf8);

			for(ssize_t i = 0; i < len; i++)
			{
				vterm_keyboard_unichar(vpty->vterm, utf8[i], mod);
			}
		}
	}

	if(d2tk_base_get_modmask(base, D2TK_MODMASK_SHIFT, false))
	{
		mod |= VTERM_MOD_SHIFT;
	}
	if(d2tk_base_get_modmask(base, D2TK_MODMASK_ALT, false))
	{
		mod |= VTERM_MOD_ALT;
	}
	if(d2tk_base_get_modmask(base, D2TK_MODMASK_CTRL, false))
	{
		mod |= VTERM_MOD_CTRL;
	}

	if(d2tk_state_is_focus_in(state))
	{
		vterm_state_focus_in(vpty->state);
	}

	if(d2tk_state_is_focus_out(state))
	{
		vterm_state_focus_out(vpty->state);
	}

	if(d2tk_state_is_hot(state))
	{
		d2tk_coord_t mx, my;
		int dx, dy;
		d2tk_base_get_mouse_pos(base, &mx, &my);
		d2tk_base_get_mouse_scroll(base, &dx, &dy, false);

		const int row = (my - rect->y) * vpty->nrows / rect->h;
		const int col = (mx - rect->x) * vpty->ncols / rect->w;

		vterm_mouse_move(vpty->vterm, row, col, mod);

		const bool btn_l = d2tk_base_get_butmask(base, D2TK_BUTMASK_LEFT, false);
		const bool btn_m = d2tk_base_get_butmask(base, D2TK_BUTMASK_MIDDLE, false);
		const bool btn_r = d2tk_base_get_butmask(base, D2TK_BUTMASK_RIGHT, false);

		vterm_mouse_button(vpty->vterm, 1, btn_l, mod);
		vterm_mouse_button(vpty->vterm, 2, btn_m, mod);
		vterm_mouse_button(vpty->vterm, 3, btn_r, mod);

		if(dy > 0)
		{
			vterm_mouse_button(vpty->vterm, 4, true, mod);
		}
		else if(dy < 0)
		{
			vterm_mouse_button(vpty->vterm, 5, true, mod);
		}
	}

	return state;
}

static inline void
_term_draw(d2tk_base_t *base, d2tk_atom_body_pty_t *vpty,
	const d2tk_rect_t *rect, bool focus)
{
	D2TK_BASE_TABLE(rect, vpty->ncols, vpty->nrows, D2TK_FLAG_TABLE_REL, tab)
	{
		const int x = d2tk_table_get_index_x(tab);
		const int y = d2tk_table_get_index_y(tab);
		const d2tk_rect_t *trect = d2tk_table_get_rect(tab);

		const d2tk_style_t *old_style = d2tk_base_get_style(base);
		d2tk_style_t style = *old_style;
		cell_t *cell = &vpty->cells[y][x];

		style.border_width = 0;
		style.padding = 0;
		style.rounding = 0;

		uint32_t fg = cell->fg;
		uint32_t bg = cell->bg;

		if(cell->cursor)
		{
			// draw box cursor
			if(vpty->cursor_shape == VTERM_PROP_CURSORSHAPE_BLOCK)
			{
				fg = focus ? DEFAULT_BG : DEFAULT_BG_LIGHT;
				bg = focus ? DEFAULT_FG : DEFAULT_FG_LIGHT;
			}
		}
		else if(cell->reverse)
		{
			const uint32_t tmp = fg;

			fg = bg;
			bg = tmp;
		}

		style.text_fill_color[D2TK_TRIPLE_NONE] = bg;
		style.text_stroke_color[D2TK_TRIPLE_NONE] = fg;

		if(cell->bold)
		{
			style.font_face = FONT_CODE_BOLD;
		}
		else if(cell->italic)
		{
			style.font_face = FONT_CODE_LIGHT;
		}
		else
		{
			style.font_face = FONT_CODE_REGULAR;
		}

		d2tk_base_set_style(base, &style);

		d2tk_base_label(base, cell->lbl_len, cell->lbl, 1.f, trect,
			D2TK_ALIGN_LEFT | D2TK_ALIGN_BOTTOM);

		d2tk_base_set_style(base, old_style);

		if(cell->cursor)
		{
			style.font_face = FONT_CODE_BOLD;
			style.text_fill_color[D2TK_TRIPLE_NONE] = 0x0;
			style.text_stroke_color[D2TK_TRIPLE_NONE] = focus
				? DEFAULT_FG
				: DEFAULT_FG_LIGHT;
			d2tk_base_set_style(base, &style);

			// draw underline cursor overlay
			if(vpty->cursor_shape == VTERM_PROP_CURSORSHAPE_UNDERLINE)
			{
				static const char lbl [2] = "_";

				d2tk_base_label(base, sizeof(lbl), lbl, 1.f, trect,
					D2TK_ALIGN_LEFT | D2TK_ALIGN_TOP);
			}
			// draw bar cursor overlay
			else if(vpty->cursor_shape == VTERM_PROP_CURSORSHAPE_BAR_LEFT)
			{
				static const char lbl [2] = "|";

				d2tk_rect_t bnd = *trect;
				bnd.x -= bnd.w/2;

				d2tk_base_label(base, sizeof(lbl), lbl, 1.f, &bnd,
					D2TK_ALIGN_LEFT | D2TK_ALIGN_TOP);
			}

			d2tk_base_set_style(base, old_style);
		}
	}
}

D2TK_API d2tk_pty_t *
d2tk_pty_begin(d2tk_base_t *base, d2tk_id_t id, char **argv,
	d2tk_coord_t height, const d2tk_rect_t *rect, bool reinit, d2tk_pty_t *pty)
{
	memset(pty, 0x0, sizeof(d2tk_pty_t));

	d2tk_atom_body_pty_t *vpty = _d2tk_base_get_atom(base, id, D2TK_ATOM_PTY,
		_term_event);
	pty->vpty = vpty;

	const d2tk_coord_t width = height / 2;
	const d2tk_coord_t ncols = rect->w / width;
	const d2tk_coord_t nrows = rect->h / height;

	if(reinit)
	{
		_term_deinit(vpty);
	}

	if(vpty->height == 0)
	{
		if(_term_init(vpty, argv, height, ncols, nrows) != 0)
		{
			fprintf(stderr, "[%s] _term_init failed\n", __func__);
		}
	}

	const d2tk_style_t *old_style = d2tk_base_get_style(base);
	d2tk_style_t style = *old_style;

	style.fill_color[D2TK_TRIPLE_ACTIVE] = _term_dark(vpty);
	style.fill_color[D2TK_TRIPLE_ACTIVE_HOT] = _term_light(vpty);
	style.fill_color[D2TK_TRIPLE_ACTIVE_FOCUS] = _term_dark(vpty);
	style.fill_color[D2TK_TRIPLE_ACTIVE_HOT_FOCUS] = _term_light(vpty);
	style.font_face = FONT_CODE_REGULAR;
	d2tk_base_set_style(base, &style);

	_term_resize(vpty, ncols, nrows);

	pty->state = _term_behave(base, vpty, id, rect);

	_term_input(vpty);

	_term_draw(base, vpty, rect, d2tk_state_is_focused(pty->state));

	if(_term_done(vpty))
	{
		_term_deinit(vpty);

		pty->state |= D2TK_STATE_CLOSE;
	}

	if(vpty->bell)
	{
		pty->state |= D2TK_STATE_BELL;

		vpty->bell = 0;
	}

	d2tk_base_set_style(base, old_style);

	return pty;
}

D2TK_API bool
d2tk_pty_not_end(d2tk_pty_t *pty)
{
	return pty ? true : false;
}

D2TK_API d2tk_pty_t *
d2tk_pty_next(d2tk_pty_t *pty __attribute__((unused)))
{
	return NULL;
}

D2TK_API d2tk_state_t
d2tk_pty_get_state(d2tk_pty_t *pty)
{
	return pty->state;
}

static inline uint32_t
_col_to_uint32(const col_t *col)
{
	return (col->r << 24)
		| (col->g << 16)
		| (col->b << 8)
		| 0xff;
}

#define FALLBACK_MAX_RED   0x7f0000ff
#define FALLBACK_MAX_GREEN 0x007f00ff
#define FALLBACK_MAX_BLUE  0x00007fff

D2TK_API uint32_t
d2tk_pty_get_max_red(d2tk_pty_t *pty)
{
	if(  (pty->vpty->max_red.r == pty->vpty->max_red.g)
		&& (pty->vpty->max_red.r == pty->vpty->max_red.b) )
	{
		return FALLBACK_MAX_RED;
	}

	return _col_to_uint32(&pty->vpty->max_red);
}

D2TK_API uint32_t
d2tk_pty_get_max_green(d2tk_pty_t *pty)
{
	if(  (pty->vpty->max_green.g == pty->vpty->max_green.r)
		&& (pty->vpty->max_green.g == pty->vpty->max_green.b) )
	{
		return FALLBACK_MAX_GREEN;
	}

	return _col_to_uint32(&pty->vpty->max_green);
}

D2TK_API uint32_t
d2tk_pty_get_max_blue(d2tk_pty_t *pty)
{
	if(  (pty->vpty->max_blue.b == pty->vpty->max_blue.r)
		&& (pty->vpty->max_blue.b == pty->vpty->max_blue.g) )
	{
		return FALLBACK_MAX_BLUE;
	}

	return _col_to_uint32(&pty->vpty->max_blue);
}
