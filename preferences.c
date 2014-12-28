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


int preferences_check_version(config_setting_t* root, char* key){
  int type = CONFIG_TYPE_INT;
  config_setting_t* setting = config_setting_get_member(root, key);
  if(!setting /* not found */
  		|| (config_setting_type(setting) != type) /* has been messed with */
  		|| (config_setting_get_int(setting) != PREFS_VERSION)){ /* old */
  	/* we need to update */
  	config_setting_remove(root, key);
		setting = config_setting_add(root, key, type);
		config_setting_set_int(setting, PREFS_VERSION);
		return 1;
  }
  return 0;
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

  /* check for values from the conf file, and set them if not found or wrong type */
  preferences_init_string(root, preference_keys.font_path, preference_defaults.font_path);
  preferences_init_int(root, preference_keys.font_size, preference_defaults.font_size);
  preferences_init_bool(root, preference_keys.screen_idle_awake, preference_defaults.screen_idle_awake);
  preferences_init_bool(root, preference_keys.auto_show_vkb, preference_defaults.auto_show_vkb);
  preferences_init_bool(root, preference_keys.keyhold_actions, preference_defaults.keyhold_actions);
  preferences_init_int(root, preference_keys.metamode_hold_key, preference_defaults.metamode_hold_key);
  preferences_init_int(root, preference_keys.metamode_doubletap_key, preference_defaults.metamode_doubletap_key);
  preferences_init_int(root, preference_keys.metamode_doubletap_delay, preference_defaults.metamode_doubletap_delay);
  preferences_init_string(root, preference_keys.tty_encoding, preference_defaults.tty_encoding);

  setting = preferences_init_group(root, preference_keys.metamode_hitbox_s);
  preferences_init_int(setting, "x", preference_defaults.hitbox.x);
  preferences_init_int(setting, "y", preference_defaults.hitbox.y);
  preferences_init_int(setting, "w", preference_defaults.hitbox.w);
  preferences_init_int(setting, "h", preference_defaults.hitbox.h);

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

  /* initialize the metamode sticky keys */
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

