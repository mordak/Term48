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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <unicode/utf.h>

#include "terminal.h"
#include "preferences.h"

pref_t *prefs = NULL;

int preferences_guess_best_font_size(int target_cols){
	/* font widths in pixels for sizes 0-250, indexed by font size */
	int screen_width, screen_height, target_width;
	if((getenv("WIDTH") == NULL) || (getenv("HEIGHT") == NULL)){
		/* no width or height in env, just return the default */
		if (prefs == NULL) {
			PRINT(stderr, "Preferences not initalized!\n");
			return 10;
		}
		return prefs->font_size;
	}
	screen_width = atoi(getenv("WIDTH"));
	screen_height = atoi(getenv("HEIGHT"));
	target_width = screen_width < screen_height ? screen_width : screen_height;
	int num_px = 0;
	for (int i = 0; i < NUM_SIZES; ++i){
		num_px = target_cols * font_widths[i];
		if(num_px > target_width){
			/* if we are too big, return the last one. */
			PRINT(stderr, "Autodetected font size %d for screen width %d\n", (i - 1), target_width);
			return (i - 1);
		}
	}
	/* if we get here, then just return the largest font */
	return font_widths[NUM_SIZES-1];
}

static int* create_int_array(config_t const *config, char const *path, size_t def_len, int const *def, int dynamic) {
	config_setting_t *setting = config_lookup(config, path);
	int use_default = 0;
	size_t source_len = 0;

	if (!setting || (config_setting_type(setting) != CONFIG_TYPE_ARRAY)) {
		fprintf(stderr, "invalid array %s, using default\n", path);
		source_len = def_len;
		use_default = 1;
	} else {
		source_len = config_setting_length(setting);
	}
	
	int *result = calloc(source_len + 1, sizeof(int));
	result[source_len] = -1;  // sentinel for end of array

	if (use_default) {
		for (int i = 0; i < source_len; i++) {
			result[i] = def[i];
		}
	} else {
		for (int i = 0; i < source_len; i++) {
			result[i] = config_setting_get_int_elem(setting, i);
		}
	}

	return result;
}

static keymap_t* create_keymap_array(config_t const *config, char const *path, size_t def_len, keymap_t const *def) {
	config_setting_t *setting = config_lookup(config, path);
	int use_default = 0;
	size_t source_len = 0;

	if (!setting || (config_setting_type(setting) != CONFIG_TYPE_LIST)) {
		fprintf(stderr, "invalid keymap list %s, using default\n", path);
		source_len = def_len;
		use_default = 1;
	} else {
		source_len = config_setting_length(setting);
	}
	
	keymap_t *result = calloc(source_len + 1, sizeof(keymap_t));
	result[source_len] = (keymap_t){0, NULL}; // sentinel for end of array
	
	if (use_default) {
		for (int i = 0; i < source_len; i++) {
			result[i].from = def[i].from;
			result[i].to = strdup(def[i].to);
		}
	} else {
		for (int i = 0; i < source_len; i++) {
			config_setting_t *m = config_setting_get_elem(setting, i);
			result[i].from = config_setting_get_int_elem(m, 0);
			result[i].to = strdup(config_setting_get_string_elem(m, 1));
		}
	}

	return result;
}

void destroy_preferences(pref_t *pref) {
	free(pref->font_path);
	free(pref->tty_encoding);

	free(pref->text_color);
	free(pref->background_color);
	free(pref->metamode_hitbox);
	
	keymap_t *m = pref->metamode_keys;
	while (m->to != NULL) { free(m->to); ++m; }
	free(pref->metamode_keys);
	
	m = pref->metamode_sticky_keys;
	while (m->to != NULL) { free(m->to); ++m; }
	free(pref->metamode_sticky_keys);
	
	m = pref->metamode_func_keys;
	while (m->to != NULL) { free(m->to); ++m; }
	free(pref->metamode_func_keys);

	for (int i = 0; i < 3; i++) {
		m = pref->sym_keys[i];
		while (m->to != NULL) { free(m->to); ++m; }
		free(pref->sym_keys[i]);
	}
	free(pref->sym_keys);
	
	free(pref->keyhold_actions_exempt);

	free(pref);
}

