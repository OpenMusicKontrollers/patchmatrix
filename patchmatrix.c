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

#include <sys/wait.h>

#define NSMC_IMPLEMENTATION
#include <nsmc/nsmc.h>

#include <patchmatrix.h>
#include <patchmatrix_jack.h>
#include <patchmatrix_nk.h>

#define NK_PUGL_API
#define NK_PUGL_IMPLEMENTATION
#include <nk_pugl/nk_pugl.h>

static app_t app;
static atomic_bool done = ATOMIC_VAR_INIT(false);

static void
_sig_interrupt(int signum)
{
	_ui_signal(&app);
	atomic_store_explicit(&app.done, true, memory_order_release);
}

static void
_sig_child(int signum)
{
	const pid_t any_child = -1;

	while(waitpid(any_child, 0, WNOHANG) > 0)
	{
		// reap zombies
	}
}

static int
_nsm_callback(void *data, const nsmc_event_t *ev)
{
	app_t *app = data;
	(void)app; //FIXME

	switch(ev->type)
	{
		//FIXME
		default:
		{
			// nothing to do
		} break;
	}

	return 0;
}

int
main(int argc, char **argv)
{
	atomic_init(&app.done, false);

	app.scale = 1.f;
	app.nxt_source = 30; //FIXME make dependent on widget height
	app.nxt_sink = 720/2;
	app.nxt_default = 30;

	app.server_name = NULL;

	fprintf(stderr,
		"%s "PATCHMATRIX_VERSION"\n"
		"Copyright (c) 2016-2020 Hanspeter Portner (dev@open-music-kontrollers.ch)\n"
		"Released under Artistic License 2.0 by Open Music Kontrollers\n", argv[0]);

	int c;
	while((c = getopt(argc, argv, "vhn:d:")) != -1)
	{
		switch(c)
		{
			case 'v':
				fprintf(stderr,
					"--------------------------------------------------------------------\n"
					"This is free software: you can redistribute it and/or modify\n"
					"it under the terms of the Artistic License 2.0 as published by\n"
					"The Perl Foundation.\n"
					"\n"
					"This source is distributed in the hope that it will be useful,\n"
					"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
					"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the\n"
					"Artistic License 2.0 for more details.\n"
					"\n"
					"You should have received a copy of the Artistic License 2.0\n"
					"along the source as a COPYING file. If not, obtain it from\n"
					"http://www.perlfoundation.org/artistic_license_2_0.\n\n");
				return 0;
			case 'h':
				fprintf(stderr,
					"--------------------------------------------------------------------\n"
					"USAGE\n"
					"   %s [OPTIONS]\n"
					"\n"
					"OPTIONS\n"
					"   [-v]                 print version and full license information\n"
					"   [-h]                 print usage information\n"
					"   [-n] server-name     connect to named JACK daemon\n\n"
					, argv[0]);
				return 0;
			case 'n':
				app.server_name = optarg;
				break;
			case '?':
				if( (optopt == 'n') )
					fprintf(stderr, "Option `-%c' requires an argument.\n", optopt);
				else if(isprint(optopt))
					fprintf(stderr, "Unknown option `-%c'.\n", optopt);
				else
					fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
				return -1;
			default:
				return -1;
		}
	}

	const char *exe = strrchr(argv[0], '/');
	exe = exe ? exe + 1 : argv[0];
	app.nsm = nsmc_new("PATCHMATRIX", exe, argv[optind], _nsm_callback, &app);

	if(!app.nsm)
	{
		fprintf(stderr, "[%s] nsmc_new failed\n", __func__);
		return 1;
	}

	while(!atomic_load(&done))
	{
		if(nsmc_managed())
		{
			nsmc_pollin(app.nsm, 1000);
		}
		else
		{
			sleep(1);
		}
	}

	nsmc_free(app.nsm);

	signal(SIGINT, _sig_interrupt);
	signal(SIGCHLD, _sig_child);

	if(!(app.from_jack = varchunk_new(0x10000, true)))
		goto cleanup;

	if(_ui_init(&app))
		goto cleanup;

	if(_jack_init(&app))
		goto cleanup;

	while(!atomic_load_explicit(&app.done, memory_order_acquire))
	{
		if(!app.animating)
		{
			nk_pugl_wait_for_event(&app.win);
		}
		else
		{
			usleep(1000000 / 25); //FIXME
			nk_pugl_post_redisplay(&app.win);
		}

		if(  _jack_anim(&app)
			|| nk_pugl_process_events(&app.win) )
		{
			atomic_store_explicit(&app.done, true, memory_order_release);
		}
	}

cleanup:
	_jack_deinit(&app);

	if(app.from_jack)
	{
		_jack_anim(&app); // drain ringbuffer
		varchunk_free(app.from_jack);
	}

	_ui_deinit(&app);

	if(app.root)
		cJSON_Delete(app.root);

	return 0;
}
