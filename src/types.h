#ifndef _TYPES_H
#define _TYPES_H

#include "SDL.h"

typedef struct _keymap_t {
	char from, *to;
} keymap_t;

typedef struct _symkey_t {
	char flash;
	keymap_t *map;
	int from_x, to_x, from_y, to_y; // used for mousedown
	UChar *uc;
} symkey_t;

typedef struct _symmenu_t {
	symkey_t **keys;
	keymap_t *entries;
	SDL_Surface *surface;
} symmenu_t;

typedef struct _pref_t {
	char *font_path;
	int font_size, *text_color, *background_color, screen_idle_awake,
		auto_show_vkb, metamode_doubletap_key, metamode_doubletap_delay,
		keyhold_actions, metamode_hold_key, allow_resize_columns,
		*metamode_hitbox;
	char *tty_encoding;
	keymap_t *metamode_keys, *metamode_sticky_keys, *metamode_func_keys;
	symmenu_t *main_symmenu;
	int sticky_sym_key, sticky_shift_key, sticky_alt_key, *keyhold_actions_exempt;
} pref_t;

#endif
