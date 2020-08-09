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
#include <signal.h>

#include <d2tk/frontend_pugl.h>
#include "example/example.h"

typedef struct _app_t app_t;

struct _app_t {
	d2tk_frontend_t *dpugl;
};

static sig_atomic_t done = false;

static void
_sig(int signum __attribute__((unused)))
{
	done = true;
}

static int
_expose(void *data, d2tk_coord_t w, d2tk_coord_t h)
{
	app_t *app = data;
	d2tk_frontend_t *dpugl = app->dpugl;
	d2tk_base_t *base = d2tk_frontend_get_base(dpugl);

	d2tk_example_run(base, w, h);

	return EXIT_SUCCESS;
}

int
main(int argc __attribute__((unused)), char **argv __attribute__((unused)))
{
	static app_t app;

	const float scale = d2tk_frontend_get_scale();
	d2tk_coord_t w = scale * 1280;
	d2tk_coord_t h = scale * 720;

	int c;
	while( (c = getopt(argc, argv, "w:h:")) != -1)
	{
		switch(c)
		{
			case 'w':
			{
				w = atoi(optarg);
			} break;
			case 'h':
			{
				h = atoi(optarg);
			} break;

			default:
			{
				fprintf(stderr, "Usage: %s\n"
					"  -w  width\n"
					"  -h  height\n\n",
					argv[0]);
			} return EXIT_FAILURE;
		}
	}

	const d2tk_pugl_config_t config = {
		.parent = 0,
		.bundle_path = "./",
		.min_w = w/4,
		.min_h = h/4,
		.w = w,
		.h = h,
		.fixed_size = false,
		.fixed_aspect = false,
		.expose = _expose,
		.data = &app
	};

	signal(SIGINT, _sig);
	signal(SIGTERM, _sig);
#if !defined(_WIN32)
	signal(SIGQUIT, _sig);
	signal(SIGKILL, _sig);
#endif

	app.dpugl = d2tk_pugl_new(&config, NULL);
	if(app.dpugl)
	{
		d2tk_example_init();

		d2tk_frontend_run(app.dpugl, &done);

		d2tk_frontend_free(app.dpugl);

		d2tk_example_deinit();

		return EXIT_SUCCESS;
	}

	return EXIT_FAILURE;
}
