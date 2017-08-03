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

static void first_run(pref_t *prefs) {
	char* home = getenv("HOME");
	if(home != NULL){ chdir(home); }
	
	char* readme_path = (atoi(getenv("WIDTH")) <= 720) ? README45_FILE_PATH : README_FILE_PATH;
	fprintf(stderr, "Updating README\n");
	if (access(readme_path, F_OK) != -1) {
		// stat success!
		if (symlink(readme_path, "./README") == -1){
			if (errno != EEXIST){
				fprintf(stderr, "Error linking README from app to PWD\n");
			}
		}
	}

	save_preferences(prefs, PREFS_FILE_PATH);
}

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
	result[source_len] = -1;  // sentinel for end of array, only useful for positive arrays

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
			char const *from_str = config_setting_get_string_elem(m, 0);
			result[i].from = from_str[0];
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

	int is_first_run = 0;
	
	config_t config_data; // what libconfig parses out of the file
	config_t *config = &config_data;
	config_init(config);
	
	if (access(filename, F_OK) == -1) {
		PRINT(stderr, "Preferences file not found, assuming first run\n");
		is_first_run = 1;
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
	DEFAULT_LOOKUP(bool, config, "screen_idle_awake", prefs->screen_idle_awake, DEFAULT_SCREEN_IDLE_AWAKE);
	DEFAULT_LOOKUP(bool, config, "auto_show_vkb", prefs->auto_show_vkb, DEFAULT_AUTO_SHOW_VKB);
	DEFAULT_LOOKUP(int, config, "metamode_doubletap_key", prefs->metamode_doubletap_key, DEFAULT_METAMODE_DOUBLETAP_KEY);
	DEFAULT_LOOKUP(int, config, "metamode_doubletap_delay", prefs->metamode_doubletap_delay, DEFAULT_METAMODE_DOUBLETAP_DELAY);
	DEFAULT_LOOKUP(bool, config, "keyhold_actions", prefs->keyhold_actions, DEFAULT_KEYHOLD_ACTIONS);
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

	if (is_first_run) {
		first_run(prefs);
	}

	return prefs;
}

void set_int_array(config_setting_t *root, char const *key, size_t num_elems, int const *source) {
	config_setting_t *setting = config_setting_add(root, key, CONFIG_TYPE_ARRAY);
	for (size_t i = 0; i < num_elems; ++i) {
		config_setting_t *elem = config_setting_add(setting, NULL, CONFIG_TYPE_INT);
		config_setting_set_int(elem, source[i]);
	}
}

void set_keymap_array(config_setting_t *root, char const *key, keymap_t const *source) {
	config_setting_t *setting = config_setting_add(root, key, CONFIG_TYPE_ARRAY);
	for (; source->to != NULL; ++source) {
		config_setting_t *group = config_setting_add(setting, NULL, CONFIG_TYPE_GROUP);
		
		config_setting_t *from_s = config_setting_add(group, NULL, CONFIG_TYPE_STRING);
		char from_str[2] = {source->from, '\0'};
		config_setting_set_string(from_s, from_str);

		config_setting_t *to_s = config_setting_add(group, NULL, CONFIG_TYPE_STRING);
		config_setting_set_string(to_s, source->to);
	}
}

#define PREF_SET(root, setptr, key, type, configtype, source) \
	do { setptr = config_setting_add(root, key, CONFIG_TYPE_##configtype); \
		config_setting_set_##type(setptr, source); } while(0)
void save_preferences(pref_t const* prefs, char const* filename) {
	config_t config;
	config_init(&config);
	config_setting_t *root = config_root_setting(&config);
	config_setting_t *setting = NULL;

	PREF_SET(root, setting, "font_path", string, STRING, prefs->font_path);
	PREF_SET(root, setting, "font_size", int, INT, prefs->font_size);
	set_int_array(root, "text_color", PREFS_COLOR_NUM_ELEMENTS, prefs->text_color);
	set_int_array(root, "background_color", PREFS_COLOR_NUM_ELEMENTS, prefs->background_color);
	PREF_SET(root, setting, "screen_idle_awake", bool, BOOL, prefs->screen_idle_awake);
	PREF_SET(root, setting, "auto_show_vkb", bool, BOOL, prefs->auto_show_vkb);
	PREF_SET(root, setting, "metamode_doubletap_key", int, INT, prefs->metamode_doubletap_key);
	PREF_SET(root, setting, "metamode_doubletap_delay", int, INT, prefs->metamode_doubletap_delay);
	PREF_SET(root, setting, "keyhold_actions", bool, BOOL, prefs->keyhold_actions);
	PREF_SET(root, setting, "metamode_hold_key", bool, BOOL, prefs->metamode_hold_key);
	PREF_SET(root, setting, "allow_resize_columns", bool, BOOL, prefs->allow_resize_columns);
	set_int_array(root, "metamode_hitbox", 4, prefs->metamode_hitbox);
	PREF_SET(root, setting, "tty_encoding", string, STRING, prefs->tty_encoding);
	set_keymap_array(root, "metamode_keys", prefs->metamode_keys);
	set_keymap_array(root, "metamode_sticky_keys", prefs->metamode_sticky_keys);
	set_keymap_array(root, "metamode_func_keys", prefs->metamode_func_keys);
	set_keymap_array(root, "sym_keys_r1", prefs->sym_keys[0]);
	set_keymap_array(root, "sym_keys_r2", prefs->sym_keys[1]);
	set_keymap_array(root, "sym_keys_r3", prefs->sym_keys[2]);
	PREF_SET(root, setting, "sticky_sym_key", bool, BOOL, prefs->sticky_sym_key);
	PREF_SET(root, setting, "sticky_shift_key", bool, BOOL, prefs->sticky_shift_key);
	PREF_SET(root, setting, "sticky_alt_key", bool, BOOL, prefs->sticky_alt_key);
	
	int num_exempt = 0;
	for (; prefs->keyhold_actions_exempt[num_exempt] > 0; ++num_exempt) { }
	set_int_array(root, "keyhold_actions_exempt", num_exempt, prefs->keyhold_actions_exempt);
	
	config_write_file(&config, filename);
	return;
}

int is_int_member(int const* list, int target) {
	for (int i = 0; list[i] != -1; ++i) {
		if (list[i] == target) {
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
