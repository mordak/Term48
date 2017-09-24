
#include <ioctl.h>
#include <unix.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/select.h>

#include <bps/screen.h>
#include <bps/virtualkeyboard.h>
#include <bps/deviceinfo.h>
#include <unicode/utf.h>

#include "SDL.h"
#include "SDL_ttf.h"
#include "SDL_syswm.h"
#include "SDL_thread.h"

#include "terminal.h"
#include "ecma48.h"
#include "preferences.h"
#include "buffer.h"
#include "io.h"
#include "colors.h"

void destroy_symmenu(symmenu_t *menu) {
	for (symkey_t **row = menu->keys; row != NULL; ++row) {
		for (symkey_t *key = *row; key->map != NULL; ++key) {
			free(key->uc);
			free(key);
		}
		free(row);
	}
	
	free(menu->entries);
	SDL_FreeSurface(menu->surface);
}

/* Use the preferences struct to initalize all the SDL stuff for symmenu */
SDL_Surface *render_symmenu(SDL_Surface *screen, pref_t *prefs, symmenu_t *menu) {
	/* get info about menu */
	int num_rows = 0;
	int longest_row_len = 0;
	for (; menu->keys[num_rows] != NULL; ++num_rows) {
		int col_len = 0;
		for (; menu->keys[num_rows][col_len].map != NULL; ++col_len) { }
		if (col_len > longest_row_len) {
			longest_row_len = col_len;
		}
	}
	
	if (menu->keys[0] == NULL) {
		return NULL;
	}

	/* symmenus should all be the same size (and centered, ideally) */
	int bg_font_size = preferences_guess_best_font_size(prefs, 10 * 1.25);
	int corner_font_size = bg_font_size / 5;
	int fg_font_size = (6 * bg_font_size) / 10;

	/* Load the font - if this was going to fail, it would have failed earlier in init() */
	TTF_Font* fg_font = TTF_OpenFont(prefs->font_path, fg_font_size);
	TTF_SetFontStyle(fg_font, TTF_STYLE_NORMAL);
	TTF_SetFontOutline(fg_font, 0);
	TTF_SetFontKerning(fg_font, 0);
	TTF_SetFontHinting(fg_font, TTF_HINTING_NORMAL);

	TTF_Font* bg_font = TTF_OpenFont(prefs->font_path, bg_font_size);
	TTF_SetFontStyle(bg_font, TTF_STYLE_NORMAL);
	TTF_SetFontOutline(bg_font, 0);
	TTF_SetFontKerning(bg_font, 0);
	TTF_SetFontHinting(bg_font, TTF_HINTING_NORMAL);

	TTF_Font* corner_font = TTF_OpenFont(prefs->font_path, corner_font_size);
	TTF_SetFontStyle(corner_font, TTF_STYLE_NORMAL);
	TTF_SetFontOutline(corner_font, 0);
	TTF_SetFontKerning(corner_font, 0);
	TTF_SetFontHinting(corner_font, TTF_HINTING_NORMAL);
	
	/* render a test character to set the proper height */
	UChar testchar;
	io_read_utf8_string("#", 2, &testchar);
	SDL_Surface *testsurf = TTF_RenderUNICODE_Shaded(fg_font, &testchar, (SDL_Color)SYMMENU_FONT, (SDL_Color)SYMMENU_BACKGROUND);
	int bg_h = testsurf->h + (2*SYMKEY_BORDER_SIZE) + SYMMENU_FRET_SIZE;
	SDL_FreeSurface(testsurf);
	int screen_width = atoi(getenv("WIDTH"));
	int bg_w = screen_width / longest_row_len;
	
	/* fill in the symkey entries from prefs keymap*/
	for (int row = 0; menu->keys[row] != NULL; ++row) {
		for (int col = 0; menu->keys[row][col].map != NULL; ++col) {
			symkey_t *sk = &menu->keys[row][col];
			
			/* calculate the background positions */
			sk->hitbox.x = col * bg_w;
			sk->hitbox.y = (screen->h - num_rows * bg_h) + row * bg_h;
			sk->hitbox.w = bg_w;
			sk->hitbox.h = bg_h;
			
			/* init the UChar from prefs keymap */
			int to_len = strlen(sk->map->to);
			sk->uc = (UChar*)calloc(to_len + 1, sizeof(UChar));
			int uc_len = io_read_utf8_string(sk->map->to, to_len, sk->uc);
		}
	}
	
	/* initialize the symmenu surface */
	SDL_Surface *menu_surface = SDL_CreateRGBSurface(0, screen->w, num_rows * bg_h, 24, 0, 0, 0, 0);
	/* render background color */
	SDL_Rect destrect;
	destrect.w = menu_surface->w;
	destrect.h = menu_surface->h;
	destrect.x = 0; destrect.y = 0;
	
	SDL_Color bgc = (SDL_Color)SYMMENU_BACKGROUND;
	Uint32 bg_fill_color = SDL_MapRGB(screen->format, bgc.r, bgc.b, bgc.g);
	
	if (SDL_FillRect(menu_surface, &destrect, bg_fill_color) != 0) {
		fprintf(stderr, "Symmenu bgfill failed: %s\n", SDL_GetError());
		return NULL;
	}

		/* render frets */
	for (int i = 0; i < num_rows; ++i) {
		SDL_Rect destrect;
		destrect.x = 0;
		destrect.y = bg_h * i;
		destrect.h = SYMMENU_FRET_SIZE;
		destrect.w = screen->w;

		SDL_Color bgc = (SDL_Color)SYMMENU_FRET;
		Uint32 fret_fill_color = SDL_MapRGB(screen->format, bgc.r, bgc.b, bgc.g);
	
		if (SDL_FillRect(menu_surface, &destrect, fret_fill_color) != 0) {
			fprintf(stderr, "Symmenu fret bgfill failed: %s\n", SDL_GetError());
			return NULL;
		}

		/* left-to-right borders */
		destrect.x = 0;
		destrect.y = bg_h * i + SYMMENU_FRET_SIZE;
		destrect.h = SYMKEY_BORDER_SIZE;
		destrect.w = screen->w;
		bgc = (SDL_Color)SYMMENU_BORDER;
		fret_fill_color = SDL_MapRGB(screen->format, bgc.r, bgc.b, bgc.g);
		if (SDL_FillRect(menu_surface, &destrect, fret_fill_color) != 0) {
			fprintf(stderr, "Symmenu border bgfill failed: %s\n", SDL_GetError());
			return NULL;
		}
		destrect.x = 0;
		destrect.y = bg_h * (i + 1) - SYMKEY_BORDER_SIZE;
		bgc = (SDL_Color)SYMMENU_BORDER;
		fret_fill_color = SDL_MapRGB(screen->format, bgc.r, bgc.b, bgc.g);
		if (SDL_FillRect(menu_surface, &destrect, fret_fill_color) != 0) {
			fprintf(stderr, "Symmenu border bgfill failed: %s\n", SDL_GetError());
			return NULL;
		}
	}

	/* render the keys */
	UChar cornerchar[2]; cornerchar[1] = NULL;
	for (int row = 0; menu->keys[row] != NULL; ++row) {
		for (int col = 0; menu->keys[row][col].map != NULL; ++col) {
			symkey_t *sk = &menu->keys[row][col];
			SDL_Rect destrect;

			/* main symbol */
			destrect.x = sk->hitbox.x + SYMKEY_BORDER_SIZE;
			destrect.y = sk->hitbox.y - (screen->h - num_rows * bg_h) + SYMKEY_BORDER_SIZE + SYMMENU_FRET_SIZE;
			SDL_Surface *destsurf = TTF_RenderUNICODE_Shaded(fg_font, sk->uc, (SDL_Color)SYMMENU_FONT, (SDL_Color)SYMMENU_BACKGROUND);
			destrect.w = destsurf->w;
			destrect.h = destsurf->h;
			if (SDL_BlitSurface(destsurf, NULL, menu_surface, &destrect) != 0){
				PRINT(stderr, "Blit Failed: %s\n", SDL_GetError());
			}
			SDL_FreeSurface(destsurf);

			/* from key */
			cornerchar[0] = sk->map->from;
			destrect.x = sk->hitbox.x;
			destrect.y = sk->hitbox.y - (screen->h - num_rows * bg_h) + SYMKEY_BORDER_SIZE + SYMMENU_FRET_SIZE;
			destsurf = TTF_RenderUNICODE_Shaded(corner_font, cornerchar, (SDL_Color)SYMMENU_FONT, (SDL_Color)SYMMENU_BACKGROUND);
			destrect.w = destsurf->w;
			destrect.h = destsurf->h;
			if(SDL_BlitSurface(destsurf, NULL, menu_surface, &destrect) != 0){
				PRINT(stderr, "Blit Failed: %s\n", SDL_GetError());
			}
			SDL_FreeSurface(destsurf);
		}
	}

	TTF_CloseFont(fg_font);
	TTF_CloseFont(corner_font);
	TTF_CloseFont(bg_font);

	return menu_surface;
}
