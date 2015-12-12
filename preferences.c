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
static config_t* preferences;

config_t* preferences_get(){
	return preferences;
}

/* unconditionally replace a string preference */
config_setting_t* preferences_replace_string(config_setting_t* root, char* key, char* default_value){
  int type = CONFIG_TYPE_STRING;
  config_setting_remove(root, key);
  config_setting_t* setting = config_setting_add(root, key, type);
  config_setting_set_string(setting, default_value);
  return setting;
}

config_setting_t* preferences_init_string(config_setting_t* root, char* key, char* default_value){
  int type = CONFIG_TYPE_STRING;
  config_setting_t* setting = config_setting_get_member(root, key);
  if(!setting || config_setting_type(setting) != type){
    config_setting_remove(root, key);
    setting = config_setting_add(root, key, type);
    config_setting_set_string(setting, default_value);
  }
  return setting;
}

config_setting_t* preferences_init_int(config_setting_t* root, char* key, int default_value){
  int type = CONFIG_TYPE_INT;
  config_setting_t* setting = config_setting_get_member(root, key);
  if(!setting || config_setting_type(setting) != type){
    config_setting_remove(root, key);
    setting = config_setting_add(root, key, type);
    config_setting_set_int(setting, default_value);
  }
  return setting;
}

config_setting_t* preferences_init_bool(config_setting_t* root, char* key, int default_value){
  int type = CONFIG_TYPE_BOOL;
  config_setting_t* setting = config_setting_get_member(root, key);
  if(!setting || config_setting_type(setting) != type){
    config_setting_remove(root, key);
    setting = config_setting_add(root, key, type);
    config_setting_set_bool(setting, default_value);
  }
  return setting;
}

config_setting_t* preferences_init_group(config_setting_t* root, char* key){
  int type = CONFIG_TYPE_GROUP;
  config_setting_t* setting = config_setting_get_member(root, key);
  if(!setting || config_setting_type(setting) != type){
    config_setting_remove(root, key);
    setting = config_setting_add(root, key, type);
  }
  return setting;
}

char preferences_is_list_of_groups_of_strings(config_setting_t* setting){
  config_setting_t* subsetting;

	if(config_setting_is_list(setting) != CONFIG_TRUE){
		return 0;
	}
  int num_elements = config_setting_length(setting); // num elements in list
  int num_subelements; // num elements in each group in the list
  unsigned int i, j;
  for(i = 0; i < num_elements; ++i){
  	subsetting = config_setting_get_elem(setting, i);
  	if(config_setting_is_group(subsetting) != CONFIG_TRUE){
  		return 0;
  	}
  	// now check for all string values
  	num_subelements = config_setting_length(subsetting);
  	for(j = 0; j < num_subelements; ++j){
  		if(config_setting_type(config_setting_get_elem(subsetting, j)) != CONFIG_TYPE_STRING){
  			return 0;
  		}
  	}
  }
  return 1;
}

config_setting_t* preferences_init_list_blank(config_setting_t* root, char* key){
  int type = CONFIG_TYPE_LIST;
  config_setting_t* setting = config_setting_get_member(root, key);
  if(!setting || config_setting_type(setting) != type || config_setting_length(setting) > 0){
    config_setting_remove(root, key);
    setting = config_setting_add(root, key, type);
  }
  return setting;
}

config_setting_t* preferences_init_array(config_setting_t* root, char* key){
  int type = CONFIG_TYPE_ARRAY;
  config_setting_t* setting = config_setting_get_member(root, key);
  if(!setting || config_setting_type(setting) != type){
    config_setting_remove(root, key);
    setting = config_setting_add(root, key, type);
  }
  return setting;
}

void preferences_pad_array_int(config_setting_t* setting, int size, int padding){
	/* we assume that setting is actually a CONFIG_TYPE_ARRAY */
	int num = config_setting_length(setting);
	for(; num < size; ++num){
		config_setting_set_int_elem(setting, -1, padding);
	}
}

