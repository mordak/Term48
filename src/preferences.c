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
#include <unicode/utf.h>

#include "terminal.h"
#include "preferences.h"

static char* preferences_filename = ".term48rc";

int preferences_guess_best_font_size(int target_cols){
	/* font widths in pixels for sizes 0-250, indexed by font size */
	int screen_width, screen_height, target_width;
	if((getenv("WIDTH") == NULL) || (getenv("HEIGHT") == NULL)){
		/* no width or height in env, just return the default */
		return preference_defaults.font_size;
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
	return (num_sizes - 1);
}

#define DEFAULT_LOOKUP(type, root, path, dest, defval) \
	if (config_lookup_##type(root, path, &dest)) { dest = defval; }

pref_t *preferences_init(const char* filename) {
	pref_t *prefs = malloc(sizeof(pref_t)); // our internal data structure
	config_t config; // what libconfig parses out of the file
	config_init(config);
	
	if (access(filename, F_OK) == -1) {
		PRINT(stderr, "Preferences file not found\n");
	} else {
		if(config_read_file(config, filename) != CONFIG_TRUE){
			fprintf(stderr, "%s:%d - %s\n", config_error_file(config),
			        config_error_line(config), config_error_text(config));
		}
	}
	
	config_setting_t root = config_root_setting(preferences);
	int default_font_columns = (atoi(getenv("WIDTH")) <= 720) ? 45 : 60;

	DEFAULT_LOOKUP(string, root, "font_path", prefs->font_path, DEFAULT_FONT_PATH);
	DEFAULT_LOOKUP(int, root, "font_size", prefs->font_size, DEFAULT_FONT_SIZE);
	DEFAULT_LOOKUP(int, root, "screen_idle_awake", prefs->screen_idle_awake, DEFAULT_SCREEN_IDLE_AWAKE);
	DEFAULT_LOOKUP(bool, root, "auto_show_vkb", prefs->auto_show_vkb, DEFAULT_AUTO_SHOW_VKB);
	DEFAULT_LOOKUP(bool, root, "allow_resize_columns", prefs->allow_resize_columns, DEFAULT_ALLOW_RESIZE_COLUMNS);
	DEFAULT_LOOKUP(bool, root, "keyhold_actions", prefs->keyhold_actions, DEFAULT_KEYHOLD_ACTIONS);
	DEFAULT_LOOKUP(bool, root, "sticky_sym_key", prefs->sticky_sym_key, DEFAULT_STICKY_SYM_KEY);
	DEFAULT_LOOKUP(bool, root, "sticky_shift_key", prefs->sticky_shift_key, DEFAULT_STICKY_SHIFT_KEY);
	DEFAULT_LOOKUP(bool, root, "sticky_alt_key", prefs->sticky_alt_key, DEFAULT_STICKY_ALT_KEY);
	preferences_init_int(root, "metamode_hold_key", prefs->metamode_hold_key, DEFAULT_METAMODE_HOLD_KEY);
	preferences_init_int(root, "metamode_doubletap_key", prefs->metamode_doubletap_key, DEFAULT_METAMODE_DOUBLETAP_EY);
	preferences_init_int(root, "metamode_doubletap_delay", prefs->metamode_doubletap_delay, DEFAULT_METAMODE_DOUBLETAP_DELAY);
	preferences_init_string(root, "tty_encoding", prefs->tty_encoding, DEFAULT_TTY_ENCODING);

	config_setting_t *hitbox_s = config_lookup(root, "metamode_hitbox");
	if (config_settings_length(hitbox_settings) == 4) {
		for (int i = 0; i < 4; i++) {
			prefs->metamode_hitbox[i] = config_settings_get_int_elem(hitbox_settings, i);
		}
	} else {
		PRINT(stderr, "invalid metamode_hitbox array, using default\n");
		prefs->metamode_hitbox = DEFAULT_METAMODE_HITBOX;
	}

	config_setting_t khex_s = config_setting_get_member(root, "keyhold_actions_exempt");
	if (!khex_s || config_setting_type(khex_s) != CONFIG_TYPE_ARRAY) {
		PRINT(stderr, "invalid keyhold_actions_exempt array, using default\n");
		prefs->keyhold_actions_exempt = DEFAULT_KEYHOLD_ACTIONS_EXEMPT;
	} else {
		size_t khex_len = config_settings_length(khex_s);
		prefs->keyhold_actions_exempt = malloc((khex_len + 1) * sizeof(int));
		for (int i = 0; i < khex_len; i++) {
			prefs->keyhold_actions_exempt[i] = config_settings_get_int_elem(khex_s, i);
		}
		prefs->keyhold_actions_exempt[khex_len] = NULL;
	}

	config_setting_t color_s = config_setting_get_member(root, "text_color");
	if (!color_s ||
	    (config_setting_type(color_s) != CONFIG_TYPE_ARRAY) ||
	    (config_settings_length(color_s) != PREFS_COLOR_NUM_ELEMENTS)) {
		PRINT(stderr, "invalid text_color array, using default\n");
		prefs->text_color = DEFAULT_TEXT_COLOR;
	} else {
		prefs->text_color = malloc(PREFS_COLOR_NUM_ELEMENTS * sizeof(int));
		for (int i = 0; i < PREFS_COLOR_NUM_ELEMENTS; i++) {
			prefs->text_color[i] = config_settings_get_int_elem(color_s, i);
		}
	}
	
	config_setting_t bgcolor_s = config_setting_get_member(root, "background_color");
	if (!bgcolor_s ||
	    (config_setting_type(bgcolor_s) != CONFIG_TYPE_ARRAY) ||
	    (config_settings_length(bgcolor_s) != PREFS_COLOR_NUM_ELEMENTS)) {
		PRINT(stderr, "invalid background_color array, using default\n");
		prefs->background_color = DEFAULT_BACKGROUND_COLOR;
	} else {
		prefs->background_color = malloc(PREFS_COLOR_NUM_ELEMENTS * sizeof(int));
		for (int i = 0; i < PREFS_COLOR_NUM_ELEMENTS; i++) {
			prefs->background_color[i] = config_settings_get_int_elem(bgcolor_s, i);
		}
	}
}

void preferences_init(){
  /* initialize the metamode keys */
  setting = config_setting_get_member(root, preference_keys.metamode_keys);
  if(!setting || config_setting_type(setting) != CONFIG_TYPE_GROUP){
  	/* initialize the keystrokes if there are none defined */
  	setting = preferences_init_group(root, preference_keys.metamode_keys);
  	int num_key_defaults = sizeof(preference_defaults.metamode_keys) / sizeof(char*) / 2;
  	int i = 0;
  	for(i = 0; i < num_key_defaults; ++i){
  		preferences_init_string(setting,
  				preference_defaults.metamode_keys[i*2],
  				preference_defaults.metamode_keys[(i*2)+1]);
  	}
  }

  /* initialize the metamode sticky keys */
  setting = config_setting_get_member(root, preference_keys.metamode_sticky_keys);
	if(!setting || config_setting_type(setting) != CONFIG_TYPE_GROUP){
		/* initialize the keystrokes if there are none defined */
		setting = preferences_init_group(root, preference_keys.metamode_sticky_keys);
		int num_key_defaults = sizeof(preference_defaults.metamode_sticky_keys) / sizeof(char*) / 2;
		int i = 0;
		for(i = 0; i < num_key_defaults; ++i){
			preferences_init_string(setting,
				                      preference_defaults.metamode_sticky_keys[i*2],
				                      preference_defaults.metamode_sticky_keys[(i*2)+1]);
		}
	}

  /* initialize the metamode function keys */
  setting = config_setting_get_member(root, preference_keys.metamode_func_keys);
	if(!setting || config_setting_type(setting) != CONFIG_TYPE_GROUP){
		/* initialize the keystrokes if there are none defined */
		setting = preferences_init_group(root, preference_keys.metamode_func_keys);
		int num_key_defaults = sizeof(preference_defaults.metamode_func_keys) / sizeof(char*) / 2;
		int i = 0;
		for(i = 0; i < num_key_defaults; ++i){
			preferences_init_string(setting,
					preference_defaults.metamode_func_keys[i*2],
					preference_defaults.metamode_func_keys[(i*2)+1]);
		}
	}

	/* initialize the sym keys */
	config_setting_t *subsetting;
	setting = config_setting_get_member(root, preference_keys.sym_keys);
	if(!setting || config_setting_type(setting) != CONFIG_TYPE_LIST || !preferences_is_list_of_groups_of_strings(setting)){
		/* initialize if not defined or invalid */
		setting = preferences_init_list_blank(root, preference_keys.sym_keys);
		int i, j, num_entries;
		for(j = 0; j < PREFS_SYMKEYS_DEFAULT_NUM_ROWS; ++j){
			subsetting = config_setting_add(setting, NULL, CONFIG_TYPE_GROUP);
			num_entries = sizeof(preference_defaults.sym_keys[j]) / sizeof(char*) / 2;
			for(i = 0; i < num_entries; ++i){
				if(preference_defaults.sym_keys[j][i*2] != NULL){
					preferences_init_string(subsetting,
							preference_defaults.sym_keys[j][i*2],
							preference_defaults.sym_keys[j][(i*2)+1]);
				}
			}
		}
	}

  /* write the prefs file if there was no local one found or we have updated */
  if(preferences_check_version(root, preference_keys.prefs_version) || !prefs_found){
  	fprintf(stderr, "Writing prefs file\n");
    config_write_file(preferences, preferences_filename);
    /* This is probably a first run */
    first_run();
  }
}

const char* preferences_get_metamode_keystrokes(char keystroke, char* pref_key){
	config_setting_t *keystrokes_group, *root;
	char setting_str[2] = {keystroke, 0};
	const char* setting_value;
	root = config_root_setting(preferences);
	keystrokes_group = config_setting_get_member(root, pref_key);
	if(keystrokes_group){
		if(CONFIG_TRUE == config_setting_lookup_string(keystrokes_group, setting_str, &setting_value)){
			return setting_value;
		}
	}
	// else
	return NULL;
}

const char* preferences_get_metamode_keys(char keystroke){
	return preferences_get_metamode_keystrokes(keystroke, preference_keys.metamode_keys);
}

const char* preferences_get_metamode_sticky_keys(char keystroke){
	return preferences_get_metamode_keystrokes(keystroke, preference_keys.metamode_sticky_keys);
}

const char* preferences_get_metamode_func_keys(char keystroke){
	return preferences_get_metamode_keystrokes(keystroke, preference_keys.metamode_func_keys);
}

int preferences_is_keyhold_exempt(int keystroke){
	config_setting_t *exempt, *root;
	root = config_root_setting(preferences);
	exempt = config_setting_get_member(root, preference_keys.keyhold_actions_exempt);
	if(exempt){
		int num_elements = config_setting_length(exempt);
		int i = 0;
		for(i = 0; i < num_elements; ++i){
			if(keystroke == config_setting_get_int_elem(exempt, i)){
				return 1;
			}
		}
	}
	// else
	return NULL;
}

int preferences_get_sym_num_rows(){
	struct config_setting_t* keys, *root;
	root = config_root_setting(preferences);
	keys = config_setting_get_member(root, preference_keys.sym_keys);
	if(keys){
		return config_setting_length(keys);
	}
	// else
	return 0;
}
/* must free the returned array */
int* preferences_get_sym_num_entries(){
	struct config_setting_t* keys, *root, *sub;
	int* ret;
	int i, num_entries;
	root = config_root_setting(preferences);
	keys = config_setting_get_member(root, preference_keys.sym_keys);
	if(keys){
		num_entries = config_setting_length(keys);
		ret = calloc(num_entries, sizeof(int));
		for(i = 0; i < num_entries; ++i){
			sub = config_setting_get_elem(keys, i);
			ret[i] = config_setting_length(sub);
		}
		return ret;
	}
	// else
	return 0;
}

struct symkey_entry** preferences_get_sym_entries(){
	struct config_setting_t* keys, *root, *sub, *entry;
	struct symkey_entry** ret;
	int i, j, num_entries, num_subentries;
	root = config_root_setting(preferences);
	keys = config_setting_get_member(root, preference_keys.sym_keys);
	if(keys){
		num_entries = config_setting_length(keys);
		ret = (struct symkey_entry**)calloc(num_entries, sizeof(struct symkey_entry*));
		for(i = 0; i < num_entries; ++i){
			sub = config_setting_get_elem(keys, i);
			num_subentries = config_setting_length(sub);
			ret[i] = (struct symkey_entry*)calloc(num_subentries, sizeof(struct symkey_entry));
			for(j = 0; j < num_subentries; ++j){
				entry = config_setting_get_elem(sub, j);
				ret[i][j].name = config_setting_name(entry);
				ret[i][j].c = config_setting_get_string(entry);
			}
		}
		return ret;
	}
	// else
	return 0;
}
