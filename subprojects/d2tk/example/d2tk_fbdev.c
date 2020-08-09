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

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <signal.h>

#include <d2tk/frontend_fbdev.h>
#include "example/example.h"

#define AUTO "auto"

typedef struct _app_t app_t;
typedef void (*foreach_t)(const char *path, const char *d_name);

struct _app_t {
	d2tk_frontend_t *fbdev;
	bool show_cursor;
};

static sig_atomic_t done = false;

static void
_sig(int signum __attribute__((unused)))
{
	done = true;
}

static inline int
_expose(void *data, d2tk_coord_t w, d2tk_coord_t h)
{
	app_t *app = data;
	d2tk_frontend_t *fbdev = app->fbdev;
	d2tk_base_t *base = d2tk_frontend_get_base(fbdev);

	d2tk_example_run(base, w, h);

	if(app->show_cursor)
	{
		d2tk_coord_t x = 0;
		d2tk_coord_t y = 0;

		d2tk_base_get_mouse_pos(base, &x, &y);
		d2tk_base_cursor(base, &D2TK_RECT(x, y, 24, 24));
	}

	return EXIT_SUCCESS;
}

static void
_state(const char *path, const char *d_name, int32_t state)
{
	char buf [PATH_MAX];

	snprintf(buf, sizeof(buf), "%s/%s/bind", path, d_name);

	FILE *f = fopen(buf, "w");
	if(f)
	{
		fprintf(f, "%"PRIi32, state);
		fflush(f);
		fclose(f);
	}
}

static void
_unbind(const char *path, const char *d_name)
{
	_state(path, d_name, 0);
}

static void
_bind(const char *path, const char *d_name)
{
	_state(path, d_name, 1);
}

static void
_find_by_format_foreach(const char *path, const char *fmt, void *val,
	foreach_t foreach)
{
	DIR *d = opendir(path);
	if(d)
	{
		struct dirent *dir;

		while( (dir = readdir(d)) )
		{
			if(!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, ".."))
			{
				continue;
			}

			if(sscanf(dir->d_name, fmt, val) == 1)
			{
				foreach(path, dir->d_name);
			}
		}

		closedir(d);
	}
}

static int
_find_by_format(const char *path, const char *fmt, void *val, char *res)
{
	int ret = EXIT_FAILURE;

	DIR *d = opendir(path);
	if(d)
	{
		struct dirent *dir;

		while( (dir = readdir(d)) )
		{
			if(!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, ".."))
			{
				continue;
			}

			if(sscanf(dir->d_name, fmt, val) == 1)
			{
				snprintf(res, PATH_MAX, "%s/%s", path, dir->d_name);
				ret = EXIT_SUCCESS;
				break;
			}
		}

		closedir(d);
	}

	return ret;
}

int
main(int argc, char **argv)
{
	static app_t app;
	static char fb_device [PATH_MAX] = AUTO;

	int c;
	while( (c = getopt(argc, argv, "f:c")) != -1)
	{
		switch(c)
		{
			case 'f':
			{
				strncpy(fb_device, optarg, PATH_MAX-1);
			} break;
			case 'c':
			{
				app.show_cursor = true;
			} break;

			default:
			{
				fprintf(stderr, "Usage: %s\n"
					"  -f  fb_device    (auto)\n"
					"  -c               show cursor\n\n",
					argv[0]);
			} return EXIT_FAILURE;
		}
	}

	if(!strcmp(fb_device, AUTO))
	{
		uint32_t num;

		if(_find_by_format("/dev", "fb%"SCNu32, &num, fb_device))
		{
			return EXIT_FAILURE;
		}
	}

	fprintf(stdout,
		"  fb_device:    %s\n\n",
		fb_device);

	const d2tk_fbdev_config_t config = {
		.fb_device = fb_device,
		.bundle_path = "./",
		.expose = _expose,
		.data = &app
	};

	signal(SIGINT, _sig);
	signal(SIGTERM, _sig);
#if !defined(_WIN32)
	signal(SIGQUIT, _sig);
	signal(SIGKILL, _sig);
#endif

	app.fbdev = d2tk_fbdev_new(&config);
	if(app.fbdev)
	{
		uint32_t num;

		_find_by_format_foreach("/sys/class/vtconsole", "vtcon%"SCNu32, &num,
			_unbind);

		d2tk_example_init();

		d2tk_frontend_run(app.fbdev, &done);

		d2tk_frontend_free(app.fbdev);

		d2tk_example_deinit();

		_find_by_format_foreach("/sys/class/vtconsole", "vtcon%"SCNu32, &num,
			_bind);

		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}
