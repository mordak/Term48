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


static struct preferences_keys_t {
	char* font_path;
	char* font_size;
	char* screen_idle_awake;
	char* metamode_doubletap_key;
	char* metamode_doubletap_delay;
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
	char* tty_encoding;
} preference_keys = {
		.font_path = "font_path",
		.font_size = "font_size",
		.screen_idle_awake = "screen_idle_awake",
		.metamode_doubletap_key = "metamode_doubletap_key",
		.metamode_doubletap_delay = "metamode_doubletap_delay",
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
		.tty_encoding = "tty_encoding"
};

static struct preference_defaults_t {
	char* font_path;
	int font_size;
	int screen_idle_awake;
	int metamode_doubletap_key;
	int metamode_doubletap_delay;
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
} preference_defaults = {
		.font_path = "/usr/fonts/font_repository/monotype/cour.ttf",
		.font_size = 16,
		.screen_idle_awake = 0,
		.metamode_doubletap_key = KEYCODE_RIGHT_SHIFT,
		.metamode_doubletap_delay = 500000000,
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
													 "v", "paste_clipboard"}
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
int preferences_get_int(char* pref);
int preferences_get_bool(char* pref);

#endif /* PREFERENCES_H_ */