int preferences_check_version(config_setting_t* root, char* key){
  int type = CONFIG_TYPE_INT;
  const char* ms_key;
  config_setting_t* setting = config_setting_get_member(root, key);
  if(!setting /* not found */
  		|| (config_setting_type(setting) != type) /* has been messed with */
  		|| (config_setting_get_int(setting) != PREFS_VERSION)){ /* old */
  	/* we need to update */
  	config_setting_remove(root, key);
		setting = config_setting_add(root, key, type);
		config_setting_set_int(setting, PREFS_VERSION);

		/* from prefs v6 to v7, the default value of hjkl changed */
		setting = config_setting_get_member(root, preference_keys.metamode_sticky_keys);
		ms_key = preferences_get_metamode_sticky_keys('h');
		if(0 == strncmp(ms_key, PREFS_V6_KDEF_H, 3)) { preferences_replace_string(setting, "h", PREFS_V7_KDEF_H);}
		ms_key = preferences_get_metamode_sticky_keys('j');
		if(0 == strncmp(ms_key, PREFS_V6_KDEF_J, 3)) { preferences_replace_string(setting, "j", PREFS_V7_KDEF_J);}
		ms_key = preferences_get_metamode_sticky_keys('k');
		if(0 == strncmp(ms_key, PREFS_V6_KDEF_K, 3)) { preferences_replace_string(setting, "k", PREFS_V7_KDEF_K);}
		ms_key = preferences_get_metamode_sticky_keys('l');
		if(0 == strncmp(ms_key, PREFS_V6_KDEF_L, 3)) { preferences_replace_string(setting, "l", PREFS_V7_KDEF_L);}
		return 1;
  }
  return 0;
}

