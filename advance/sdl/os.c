/*
 * This file is part of the Advance project.
 *
 * Copyright (C) 2001 Andrea Mazzoleni
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. 
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * In addition, as a special exception, Andrea Mazzoleni
 * gives permission to link the code of this program with
 * the MAME library (or with modified versions of MAME that use the
 * same license as MAME), and distribute linked combinations including
 * the two.  You must obey the GNU General Public License in all
 * respects for all of the code used other than MAME.  If you modify
 * this file, you may extend this exception to your version of the
 * file, but you are not obligated to do so.  If you do not wish to
 * do so, delete this exception statement from your version.
 */

#include "os.h"
#include "log.h"
#include "ksdl.h"
#include "msdl.h"
#include "target.h"
#include "file.h"

#include "SDL.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <sys/stat.h>

struct os_context {
	int is_term; /**< Is termination requested */
};

static struct os_context OS;

/***************************************************************************/
/* Clock */

os_clock_t OS_CLOCKS_PER_SEC = 1000;

os_clock_t os_clock(void) {
	return SDL_GetTicks();
}

/***************************************************************************/
/* Init */

#include "icondef.dat"

static void SDL_WM_DefIcon(void) {
	SDL_Surface* surface;
	SDL_Color colors[ICON_PALETTE];
	unsigned i,x,y;

	surface = SDL_CreateRGBSurface(SDL_SWSURFACE, ICON_SIZE, ICON_SIZE, 8, 0, 0, 0, 0);
	if (!surface) {
		log_std(("os: SDL_WM_DefIcon() failed in SDL_CreateRGBSurface\n"));
		return;
	}

	for(y=0;y<ICON_SIZE;++y) {
		unsigned char* p = (unsigned char*)surface->pixels + y * surface->pitch;
		for(x=0;x<ICON_SIZE;++x)
			p[x] = icon_pixel[y*ICON_SIZE+x];
	}

	for(i=0;i<ICON_PALETTE;++i) {
		colors[i].r = icon_palette[i*3+0];
		colors[i].g = icon_palette[i*3+1];
		colors[i].b = icon_palette[i*3+2];
	}

	if (SDL_SetColors(surface, colors, 0, ICON_PALETTE) != 1) {
		log_std(("os: SDL_WM_DefIcon() failed in SDL_SetColors\n"));
		SDL_FreeSurface(surface);
		return;
	}

	SDL_WM_SetIcon(surface, icon_mask);

	SDL_FreeSurface(surface);
}

int os_init(struct conf_context* context) {
	memset(&OS,0,sizeof(OS));

	return 0;
}

void os_done(void) {
}

static void os_term_signal(int signum) {
	OS.is_term = 1;
}

int os_inner_init(const char* title) {
	SDL_version compiled;

	/* the SDL_INIT_VIDEO flags must be specified also if the video */
	/* output isn't used */
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE) != 0) {
		log_std(("os: SDL_Init() failed, %s\n", SDL_GetError()));
		return -1;
	}

	SDL_VERSION(&compiled);

	log_std(("os: compiled with sdl %d.%d.%d\n", compiled.major, compiled.minor, compiled.patch));
	log_std(("os: linked with sdl %d.%d.%d\n", SDL_Linked_Version()->major, SDL_Linked_Version()->minor, SDL_Linked_Version()->patch));
	if (SDL_BYTEORDER == SDL_LIL_ENDIAN)
		log_std(("os: little endian system\n"));
	else
		log_std(("os: big endian system\n"));

	/* set the titlebar */
	SDL_WM_SetCaption(title,title);
	SDL_WM_DefIcon();

	/* set some signal handlers */
	signal(SIGABRT, os_signal);
	signal(SIGFPE, os_signal);
	signal(SIGILL, os_signal);
	signal(SIGINT, os_signal);
	signal(SIGSEGV, os_signal);
	signal(SIGTERM, os_term_signal);

	return 0;
}

void os_inner_done(void) {
	SDL_Quit();
}

void os_poll(void) {
	SDL_Event event;

	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_KEYDOWN :
#ifdef USE_KEYBOARD_SDL
				keyb_sdl_event_press(event.key.keysym.sym);
#endif

				/* toggle fullscreen check */
				if (event.key.keysym.sym == SDLK_RETURN
					&& (event.key.keysym.mod & KMOD_ALT) != 0) {
					if (SDL_WasInit(SDL_INIT_VIDEO) && SDL_GetVideoSurface()) {
						SDL_WM_ToggleFullScreen(SDL_GetVideoSurface());

						if ((SDL_GetVideoSurface()->flags & SDL_FULLSCREEN) != 0) {
							SDL_ShowCursor(SDL_DISABLE);
						} else {
							SDL_ShowCursor(SDL_ENABLE);
						}
					}
				}
			break;
			case SDL_KEYUP :
#ifdef USE_KEYBOARD_SDL
				keyb_sdl_event_release(event.key.keysym.sym);
#endif
			break;
			case SDL_MOUSEMOTION :
#ifdef USE_MOUSE_SDL
				mouseb_sdl_event_move(event.motion.xrel, event.motion.yrel);
#endif
			break;
			case SDL_MOUSEBUTTONDOWN :
#ifdef USE_MOUSE_SDL
				if (event.button.button > 0)
					mouseb_sdl_event_press(event.button.button-1);
#endif
			break;
			case SDL_MOUSEBUTTONUP :
#ifdef USE_MOUSE_SDL
				if (event.button.button > 0)
					mouseb_sdl_event_release(event.button.button-1);
#endif
			break;
			case SDL_QUIT :
				OS.is_term = 1;
				break;
		}
	}
}

/***************************************************************************/
/* Led */

void os_led_set(unsigned mask)
{
}

/***************************************************************************/
/* Signal */

int os_is_term(void) {
	return OS.is_term;
}

void os_default_signal(int signum)
{
	log_std(("os: signal %d\n", signum));

#if defined(USE_VIDEO_SDL)
	log_std(("os: video_abort\n"));
	{
		extern void video_abort(void);
		video_abort();
	}
#endif

#if defined(USE_SOUND_SDL)
	log_std(("os: sound_abort\n"));
	{
		extern void sound_abort(void);
		sound_abort();
	}
#endif

	SDL_Quit();

	target_mode_reset();

	log_std(("os: close log\n"));
	log_abort();

	if (signum == SIGINT) {
		fprintf(stderr,"Break pressed\n\r");
		exit(EXIT_FAILURE);
	} else {
		fprintf(stderr,"AdvanceMAME signal %d.\n",signum);
		fprintf(stderr,"%s, %s\n\r", __DATE__, __TIME__);

		if (signum == SIGILL) {
			fprintf(stderr,"Are you using the correct binary ?\n");
		}

		_exit(EXIT_FAILURE);
	}
}

/***************************************************************************/
/* Main */

int main(int argc, char* argv[])
{
	if (target_init() != 0)
		return EXIT_FAILURE;

	if (file_init() != 0) {
		target_done();
		return EXIT_FAILURE;
	}

	if (os_main(argc,argv) != 0) {
		file_done();
		target_done();		
		return EXIT_FAILURE;
	}
		
	file_done();
	target_done();
	
	return EXIT_SUCCESS;
}