#define DEFAULT_LOOKUP(type, conf, path, target, defval) \
	do { if (CONFIG_TRUE != config_lookup_##type(conf, path, &(target))) { target = defval; } } while(0)
pref_t *read_preferences(const char* filename) {
	pref_t *prefs = calloc(1, sizeof(pref_t)); // our internal data structure
	if (prefs == NULL) {
		fprintf(stderr, "fatal error: failed to calloc prefs structure\n");
		exit(1);
	}
	
	config_t config_data; // what libconfig parses out of the file
	config_t *config = &config_data;
	config_init(config);
	
	if (access(filename, F_OK) == -1) {
		fprintf(stderr, "Preferences file not found\n");
	} else {
		if(config_read_file(config, filename) != CONFIG_TRUE){
			fprintf(stderr, "%s:%d - %s\n", config_error_file(config),
			        config_error_line(config), config_error_text(config));
		}
	}
	
	int default_font_columns = (atoi(getenv("WIDTH")) <= 720) ? 45 : 60;

	DEFAULT_LOOKUP(string, config, "font_path", prefs->font_path, DEFAULT_FONT_PATH);
	prefs->font_path = strdup(prefs->font_path);
	DEFAULT_LOOKUP(int, config, "font_size", prefs->font_size, DEFAULT_FONT_SIZE);
	prefs->text_color = create_int_array(config, "text_color", PREFS_COLOR_NUM_ELEMENTS, DEFAULT_TEXT_COLOR, 0);
	prefs->background_color = create_int_array(config, "background_color", PREFS_COLOR_NUM_ELEMENTS, DEFAULT_BACKGROUND_COLOR, 0);
	DEFAULT_LOOKUP(int, config, "screen_idle_awake", prefs->screen_idle_awake, DEFAULT_SCREEN_IDLE_AWAKE);
	DEFAULT_LOOKUP(bool, config, "auto_show_vkb", prefs->auto_show_vkb, DEFAULT_AUTO_SHOW_VKB);
	DEFAULT_LOOKUP(int, config, "metamode_doubletap_key", prefs->metamode_doubletap_key, DEFAULT_METAMODE_DOUBLETAP_KEY);
	DEFAULT_LOOKUP(int, config, "metamode_doubletap_delay", prefs->metamode_doubletap_delay, DEFAULT_METAMODE_DOUBLETAP_DELAY);
	DEFAULT_LOOKUP(int, config, "keyhold_actions", prefs->keyhold_actions, DEFAULT_KEYHOLD_ACTIONS);
	DEFAULT_LOOKUP(int, config, "metamode_hold_key", prefs->metamode_hold_key, DEFAULT_METAMODE_HOLD_KEY);
	DEFAULT_LOOKUP(bool, config, "allow_resize_columns", prefs->allow_resize_columns, DEFAULT_ALLOW_RESIZE_COLUMNS);
	prefs->metamode_hitbox = create_int_array(config, "metamode_hitbox", 4, DEFAULT_METAMODE_HITBOX, 0);
	DEFAULT_LOOKUP(string, config, "tty_encoding", prefs->tty_encoding, DEFAULT_TTY_ENCODING);
	prefs->tty_encoding = strdup(prefs->tty_encoding);
	prefs->metamode_keys = create_keymap_array(config, "metamode_keys", DEFAULT_METAMODE_KEYS_LEN, DEFAULT_METAMODE_KEYS);
	prefs->metamode_sticky_keys = create_keymap_array(config, "metamode_sticky_keys", DEFAULT_METAMODE_STICKY_KEYS_LEN, DEFAULT_METAMODE_STICKY_KEYS);
	prefs->metamode_func_keys = create_keymap_array(config, "metamode_func_keys", DEFAULT_METAMODE_FUNC_KEYS_LEN, DEFAULT_METAMODE_FUNC_KEYS);
	prefs->sym_keys[0] = create_keymap_array(config, "sym_keys_r1", DEFAULT_SYM_KEYS_R1_LEN, DEFAULT_SYM_KEYS_R1);
	prefs->sym_keys[1] = create_keymap_array(config, "sym_keys_r2", DEFAULT_SYM_KEYS_R2_LEN, DEFAULT_SYM_KEYS_R2);
	prefs->sym_keys[2] = create_keymap_array(config, "sym_keys_r3", DEFAULT_SYM_KEYS_R3_LEN, DEFAULT_SYM_KEYS_R3);
	DEFAULT_LOOKUP(bool, config, "sticky_sym_key", prefs->sticky_sym_key, DEFAULT_STICKY_SYM_KEY);
	DEFAULT_LOOKUP(bool, config, "sticky_shift_key", prefs->sticky_shift_key, DEFAULT_STICKY_SHIFT_KEY);
	DEFAULT_LOOKUP(bool, config, "sticky_alt_key", prefs->sticky_alt_key, DEFAULT_STICKY_ALT_KEY);
	prefs->keyhold_actions_exempt = create_int_array(config, "keyhold_actions_exempt", DEFAULT_KEYHOLD_ACTIONS_EXEMPT_LEN, DEFAULT_KEYHOLD_ACTIONS_EXEMPT, 1);

	return prefs;
}

void save_preferences(pref_t const* pref, char const* filename) {

	fprintf(stderr, "preferences saving not implemented yet\n");
	return;
	/* config_write_file(preferences, preferences_filename); */
}

int preferences_is_keyhold_exempt(int keystroke){
	int *k = prefs->keyhold_actions_exempt;
	while (k != NULL) {
		if (*k == keystroke) {
			return 1;
		}
	}
	
	return 0;
}

const char* keystroke_lookup(char keystroke, keymap_t *keymap_head) {
	while (keymap_head->to != NULL) {
		if (keymap_head->from == keystroke) {
			return keymap_head->to;
		}
		++keymap_head;
	}
	return NULL;
}