int preferences_guess_best_font_size(int target_cols){
	/* font widths in pixels for sizes 0-250, indexed by font size */
	int num_sizes = 251;
	int font_widths[251] = {0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6,
												 7, 7, 8, 8, 9, 10, 10, 11, 11, 12,
												 13, 13, 14, 14, 15, 16, 16, 17, 17,
												 18, 19, 19, 20, 20, 21, 22, 22, 23,
												 23, 24, 25, 25, 26, 26, 27, 28, 28,
												 29, 29, 30, 31, 31, 32, 32, 33, 34,
												 34, 35, 35, 36, 37, 37, 38, 38, 39,
												 40, 40, 41, 41, 42, 43, 43, 44, 44,
												 45, 46, 46, 47, 47, 48, 49, 49, 50,
												 50, 51, 52, 52, 53, 53, 54, 55, 55,
												 56, 56, 57, 58, 58, 59, 59, 60, 61,
												 61, 62, 62, 63, 64, 64, 65, 65, 66,
												 67, 67, 68, 68, 69, 70, 70, 71, 71,
												 72, 73, 73, 74, 74, 75, 76, 76, 77,
												 77, 78, 79, 79, 80, 80, 81, 82, 82,
												 83, 83, 84, 85, 85, 86, 86, 87, 88,
												 88, 89, 89, 90, 91, 91, 92, 92, 93,
												 94, 94, 95, 95, 96, 97, 97, 98, 98,
												 99, 100, 100, 101, 101, 102, 103, 103,
												 104, 104, 105, 106, 106, 107, 107, 108,
												 109, 109, 110, 110, 111, 112, 112, 113,
												 113, 114, 115, 115, 116, 116, 117, 118,
												 118, 119, 119, 120, 121, 121, 122, 122,
												 123, 124, 124, 125, 125, 126, 127, 127,
												 128, 128, 129, 130, 130, 131, 131, 132,
												 133, 133, 134, 134, 135, 136, 136, 137,
												 137, 138, 139, 139, 140, 140, 141, 142,
												 142, 143, 143, 144, 145, 145, 146, 146,
												 147, 148, 148, 149, 149};
	int screen_width, screen_height, target_width;
	if((getenv("WIDTH") == NULL) || (getenv("HEIGHT") == NULL)){
		/* no width or height in env, just return the default */
		return preference_defaults.font_size;
	}
	screen_width = atoi(getenv("WIDTH"));
	screen_height = atoi(getenv("HEIGHT"));
	target_width = screen_width < screen_height ? screen_width : screen_height;
	int i = 0;
	int num_px = 0;
	for(i = 0; i < num_sizes; ++i){
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

void preferences_init(){

  preferences = calloc(1, sizeof(config_t));
  char prefs_found = 0;

  config_init(preferences);
  FILE *f;
  f = fopen(preferences_filename, "r");
  if(f){
    fclose(f);
    prefs_found = 1;
    if(config_read_file(preferences, preferences_filename) != CONFIG_TRUE){
       fprintf(stderr, "%s:%d - %s\n", config_error_file(preferences),
                       config_error_line(preferences), config_error_text(preferences));
     }
  } else {
    PRINT(stderr, "Preferences file not found\n");
  }

  config_setting_t *root, *setting;
  root = config_root_setting(preferences);
  int default_font_columns = (atoi(getenv("WIDTH")) <= 720) ? 45 : 60;

  /* check for values from the conf file, and set them if not found or wrong type */
  preferences_init_string(root, preference_keys.font_path, preference_defaults.font_path);
  preferences_init_int(root, preference_keys.font_size, preferences_guess_best_font_size(default_font_columns));
  preferences_init_bool(root, preference_keys.screen_idle_awake, preference_defaults.screen_idle_awake);
  preferences_init_bool(root, preference_keys.auto_show_vkb, preference_defaults.auto_show_vkb);
  preferences_init_bool(root, preference_keys.allow_resize_columns, preference_defaults.allow_resize_columns);
  preferences_init_bool(root, preference_keys.keyhold_actions, preference_defaults.keyhold_actions);
  preferences_init_bool(root, preference_keys.sticky_sym_key, preference_defaults.sticky_sym_key);
  preferences_init_bool(root, preference_keys.sticky_shift_key, preference_defaults.sticky_shift_key);
  //preferences_init_bool(root, preference_keys.sticky_alt_key, preference_defaults.sticky_alt_key);
  preferences_init_int(root, preference_keys.metamode_hold_key, preference_defaults.metamode_hold_key);
  preferences_init_int(root, preference_keys.metamode_doubletap_key, preference_defaults.metamode_doubletap_key);
  preferences_init_int(root, preference_keys.metamode_doubletap_delay, preference_defaults.metamode_doubletap_delay);
  preferences_init_string(root, preference_keys.tty_encoding, preference_defaults.tty_encoding);

  setting = preferences_init_group(root, preference_keys.metamode_hitbox_s);
  preferences_init_int(setting, "x", preference_defaults.hitbox.x);
  preferences_init_int(setting, "y", preference_defaults.hitbox.y);
  preferences_init_int(setting, "w", preference_defaults.hitbox.w);
  preferences_init_int(setting, "h", preference_defaults.hitbox.h);

  /* init the keyhold_exempt keys */
  setting = config_setting_get_member(root, preference_keys.keyhold_actions_exempt);
  if(!setting || config_setting_type(setting) != CONFIG_TYPE_ARRAY){
  	setting = preferences_init_array(root, preference_keys.keyhold_actions_exempt);
  	int num_key_defaults = sizeof(preference_defaults.keyhold_actions_exempt) / sizeof(int);
  	int i = 0;
  	for(i = 0; i < num_key_defaults; ++i){
  		config_setting_set_int_elem(setting, -1, preference_defaults.keyhold_actions_exempt[i]);
  	}
  }

  /* initialize the text and background colours */
  setting = config_setting_get_member(root, preference_keys.text_color);
  if(!setting || config_setting_type(setting) != CONFIG_TYPE_ARRAY){
  	setting = preferences_init_array(root, preference_keys.text_color);
  	int i = 0;
  	for(i = 0; i < PREFS_COLOR_NUM_ELEMENTS; ++i){
  		config_setting_set_int_elem(setting, -1, preference_defaults.text_color[i]);
  	}
  }
  /* ensure array length is long enough */
  preferences_pad_array_int(setting, PREFS_COLOR_NUM_ELEMENTS, 0);

  setting = config_setting_get_member(root, preference_keys.background_color);
  if(!setting || config_setting_type(setting) != CONFIG_TYPE_ARRAY){
  	setting = preferences_init_array(root, preference_keys.background_color);
  	int i = 0;
  	for(i = 0; i < PREFS_COLOR_NUM_ELEMENTS; ++i){
  		config_setting_set_int_elem(setting, -1, preference_defaults.background_color[i]);
  	}
  }
  preferences_pad_array_int(setting, PREFS_COLOR_NUM_ELEMENTS, 0);

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

int preferences_save(config_t* prefs){

  return 0;
}

int preferences_free(config_t* prefs){

  if(prefs != NULL){
    config_destroy(prefs);
    free(prefs);
  }
  return 0;
}

void preferences_uninit(){
	preferences_free(preferences);
}

const char* preferences_get_string(char* pref){
	if(!preferences){
		preferences_init();
	}
	const char* value;
  config_lookup_string(preferences, pref, &value);
  return value;
}

int preferences_get_int(char* pref){
	if(!preferences){
		preferences_init();
	}
	int value;
  config_lookup_int(preferences, pref, &value);
  return value;
}

int preferences_get_bool(char* pref){
	if(!preferences){
		preferences_init();
	}
	int value;
	config_lookup_bool(preferences, pref, &value);
	return value;
}

int preferences_get_int_array(char* pref, int* fillme, int length){
	config_setting_t *root, *setting;
	if(!preferences){
		preferences_init();
	}
	int num_copied = 0;
	root = config_root_setting(preferences);
  setting = config_setting_get_member(root, pref);
  if(setting && config_setting_is_array(setting)){
  	int i = 0;
  	int num_eles = config_setting_length(setting);
  	num_copied = length;
  	if(num_eles < length){
  		num_copied = num_eles;
  	}
  	for(i = 0; i < num_copied; ++i){
  		fillme[i] = config_setting_get_int_elem(setting, i);
  	}
  }
  return num_copied;
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
