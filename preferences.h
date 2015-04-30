/*
 * Copyright (c) 2013 Todd Mortimer
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PREFERENCES_H_
#define PREFERENCES_H_

#include <sys/keycodes.h>
#include <libconfig.h>
#include "SDL.h"

#define PREFS_COLOR_NUM_ELEMENTS 3
#define PREFS_SYMKEYS_DEFAULT_NUM_ROWS 2

static int PREFS_VERSION = 6;

struct symkey_entry {
	const char* name;
	const char* c;
  char flash;
	UChar* uc;
	int x, y, off_x, off_y;
	SDL_Surface* background;
	SDL_Surface* symbol;
	SDL_Surface* flash_symbol;
	SDL_Surface* key;
};

static struct preferences_keys_t {
	char* font_path;
	char* font_size;
	char* text_color;
	char* background_color;
	char* screen_idle_awake;
	char* auto_show_vkb;
	char* metamode_doubletap_key;
	char* metamode_doubletap_delay;
	char* keyhold_actions;
	char* keyhold_actions_exempt;
	char* metamode_hold_key;
	char* metamode_hitbox_s;
	char* metamode_keys;
	char* metamode_sticky_keys;
	char* metamode_func_keys;
	struct hitbox_kt {
			char* x;
			char* y;
			char* w;
			char* h;
	} metamode_hitbox;
	char* sym_keys;
	char* sticky_sym_key;
	char* sticky_shift_key;
	char* sticky_alt_key;
	char* tty_encoding;
	char* prefs_version;
} preference_keys = {
		.font_path = "font_path",
		.font_size = "font_size",
		.text_color = "text_color",
		.background_color = "background_color",
		.screen_idle_awake = "screen_idle_awake",
		.auto_show_vkb = "auto_show_vkb",
		.metamode_doubletap_key = "metamode_doubletap_key",
		.metamode_doubletap_delay = "metamode_doubletap_delay",
		.keyhold_actions = "keyhold_actions",
		.keyhold_actions_exempt = "keyhold_actions_exempt",
		.metamode_hold_key = "metamode_hold_key",
		.metamode_hitbox_s = "metamode_hitbox",
		.metamode_keys = "metamode_keys",
		.metamode_sticky_keys = "metamode_sticky_keys",
		.metamode_func_keys = "metamode_func_keys",
		.metamode_hitbox = {
				.x = "metamode_hitbox.x",
				.y = "metamode_hitbox.y",
				.w = "metamode_hitbox.w",
				.h = "metamode_hitbox.h"
		},
		.sym_keys = "sym_keys",
		.sticky_sym_key = "sticky_sym_key",
		.sticky_shift_key = "sticky_shift_key",
		.sticky_alt_key = "sticky_alt_key",
		.tty_encoding = "tty_encoding",
		.prefs_version = "prefs_version"
};

static struct preference_defaults_t {
	char* font_path;
	int font_size;
	int text_color[PREFS_COLOR_NUM_ELEMENTS];
	int background_color[PREFS_COLOR_NUM_ELEMENTS];
	int screen_idle_awake;
	int auto_show_vkb;
	int metamode_doubletap_key;
	int metamode_doubletap_delay;
	int keyhold_actions;
	int metamode_hold_key;
	struct hitbox_t {
		int x;
		int y;
		int w;
		int h;
	} hitbox;
	char* tty_encoding;
	char* metamode_keys[4];
	char* metamode_sticky_keys[8];
	char* metamode_func_keys[8];
	char* sym_keys[2][20];
	int sticky_sym_key;
	int sticky_shift_key;
	int sticky_alt_key;
	int keyhold_actions_exempt[2];
} preference_defaults = {
		.font_path = "/usr/fonts/font_repository/monotype/cour.ttf",
		.font_size = 40,
		/* must be of length == PREFS_COLOR_NUM_ELEMENTS */
		.text_color = {255, 255, 255},
		.background_color = {0, 0, 0},
		.screen_idle_awake = 0,
		.auto_show_vkb = 1,
		.metamode_doubletap_key = KEYCODE_RIGHT_SHIFT,
		.metamode_doubletap_delay = 500000000,
		.keyhold_actions = 1,
		.metamode_hold_key = KEYCODE_SPACE,
		.hitbox = {0,0,100,100},
		.tty_encoding = "UTF-8",
		/* remember to update array size above when changing */
		.metamode_keys = {"e", "\x1b",
		                  "t", "\x09"},
		/* remember to update array size above when changing */
		.metamode_sticky_keys = {"k", "\x1b\x5b\x41",
														 "j", "\x1b\x5b\x42",
														 "l", "\x1b\x5b\x43",
														 "h", "\x1b\x5b\x44"},
		/* remember to update array size above when changing */
		.metamode_func_keys = {"a", "alt_down",
													 "c", "ctrl_down",
													 "s", "rescreen",
													 "v", "paste_clipboard"},
		/* remember to update array sizes above when changing and PREFS_SYMKEYS_DEFAULT_NUM_ROWS */
		.sym_keys = {{"a", "=", "s", "-", "d", "*", "f", "/", "g", "\\", "h", "|", "j", "&", "k", "'", "l", "\""},
					 {"q", "~", "w", "`", "e", "{", "r", "}", "t", "[", "y", "]", "u", "<", "i", ">", "o", "^", "p", "%"}},
		.sticky_sym_key = 0,
		.sticky_shift_key = 0,
		.sticky_alt_key = 0,
	    /* remember to update array size above when changing */
		.keyhold_actions_exempt = {KEYCODE_BACKSPACE,
				                       KEYCODE_RETURN}
};

config_t* preferences_get();
void preferences_init();
void preferences_uninit();
int preferences_save(config_t* prefs);
int preferences_free(config_t* prefs);

const char* preferences_get_string(char* pref);
const char* preferences_get_metamode_keystrokes(char keystroke, char* pref_key);
const char* preferences_get_metamode_keys(char keystroke);
const char* preferences_get_metamode_sticky_keys(char keystroke);
const char* preferences_get_metamode_func_keys(char keystroke);
int preferences_is_keyhold_exempt(int keystroke);
int preferences_get_int(char* pref);
int preferences_get_bool(char* pref);
int preferences_get_int_array(char* pref, int* fillme, int length);
int preferences_guess_best_font_size(int target_cols);

int preferences_get_sym_num_rows();
int* preferences_get_sym_num_entries();
struct symkey_entry** preferences_get_sym_entries();
#endif /* PREFERENCES_H_ */
