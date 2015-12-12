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

#include <ioctl.h>
#include <unix.h>
#include <termios.h>
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

static int exit_application = 0;

static char slave_ptyname[L_ctermid];

static int cursor_x = 0;
static int cursor_y = 0;
char draw_cursor = 1;

char flash = 0;

static char symmenu_show = 0;
static char symmenu_lock = 0;
static int symmenu_num_rows = 0;
static int* symmenu_num_entries;
static struct symkey_entry** symmenu_entries;
static int symmenu_height = 0;

static char metamode = 0;
static int metamode_doubletap_key = 0;
static struct timespec metamode_last;
static uint64_t metamode_doubletap;
static int metamode_hitbox[4];
static SDL_Color metamode_cursor_fg = (SDL_Color)BLACK;
static SDL_Color metamode_cursor_bg = (SDL_Color)GREEN;
static SDL_Surface* metamode_cursor;
static int vmodifiers = 0;

static TTF_Font* font;
static int text_width;
static int text_height;
static int text_height_padding;
static int advance;
static int default_text_color_arr[PREFS_COLOR_NUM_ELEMENTS];
static int default_bg_color_arr[PREFS_COLOR_NUM_ELEMENTS];
static SDL_Color default_text_color = (SDL_Color)WHITE;
static SDL_Color default_bg_color = (SDL_Color)BLACK;
struct font_style default_text_style;

struct screenchar blank_sc;
static SDL_Surface* flash_surface;
static SDL_Surface* cursor;
static SDL_Surface* inv_cursor;
static SDL_Surface* screen;
static SDL_Surface* ctrl_key_indicator;
static SDL_Surface* alt_key_indicator;
static SDL_Surface* shift_key_indicator;
SDL_Surface* blank_surface;

static pid_t child_pid = -1;

static char virtualkeyboard_visible = 0;
static char isPassport = 0;
static char key_repeat_done = 0;

static SDL_mutex *input_mutex = NULL;

static int event_pipe[2];

/* from buffer.c */
extern int rows;
extern int cols;
extern buf_t* buf;
extern int MAX_COLS;
extern int MAX_ROWS;
extern int TEXT_BUFFER_SIZE;
extern struct scroll_region sr;


#define PB_D_PIXELS 32
#define README_FILE_PATH "../app/native/README"
#define README45_FILE_PATH "../app/native/README45"
#define SYMMENU_KEY_BORDER 2

int is_terminfo_keystrokes(const char* keystrokes){
  if(keystrokes[0] == 'k'){
    if(0 == strncmp(keystrokes, "kcub1", 5)){ return KEYCODE_LEFT; }
    if(0 == strncmp(keystrokes, "kcud1", 5)){ return KEYCODE_DOWN; }
    if(0 == strncmp(keystrokes, "kcuf1", 5)){ return KEYCODE_RIGHT; }
    if(0 == strncmp(keystrokes, "kcuu1", 5)){ return KEYCODE_UP; }
    if(0 == strncmp(keystrokes, "khome", 5)){ return KEYCODE_HOME; }
    if(0 == strncmp(keystrokes, "kend", 4)){ return KEYCODE_END; }
    if(0 == strncmp(keystrokes, "kf1", 3)){ return KEYCODE_F1; }
    if(0 == strncmp(keystrokes, "kf2", 3)){ return KEYCODE_F2; }
    if(0 == strncmp(keystrokes, "kf3", 3)){ return KEYCODE_F3; }
    if(0 == strncmp(keystrokes, "kf4", 3)){ return KEYCODE_F4; }
    if(0 == strncmp(keystrokes, "kf5", 3)){ return KEYCODE_F5; }
    if(0 == strncmp(keystrokes, "kf6", 3)){ return KEYCODE_F6; }
    if(0 == strncmp(keystrokes, "kf7", 3)){ return KEYCODE_F7; }
    if(0 == strncmp(keystrokes, "kf8", 3)){ return KEYCODE_F8; }
    if(0 == strncmp(keystrokes, "kf9", 3)){ return KEYCODE_F9; }
    if(0 == strncmp(keystrokes, "kf10", 4)){ return KEYCODE_F10; }
    if(0 == strncmp(keystrokes, "kf11", 4)){ return KEYCODE_F11; }
    if(0 == strncmp(keystrokes, "kf12", 4)){ return KEYCODE_F12; }
  }
  return 0;
}

int send_metamode_keystrokes(const char* keystrokes){

  UChar* ukeystrokes;
  size_t ukeystrokes_len;
  size_t keystrokes_len;
  int terminfo_key = 0;
  UChar terminfo_keystrokes[CHARACTER_BUFFER];

  if(keystrokes){
    terminfo_key = is_terminfo_keystrokes(keystrokes);
    /* if the keystrokes for this key match a terminfo pattern,
     * send the appropriate sequence instead of the literal string */
    if(terminfo_key){
      ukeystrokes_len = ecma48_parse_control_codes(terminfo_key, 0, terminfo_keystrokes);
      /* and write out to the tty whatever the keys were */
      io_write_master(terminfo_keystrokes, ukeystrokes_len);
      return 1;
    }
    // else
    keystrokes_len = strlen(keystrokes);
    /* libconfig will return ascii strings, but we can put utf8 in there too */
    ukeystrokes = (UChar*)calloc(keystrokes_len, sizeof(UChar));
    ukeystrokes_len = io_read_utf8_string(keystrokes, keystrokes_len, ukeystrokes);
    /* and write out to the tty whatever the keys were */
    io_write_master(ukeystrokes, ukeystrokes_len);
    free(ukeystrokes);
    return 1;
  }
  /* no keystrokes saved for this key */
  return 0;
}

int get_virtualkeyboard_height(){
  int rc, vkb_h;
  rc = virtualkeyboard_get_height(&vkb_h);
  if(rc != BPS_SUCCESS){
    fprintf(stderr, "Could not get virtual keyboard height\n");
    vkb_h = 0; // assume zero?
  }
  return vkb_h;
}

void init_virtualkeyboard(){
  /* and show the keyboard if the user wants it */
  if(preferences_get_bool(preference_keys.auto_show_vkb) || isPassport){
    virtualkeyboard_show();
  }
}

void check_device(){
  deviceinfo_details_t *di_t;
  int rc;
  char * model;
  rc = deviceinfo_get_details(&di_t);
  if(rc != BPS_SUCCESS){
    fprintf(stderr, "Could not get device info");
    return;
  }
  if(strncmp("Passport", deviceinfo_details_get_model_name(di_t), 8) == 0){
    isPassport = 1;
  }
  deviceinfo_free_details(&di_t);
}

void first_run(){
  struct stat statbuf;
  char* readme_path = (atoi(getenv("WIDTH")) <= 720) ? README45_FILE_PATH : README_FILE_PATH;
  fprintf(stderr, "Updating README\n");
  int rc = stat(readme_path, &statbuf);
  if(rc == 0){
    // stat success!
    if(symlink(readme_path, "./README") == -1){
      if(errno != EEXIST){
        fprintf(stderr, "Error linking README from app to PWD\n");
      }
    }
  }
}

int get_wm_info(SDL_SysWMinfo* info){
  SDL_version version;
  SDL_VERSION(&version);
  info->version = version;
  return SDL_GetWMInfo(info);
}

void metamode_toggle(){
  metamode = metamode ? 0 : 1;
}

void symmenu_stick(){
    PRINT(stderr, "Sticking Sym key\n");
    symmenu_lock = 1;
}

void symmenu_toggle(){
    symmenu_show = symmenu_show ? 0 : 1;
    if(symmenu_show){
        // resize to show menu
        setup_screen_size(screen->w, screen->h - symmenu_height);
        if(preferences_get_bool(preference_keys.sticky_sym_key)){
            symmenu_stick();
        }
    } else {
        // resize to take full screen
        setup_screen_size(screen->w, screen->h);
        symmenu_lock = 0;
    }
}

const char* symkey_for_mousedown(Uint16 x, Uint16 y){
	int i, j;
	if(symmenu_num_rows > 0){
		for(i = 0; i < symmenu_num_rows; ++i){
			for(j = 0; j < symmenu_num_entries[i]; ++j){
				if(    (x > symmenu_entries[i][j].x)
						&& (y > symmenu_entries[i][j].y)
						&& (x < (symmenu_entries[i][j].x + symmenu_entries[i][j].background->w))
						&& (y < (symmenu_entries[i][j].y + symmenu_entries[i][j].symbol->h))){
				  if(!symmenu_lock){
				    symmenu_toggle();
				  } else {
				    symmenu_entries[i][j].flash = 1;
				  }
					return symmenu_entries[i][j].c;
				}
			}
		}
	}
	return NULL;
}

const char* get_symmenu_keys(char key){
	int i, j;
	if(symmenu_num_rows > 0){
		for(i = 0; i < symmenu_num_rows; ++i){
			for(j = 0; j < symmenu_num_entries[i]; ++j){
				if(symmenu_entries[i][j].name[0] == key){
				  if(!symmenu_lock){
				    symmenu_toggle();
				  } else {
				    symmenu_entries[i][j].flash = 1;
				  }
				    return symmenu_entries[i][j].c;
				}
			}
		}
	}
	return NULL;
}

void symmenu_uninit(){
  int i, j;
  if(symmenu_num_rows > 0){
    for(i = 0; i < symmenu_num_rows; ++i){
      for(j = 0; j < symmenu_num_entries[i]; ++j){
        SDL_FreeSurface(symmenu_entries[i][j].background);
        SDL_FreeSurface(symmenu_entries[i][j].flash_symbol);
        SDL_FreeSurface(symmenu_entries[i][j].key);
        SDL_FreeSurface(symmenu_entries[i][j].symbol);
        free(symmenu_entries[j][i].uc);
      }
      free(symmenu_entries[i]);
    }
    free(symmenu_entries);
  }
}

void symmenu_init(){

	symmenu_num_rows = preferences_get_sym_num_rows();
	symmenu_num_entries = preferences_get_sym_num_entries();
	symmenu_entries = preferences_get_sym_entries();

	PRINT(stderr, "num rows: %d\n", symmenu_num_rows);

	UChar* ukeystrokes;
	size_t ukeystrokes_len;
	size_t keystrokes_len;
	struct symkey_entry entry;
	UChar blank_background[2] = {' ', NULL};
	UChar cornerchar[2]; cornerchar[1] = NULL;

	if(symmenu_num_rows > 0){
		int i, j;

		int longest = symmenu_num_entries[0];
		for(i = 0; i < symmenu_num_rows; ++i){
			longest = symmenu_num_entries[i] > longest ? symmenu_num_entries[i] : longest;
		}

		const char* symmenu_font_path = preference_defaults.font_path;
		int symmenu_background_size = preferences_guess_best_font_size(longest);
		int symmenu_corner_size = symmenu_background_size / 5;
		int symmenu_font_size = (6 * symmenu_background_size) / 10;

		/* Load the font - if this was going to fail, it would have failed earlier in init() */
		TTF_Font* symmenu_font = TTF_OpenFont(symmenu_font_path, symmenu_font_size);
		TTF_SetFontStyle(symmenu_font, TTF_STYLE_NORMAL);
		TTF_SetFontOutline(symmenu_font, 0);
		TTF_SetFontKerning(symmenu_font, 0);
		TTF_SetFontHinting(symmenu_font, TTF_HINTING_NORMAL);

		TTF_Font* symmenu_background_font = TTF_OpenFont(symmenu_font_path, symmenu_background_size);
		TTF_SetFontStyle(symmenu_background_font, TTF_STYLE_NORMAL);
		TTF_SetFontOutline(symmenu_background_font, 0);
		TTF_SetFontKerning(symmenu_background_font, 0);
		TTF_SetFontHinting(symmenu_background_font, TTF_HINTING_NORMAL);

		TTF_Font* symmenu_corner_font = TTF_OpenFont(symmenu_font_path, symmenu_corner_size);
		TTF_SetFontStyle(symmenu_corner_font, TTF_STYLE_NORMAL);
		TTF_SetFontOutline(symmenu_corner_font, 0);
		TTF_SetFontKerning(symmenu_corner_font, 0);
		TTF_SetFontHinting(symmenu_corner_font, TTF_HINTING_NORMAL);

		for(j = 0; j < symmenu_num_rows; ++j){
			for(i = 0; i < symmenu_num_entries[j]; ++i){
				keystrokes_len = strlen(symmenu_entries[j][i].c);
				symmenu_entries[j][i].uc = (UChar*)calloc(keystrokes_len + 1, sizeof(UChar));
				symmenu_entries[j][i].flash = 0;
				ukeystrokes_len = io_read_utf8_string(symmenu_entries[j][i].c, keystrokes_len, symmenu_entries[j][i].uc);
				symmenu_entries[j][i].background = TTF_RenderUNICODE_Shaded(symmenu_background_font, blank_background, (SDL_Color)SYMMENU_BORDER, (SDL_Color)SYMMENU_BORDER);
        symmenu_entries[j][i].symbol = TTF_RenderUNICODE_Shaded(symmenu_font, symmenu_entries[j][i].uc, (SDL_Color)SYMMENU_FONT, (SDL_Color)SYMMENU_BACKGROUND);
        symmenu_entries[j][i].flash_symbol = TTF_RenderUNICODE_Shaded(symmenu_font, symmenu_entries[j][i].uc, (SDL_Color)SYMMENU_BACKGROUND, (SDL_Color)SYMMENU_FONT);
				symmenu_entries[j][i].off_x = (symmenu_entries[j][i].background->w - symmenu_entries[j][i].symbol->w) / 2;
				symmenu_entries[j][i].off_y = (symmenu_entries[j][i].background->h - symmenu_entries[j][i].symbol->h) / 2;
				cornerchar[0] = symmenu_entries[j][i].name[0];
				symmenu_entries[j][i].key = TTF_RenderUNICODE_Shaded(symmenu_corner_font, cornerchar, (SDL_Color)SYMMENU_CORNER, (SDL_Color)SYMMENU_BACKGROUND);
				symmenu_height = symmenu_num_rows * ((2*SYMMENU_KEY_BORDER) + symmenu_entries[j][i].symbol->h);
				PRINT(stderr, "Got symkey: name: %s, value: %s\n", symmenu_entries[j][i].name, symmenu_entries[j][i].c);
			}
		}
		TTF_CloseFont(symmenu_font);
		TTF_CloseFont(symmenu_corner_font);
		TTF_CloseFont(symmenu_background_font);
	}
}

int font_init(int font_size){

  fprintf(stderr, "Setting font size %d\n", font_size);
  const char* font_path = preferences_get_string(preference_keys.font_path);
  if(font_size < MIN_FONT_SIZE){
    fprintf(stderr, "Refusing to set font size to %d - too small\n",font_size);
    int default_font_columns = (atoi(getenv("WIDTH")) <= 720) ? 45 : 60;
    font_size = preferences_guess_best_font_size(default_font_columns);
  }

  /* Load the font */
  font = TTF_OpenFont(font_path, font_size);
  if ( font == NULL ) {
    /* try opening the default stuff */
    fprintf(stderr, "Couldn't load %d pt font from %s: %s\n", font_size, font_path, SDL_GetError());
    font = TTF_OpenFont(preference_defaults.font_path, preference_defaults.font_size);
    if(font == NULL){
      fprintf(stderr, "Could not open default font %s: %s\n", preference_defaults.font_path, SDL_GetError());
      return TERM_FAILURE;
    }
  }
  PRINT(stderr, "Font is Fixed Width: %d\n", TTF_FontFaceIsFixedWidth(font));

  /* Set default options */
  TTF_SetFontStyle(font, TTF_STYLE_NORMAL);
  TTF_SetFontOutline(font, 0);
  TTF_SetFontKerning(font, 0);
  TTF_SetFontHinting(font, TTF_HINTING_NORMAL);

  /* get default colour settings */
  int pref_color_len = preferences_get_int_array(preference_keys.text_color, default_text_color_arr, PREFS_COLOR_NUM_ELEMENTS);
  if(pref_color_len == PREFS_COLOR_NUM_ELEMENTS){
    default_text_color.r = (Uint8)default_text_color_arr[0];
    default_text_color.g = (Uint8)default_text_color_arr[1];
    default_text_color.b = (Uint8)default_text_color_arr[2];
    default_text_color.unused = 0;
  }
  pref_color_len = preferences_get_int_array(preference_keys.background_color, default_bg_color_arr, PREFS_COLOR_NUM_ELEMENTS);
  if(pref_color_len == PREFS_COLOR_NUM_ELEMENTS){
    default_bg_color.r = (Uint8)default_bg_color_arr[0];
    default_bg_color.g = (Uint8)default_bg_color_arr[1];
    default_bg_color.b = (Uint8)default_bg_color_arr[2];
    default_bg_color.unused = 0;
  }

  default_text_style.fg_color = default_text_color;
  default_text_style.bg_color = default_bg_color;
  default_text_style.style = TTF_STYLE_NORMAL;

  /* initialize special characters */
  UChar str[2] = {' ', NULL};
  blank_surface = TTF_RenderUNICODE_Shaded(font, str, default_text_color, default_bg_color);
  if (blank_surface == NULL){
    PRINT(stderr, "Couldn't render blank surface: %s\n", TTF_GetError());
    return TERM_FAILURE;
  }

  blank_sc.c = ' ';
  blank_sc.style = default_text_style;
  blank_sc.surface = blank_surface;
  flash_surface = TTF_RenderUNICODE_Shaded(font, str, default_bg_color, default_text_color);
  if (flash_surface == NULL){
    PRINT(stderr, "Couldn't render flash surface: %s\n", TTF_GetError());
    return TERM_FAILURE;
  }

  str[0] = 'A';
  alt_key_indicator = TTF_RenderUNICODE_Shaded(font, str, metamode_cursor_fg, metamode_cursor_bg);
  if (alt_key_indicator == NULL){
    PRINT(stderr, "Couldn't render alt_key_indicator surface: %s\n", TTF_GetError());
    return TERM_FAILURE;
  }

  str[0] = 'C';
  ctrl_key_indicator = TTF_RenderUNICODE_Shaded(font, str, metamode_cursor_fg, metamode_cursor_bg);
  if (ctrl_key_indicator == NULL){
    PRINT(stderr, "Couldn't render ctrl_key_indicator surface: %s\n", TTF_GetError());
    return TERM_FAILURE;
  }

  str[0] = 0x2191;
  shift_key_indicator = TTF_RenderUNICODE_Shaded(font, str, metamode_cursor_fg, metamode_cursor_bg);
  if (shift_key_indicator == NULL){
    PRINT(stderr, "Couldn't render shift_key_indicator surface: %s\n", TTF_GetError());
    return TERM_FAILURE;
  }

  str[0] = 'M';
  metamode_cursor = TTF_RenderUNICODE_Shaded(font, str, metamode_cursor_fg, metamode_cursor_bg);
  if (metamode_cursor == NULL){
    PRINT(stderr, "Couldn't render metamode_cursor surface: %s\n", TTF_GetError());
    return TERM_FAILURE;
  }

  /* Initialize the cursor */
  UChar cursorstr[2] = {' ', NULL};
  cursor = TTF_RenderUNICODE_Shaded(font, cursorstr, default_bg_color, default_text_color);
  if (cursor == NULL){
    PRINT(stderr, "Couldn't render cursor char: %s\n", TTF_GetError());
    return TERM_FAILURE;
  }

  /* Get the size of the font */
  int minx, maxx, miny, maxy;
  if(TTF_GlyphMetrics(font, (Uint16)'X', &minx, &maxx, &miny, &maxy, &advance) != 0){
    PRINT(stderr, "Could not get Glyph Metrics: %s\n", TTF_GetError());
    return TERM_FAILURE;
  }

  text_width = advance;
  text_height = maxy - miny;
  text_height_padding = TTF_FontLineSkip(font) - text_height;
  text_height += text_height_padding;
  PRINT(stderr, "Character h: %d w:%d (h padding: %d) advance: %d\n", text_height, text_width, text_height_padding, advance);

  return TERM_SUCCESS;
}

void font_uninit(){

  SDL_FreeSurface(blank_surface);
  SDL_FreeSurface(flash_surface);
  SDL_FreeSurface(metamode_cursor);
  SDL_FreeSurface(ctrl_key_indicator);
  SDL_FreeSurface(alt_key_indicator);
  SDL_FreeSurface(shift_key_indicator);
  SDL_FreeSurface(cursor);
  if(font != NULL){
    TTF_CloseFont(font);
  }
}

void handle_activeevent(gain, state){
  if(gain){
    PRINT(stderr, "Got ActiveEvent - initializing keyboard\n");
    init_virtualkeyboard();
  }
}

void handle_mousedown(Uint16 x, Uint16 y){
  /* check for hits in the metamode_hitbox */
  if(x >= metamode_hitbox[0]
     && x <= (metamode_hitbox[0] + metamode_hitbox[2])
     && y >= metamode_hitbox[1]
     && y <= metamode_hitbox[1] + metamode_hitbox[3]){
    /* hit in the box */
    metamode_toggle();
  }
  /* touching the screen will reveal the keyboard on a Passport,
   * since the system wide gesture doesn't work to reveal. */
  if(isPassport){
    virtualkeyboard_show();
  }

  /* check for symmenu touches */
  if(symmenu_show){
      send_metamode_keystrokes(symkey_for_mousedown(x, y));
  }
}

void handle_virtualkeyboard_event(bps_event_t *event){
  PRINT(stderr, "Virtual Keyboard event\n");
  int event_code = bps_event_get_code(event);
  int vkb_h;
  int resolution[2] = {screen->w, screen->h};

  vkb_h = get_virtualkeyboard_height();

  switch (event_code){
    case VIRTUALKEYBOARD_EVENT_VISIBLE:
      setup_screen_size(resolution[0], resolution[1] - vkb_h);
      virtualkeyboard_visible = 1;
      break;
    case VIRTUALKEYBOARD_EVENT_HIDDEN:
      setup_screen_size(resolution[0], resolution[1]);
      virtualkeyboard_visible = 0;
      break;
    case VIRTUALKEYBOARD_EVENT_INFO:
      vkb_h = virtualkeyboard_visible ? virtualkeyboard_event_get_height(event) : 0;
      setup_screen_size(resolution[0], resolution[1] - vkb_h);
      break;
    default:
      fprintf(stderr, "Unknown keyboard event code %d\n", event_code);
      break;
  }
}

void rescreen(int w, int h){

  int width  = w == -1 ? screen->w : w;
  int height = h == -1 ? screen->h : h;
  int vkb_h = 0;
  screen = SDL_SetVideoMode(width, height, PB_D_PIXELS, SDL_HWSURFACE | SDL_DOUBLEBUF);
  /* reset the font size as well */
  font_uninit();
  buf_clear_all_renders();
  if(font_init(preferences_get_int(preference_keys.font_size)) == TERM_FAILURE){
    fprintf(stderr, "Couldn't initialize font\n");
    exit_application = 1;
  }

  setup_screen_size(width, height);
  if(virtualkeyboard_visible){
    vkb_h = get_virtualkeyboard_height();
    setup_screen_size(width, height - vkb_h);
  }
}

void toggle_vkeymod(int mod){
  PRINT(stderr, "Toggle modifier %d\n", mod);
  if(vmodifiers & mod){
    vmodifiers &= ~mod;
  }
  else {
    vmodifiers |= mod;
  }
}

char no_uppercase_representation(int keysym){
	switch(keysym){
		case 0x00:
		case 0x01:
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
		case 0x08:
		case 0x09:
		case 0x0a:
		case 0x0b:
		case 0x0c:
		case 0x0d:
		case 0x0e:
		case 0x0f:
		case 0x10:
		case 0x11:
		case 0x12:
		case 0x13:
		case 0x14:
		case 0x15:
		case 0x16:
		case 0x17:
		case 0x18:
		case 0x19:
		case 0x1a:
		case 0x1b:
		case 0x1c:
		case 0x1d:
		case 0x1e:
		case 0x1f:
    case KEYCODE_PAUSE      :
    case KEYCODE_SCROLL_LOCK:
    case KEYCODE_PRINT      :
    case KEYCODE_SYSREQ     :
    case KEYCODE_BREAK      :
    case KEYCODE_ESCAPE     :
    case KEYCODE_BACKSPACE  :
    case KEYCODE_TAB        :
    case KEYCODE_BACK_TAB   :
    case KEYCODE_CAPS_LOCK  :
    case KEYCODE_LEFT_SHIFT :
    case KEYCODE_RIGHT_SHIFT:
    case KEYCODE_LEFT_CTRL  :
    case KEYCODE_RIGHT_CTRL :
    case KEYCODE_LEFT_ALT   :
    case KEYCODE_RIGHT_ALT  :
    case KEYCODE_MENU       :
    case KEYCODE_LEFT_HYPER :
    case KEYCODE_RIGHT_HYPER:
    case KEYCODE_INSERT     :
    case KEYCODE_HOME       :
    case KEYCODE_PG_UP      :
    case KEYCODE_DELETE     :
    case KEYCODE_END        :
    case KEYCODE_PG_DOWN    :
    case KEYCODE_NUM_LOCK   :
    case KEYCODE_F1         :
    case KEYCODE_F2         :
    case KEYCODE_F3         :
    case KEYCODE_F4         :
    case KEYCODE_F5         :
    case KEYCODE_F6         :
    case KEYCODE_F7         :
    case KEYCODE_F8         :
    case KEYCODE_F9         :
    case KEYCODE_F10        :
    case KEYCODE_F11        :
    case KEYCODE_F12        :
    return 1;
    break;
	}
	return 0;
}

void handleKeyboardEvent(screen_event_t screen_event)
{
  int screen_val, screen_flags, screen_alt_val;
  int modifiers;
  int num_chars;
  int vkbd_h;
  int metamode_just_set = 0;
  UChar c[CHARACTER_BUFFER];
  UChar *target = c;
  struct timespec now;
  uint64_t now_t, diff_t, metamode_last_t;
  const char* keys = NULL;
  int32_t last_len = 0;
  int32_t bs_i = 0;
  size_t upcase_len = 0;
  UChar backspace = 0x8;

  screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_KEY_FLAGS, &screen_flags);
  screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_KEY_SYM, &screen_val);
  screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_KEY_ALTERNATE_SYM, &screen_alt_val);
  screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_KEY_MODIFIERS, &modifiers);
  //screen_get_event_property_iv(screen_event, SCREEN_PROPERTY_KEY_CAP, &cap);

  if (screen_flags & KEY_DOWN) {
    PRINT(stderr, "The '%d' key was pressed (modifiers: %d) (char %c) (alt %d)\n", (int)screen_val, modifiers, (char)screen_val, (int)screen_alt_val);
    fflush(stdout);

    /* if we're toggling metamode on or off with doubletap */
    if((screen_val == metamode_doubletap_key) && !(screen_flags & KEY_REPEAT)){
      clock_gettime(CLOCK_MONOTONIC, &now);
      now_t = timespec2nsec(&now);
      metamode_last_t = timespec2nsec(&metamode_last);
      diff_t = now_t > metamode_last_t ? now_t - metamode_last_t : now_t;
      if(diff_t <= metamode_doubletap){
        metamode_toggle();
        metamode_just_set = 1;
      }
      metamode_last = now;
    }

    /* handle sticky keys */
    if(screen_val == KEYCODE_BB_SYM_KEY){
    	if(!(screen_flags & KEY_REPEAT)){
    		symmenu_toggle();
    	} else{
    		/* they are holding it down */
    		symmenu_stick();
    	}
    	return;
    }
    if(!virtualkeyboard_visible
        && ((screen_val == KEYCODE_LEFT_SHIFT) || (screen_val == KEYCODE_RIGHT_SHIFT))){
      if(preferences_get_bool(preference_keys.sticky_shift_key)){
        if(screen_flags & KEY_REPEAT){
          return;
        } else {
          toggle_vkeymod(KEYMOD_SHIFT);
          return;
        }
      }
    }

    /* metamode sticky keys don't trigger repreat */
    if(metamode && !metamode_just_set){
      keys = preferences_get_metamode_sticky_keys((char)screen_val);
      if(keys != NULL){
        send_metamode_keystrokes(keys);
        return;
      }
    }

    /* handle key repeat to upcase / metamode */
    if ((screen_flags & KEY_REPEAT)
        && preferences_get_bool(preference_keys.keyhold_actions)
        && !preferences_is_keyhold_exempt(screen_val)){
      if(!key_repeat_done){
        /* Check for a metamode toggle key first */
        if(screen_val == preferences_get_int(preference_keys.metamode_hold_key)){
          io_write_master(&backspace, 1);
          metamode_toggle();
          key_repeat_done = 1;
          return;
        } else if (!no_uppercase_representation(screen_val)){
					/* Now try to upcase */
					last_len = io_upcase_last_write(&target, CHARACTER_BUFFER);
					if(last_len > 0){
						/* We can upcase, send last_len backspaces and then the upcase char.
						 * Note that this really only works if the program on the other
						 * end of the line understands unicode, and can marry up backspaces
						 * with codepoints, instead of just blindly deleting one byte at a time. */
						upcase_len = (size_t)(target - c);
						PRINT(stderr, "Writing %d backspace and %d upcase chars\n", last_len, upcase_len);
						for(bs_i = 1; bs_i <= last_len; ++bs_i){
							io_write_master(&backspace, 1);
						}
						io_write_master(c, upcase_len);
						key_repeat_done = 1;
						return;
					}
        } // fall through to usual behaviour if we can't upcase the last write.
      } else {
        // We have already handled this key repeat
        return;
      }
    } else {
      key_repeat_done = 0;
    }

    if(metamode && !metamode_just_set){
      keys = preferences_get_metamode_keys((char)screen_val);
      if(keys != NULL){
        send_metamode_keystrokes(keys);
        metamode_toggle();
        return;
      }
      // else
      keys = preferences_get_metamode_func_keys((char)screen_val);
      if(keys != NULL){
        int f = 0;
        if(!f && (0 == strncmp(keys, "alt_down", 8)))          { toggle_vkeymod(KEYMOD_ALT);f=1;}
        if(!f && (0 == strncmp(keys, "ctrl_down", 9)))         { toggle_vkeymod(KEYMOD_CTRL);f=1;}
        if(!f && (0 == strncmp(keys, "rescreen", 8)))          { rescreen(-1, -1);f=1;}
        if(!f && (0 == strncmp(keys, "paste_clipboard", 15)))  { io_paste_from_clipboard();f=1;}
      }
      metamode_toggle();
      return;
    }

    /* handle sym keys */
    if(symmenu_show){
      keys = get_symmenu_keys((char)screen_val);
      if(keys != NULL){
        send_metamode_keystrokes(keys);
        return;
      }
    }

    /* if we have virtual keymods, then put them in, then turn them off */
    modifiers |= vmodifiers;
    vmodifiers = 0;

    /* now process the keypress */
    switch (screen_val) {
      case KEYCODE_PAUSE      :
      case KEYCODE_SCROLL_LOCK:
      case KEYCODE_PRINT      :
      case KEYCODE_SYSREQ     :
      case KEYCODE_BREAK      :
        //case KEYCODE_ESCAPE     :
        //case KEYCODE_BACKSPACE  :
        //case KEYCODE_TAB        :
        //case KEYCODE_BACK_TAB   :
      case KEYCODE_CAPS_LOCK  :
      case KEYCODE_LEFT_SHIFT :
      case KEYCODE_RIGHT_SHIFT:
      case KEYCODE_LEFT_CTRL  :
      case KEYCODE_RIGHT_CTRL :
      case KEYCODE_LEFT_ALT   :
      case KEYCODE_RIGHT_ALT  :
      case KEYCODE_MENU       :
      case KEYCODE_LEFT_HYPER :
      case KEYCODE_RIGHT_HYPER:
        //case KEYCODE_INSERT     :
        //case KEYCODE_HOME       :
        //case KEYCODE_PG_UP      :
        //case KEYCODE_DELETE     :
        //case KEYCODE_END        :
        //case KEYCODE_PG_DOWN    :
      case KEYCODE_NUM_LOCK   :
        //case KEYCODE_F1         :
        //case KEYCODE_F2         :
        //case KEYCODE_F3         :
        //case KEYCODE_F4         :
        //case KEYCODE_F5         :
        //case KEYCODE_F6         :
        //case KEYCODE_F7         :
        //case KEYCODE_F8         :
        //case KEYCODE_F9         :
        //case KEYCODE_F10        :
        //case KEYCODE_F11        :
        //case KEYCODE_F12        :
        PRINT(stderr, "Modifier %d\n", screen_val);
        break;
      default:
        num_chars = ecma48_parse_control_codes(screen_val, modifiers, c);
        int nc;
        for(nc = 0; nc < num_chars; ++nc){
          PRINT(stderr, "Writing 0x%x\n", (int)c[nc]);
        }
        io_write_master((const UChar*)&c, num_chars);
        break;
    }
  }
}

void set_tty_window_size(){
  if(tcsetsize(io_get_master(), rows, cols) < 0){
    PRINT(stderr, "ERROR: tcsetsize() returned <0 (%s). Did not set child pty window size. \n", strerror(errno));
  }
  /* and send SIGWINCH */
  int pgrp;
  if(ioctl(io_get_master(), TIOCGPGRP, &pgrp) != -1){
    killpg(pgrp, SIGWINCH);
  } else {
    PRINT(stderr, "Could not get pgrp of tty: %s\n", strerror(errno));
    /* and assume the pgrp is our own, because we're not allowed to set pgrp.. */
    killpg(getpid(), SIGWINCH);
  }
}

/* Call _after_ we have calculated the text size */
void setup_screen_size(int s_w, int s_h){

  if(s_w <= 1 || s_h <= 1){
    /* refusing to do that */
    return;
  }

  int old_rows = rows;
  int old_bottom_line = buf_bottom_line();
  rows = s_h / text_height;
  cols = s_w / text_width;
  int diff_rows = rows - old_rows;
  PRINT(stderr, "Rows: %d Cols: %d\n", rows, cols);

  /* calculate where we should start drawing */
  if(buf->line && old_rows > rows){ // new size smaller
    buf->top_line = buf->line - buf->top_line + 1 > rows ? buf->line - rows + 1 : buf->top_line;
    // clear covered up lines that are below the cursor
    int toclear = old_bottom_line - buf_bottom_line();
    PRINT(stderr, "new rows: %d, new bottom: %d, old rows: %d, old_bottom: %d, clearing %d lines (SIZE: %d)\n",
        rows, buf_bottom_line(), old_rows, old_bottom_line, toclear, TEXT_BUFFER_SIZE);
    buf_erase_lines(buf_bottom_line() + 1, toclear);
  } else if (buf->line && old_rows < rows){ // new size bigger
    //buf->top_line = buf->line - rows + 1 < 0 ? 0 : buf->line - rows + 1;
    buf->top_line = buf->top_line - diff_rows < 0 ? 0 : buf->top_line - diff_rows;
    // clear newly revealed lines of artifacts
    int toclear = buf_bottom_line() < TEXT_BUFFER_SIZE ?
        buf_bottom_line() - old_bottom_line :
        TEXT_BUFFER_SIZE - 1 - old_bottom_line;
    PRINT(stderr, "new rows: %d, new bottom: %d, old rows: %d, old_bottom: %d, clearing %d lines\n",
        rows, buf_bottom_line(), old_rows, old_bottom_line, toclear);
    buf_erase_lines(old_bottom_line + 1, toclear);
  }

  /* and reset the scroll region */
  sr.top = 1;
  sr.bottom = rows;

  // set the tty size
  set_tty_window_size();
}

void lock_input(){
  if(SDL_LockMutex(input_mutex) == -1){
    fprintf(stderr, "Couldn't lock input mutex - exiting\n");
    exit_application = 1;
  }
}
void unlock_input(){
  if(SDL_UnlockMutex(input_mutex) == -1){
    fprintf(stderr, "Couldn't unlock input mutex - exiting\n");
    exit_application = 1;
  }
}

void indicate_event_input(){
  char *indicate_buf = "w";
  /* indicate that the render thread should run. Note that
   * we are logging errors here, but aren't doing anything with them. */
  if(write(event_pipe[1], (void*)indicate_buf, 1) < 0){
    fprintf(stderr, "Error writing to event pipe: %d\n", errno);
  }
}


/* This function is intended for resizing the number of
 * colums after app init */
void set_screen_cols(int ncols){
  /* the user wants this number of columns */
  if(preferences_get_bool(preference_keys.allow_resize_columns)){
    int new_fontsize = preferences_guess_best_font_size(ncols);
    font_uninit();
    buf_clear_all_renders();
    if(font_init(new_fontsize) == TERM_FAILURE){
      fprintf(stderr, "Error setting new font size\n");
      exit_application = 1;
    } else {
      setup_screen_size(screen->w, screen->h);
      /* and force the number of columns */
      cols = ncols;
      set_tty_window_size();
    }
  }
}

int init() {

  int i;

  /* init the input mutex */
  input_mutex = SDL_CreateMutex();
  /* init the event input pipe */
  if(pipe(event_pipe) == -1){
    fprintf(stderr, "Couldn't create event pipe\n");
    return TERM_FAILURE;
  }

  /* Initialize SDL */
  if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
    PRINT(stderr, "Couldn't initialize SDL: %s\n",SDL_GetError());
    return TERM_FAILURE;
  }
  PRINT(stderr, "Post SDL_Init()\n");
  check_device();

  SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
  // We get keyboard events from the SysWMEvents
  SDL_EventState(SDL_KEYDOWN, SDL_IGNORE);
  SDL_EventState(SDL_KEYUP, SDL_IGNORE);

  screen_window_t window;
  screen_context_t context;

  SDL_SysWMinfo info;
  if(get_wm_info(&info) != 1){
    fprintf(stderr, "Couldn't get WM Info: %s\n",SDL_GetError());
    return TERM_FAILURE;
  }

  /* grab the orientation and resolution so we can start up that way */
  window = info.mainWindow;
  context = info.context;
  int wm_size[2] = {0,0};

  if (screen_get_window_property_iv(window, SCREEN_PROPERTY_SIZE, wm_size)) {
    fprintf(stderr, "Cannot get resolution: %s", strerror(errno));
    return TERM_FAILURE;
  }
  PRINT(stderr, "wm size returned: w:%d, h:%d\n", wm_size[0], wm_size[1]);

  /* Initialize the TTF library */
  if ( TTF_Init() < 0 ) {
    PRINT(stderr, "Couldn't initialize TTF: %s\n",SDL_GetError());
    SDL_Quit();
    return TERM_FAILURE;
  }

  /* set screen idle mode */
  if(!preferences_get_bool(preference_keys.screen_idle_awake)){
    setenv("SCREEN_IDLE_NORMAL", "1", 0);
  }

  /* check to verify if the wm returned the native resolution */
  if (getenv("WIDTH") != NULL && getenv("HEIGHT") != NULL) {
    if(wm_size[0] != atoi(getenv("WIDTH")) || wm_size[1] != atoi(getenv("HEIGHT"))){
      fprintf(stderr, "SDL_WMInfo returned non-native screen resolution - forcing\n");
      wm_size[0] = atoi(getenv("WIDTH"));
      wm_size[1] = atoi(getenv("HEIGHT"));
    }
  }

  screen = SDL_SetVideoMode(wm_size[0], wm_size[1], PB_D_PIXELS, SDL_HWSURFACE | SDL_DOUBLEBUF);
  if ( screen == NULL ) {
    PRINT(stderr, "Couldn't set %d x %d x %d video mode: %s\n", wm_size[0], wm_size[1], PB_D_PIXELS, SDL_GetError());
    TTF_Quit();
    SDL_Quit();
    return TERM_FAILURE;
  }

  if(font_init(preferences_get_int(preference_keys.font_size)) == TERM_FAILURE){
    PRINT(stderr, "Couldn't initialize font\n");
    TTF_Quit();
    SDL_Quit();
    return TERM_FAILURE;
  }

  /* Initialize the Sym Menu entries */
  symmenu_init();

  /* Don't show the mouse icon */
  SDL_ShowCursor(SDL_DISABLE);

  /* we allocate as much buffer as we will ever need */
  int largest_dimension = screen->w > screen->h ? screen->w : screen->h;
  MAX_ROWS = largest_dimension / MIN_FONT_SIZE;
  MAX_COLS = largest_dimension / MIN_FONT_SIZE;
  TEXT_BUFFER_SIZE = MAX_ROWS * 2;
  fprintf(stderr, "Allocating %d rows and %d cols\n",TEXT_BUFFER_SIZE, MAX_COLS);

  /* initialize the number of rows and columns */
  rows = screen->h / text_height;
  cols = screen->w / text_width;

  if(buf_init() == TERM_FAILURE){
    PRINT(stderr, "Couldn't initialize font\n");
    TTF_Quit();
    SDL_Quit();
    return TERM_FAILURE;
  }

  setup_screen_size(screen->w, screen->h);

  /* initialize the metamode key */
  metamode_doubletap_key = preferences_get_int(preference_keys.metamode_doubletap_key);
  /* and set the last 'press' */
  clock_gettime(CLOCK_MONOTONIC, &metamode_last);

  /* grab the doubletap interval */
  metamode_doubletap = (uint64_t)preferences_get_int(preference_keys.metamode_doubletap_delay);

  /* initialize the metamode_hitbox */
  int hb_val = preferences_get_int(preference_keys.metamode_hitbox.x);
  metamode_hitbox[0] = hb_val ? hb_val : preference_defaults.hitbox.x;
  hb_val = preferences_get_int(preference_keys.metamode_hitbox.y);
  metamode_hitbox[1] = hb_val ? hb_val : preference_defaults.hitbox.y;
  hb_val = preferences_get_int(preference_keys.metamode_hitbox.w);
  metamode_hitbox[2] = hb_val ? hb_val : preference_defaults.hitbox.w;
  hb_val = preferences_get_int(preference_keys.metamode_hitbox.h);
  metamode_hitbox[3] = hb_val ? hb_val : preference_defaults.hitbox.h;

  ecma48_init();

  return TERM_SUCCESS;
}

void uninit(){

  buf_uninit();

  SDL_DestroyMutex(input_mutex);

  symmenu_uninit();
  font_uninit();
  SDL_FreeSurface(screen);

  ecma48_uninit();

  TTF_Quit();
  SDL_Quit();

  preferences_uninit();

  io_uninit();
}

void render() {

  int i, j, offset;
  float x, y;
  struct screenchar* sc;
  SDL_Rect destrect;
  SDL_Rect symmenu_srcrect;
  SDL_Surface* torender;
  SDL_Color symmenu_background = (SDL_Color)SYMMENU_BACKGROUND;
  UChar str[2];
  str[1] = NULL;

  /* Set the background */
  SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, default_bg_color.r, default_bg_color.g, default_bg_color.b));

  for(i=0; i < rows; ++i){
    y = text_height * (i);
    x = 0.0;
    for(j=0; j < cols; ++j){
      /* guard against screen rotations that push the bottom of the screen past the
       * bottom of the buffer. */
      sc = i+buf->top_line < TEXT_BUFFER_SIZE ? &buf->text[i+buf->top_line][j] : &blank_sc;
      if((sc->surface == NULL) && (sc->c != 0)){
        // we have added a new char, but not rendered it yet
        str[0] = sc->c;
        TTF_SetFontStyle(font, sc->style.style);
        if(buf->inverse_video){
          sc->surface = TTF_RenderUNICODE_Shaded(font, str, sc->style.bg_color, sc->style.fg_color);
        } else {
          sc->surface = TTF_RenderUNICODE_Shaded(font, str, sc->style.fg_color, sc->style.bg_color);
        }
        if(sc->surface == NULL){
          PRINT(stderr, "Rendering failed for char %d\n", (int)sc->c);
        }
      }
      if(sc->surface == NULL || flash){
        // no glyph here - render blank
        if(buf->inverse_video){
          torender = flash ? blank_surface : flash_surface;
        } else {
          torender = flash ? flash_surface : blank_surface;
        }
      } else {
        torender = sc->surface;
      }
      destrect.x = x;
      destrect.y = y;
      destrect.w = torender->w;
      destrect.h = torender->h;
      if(SDL_BlitSurface(torender, NULL, screen, &destrect) != 0){
        PRINT(stderr, "Blit Failed: %s\n", SDL_GetError());
      }
      x += advance;
    }
  }

  if (draw_cursor){
    // draw the cursor
    /* Free the old cursor if we have one */
    SDL_FreeSurface(inv_cursor);
    inv_cursor = NULL;
    /* Get the character under the cursor */

    int drawcols = buf->col;
    if(buf->col == cols){
    	// Don't draw off the edge - also make backspace from the right margin work 'right'
    	drawcols -= 1;
    }

    sc = &buf->text[buf->line][drawcols];
    if(sc->c){
      str[0] = sc->c;
      TTF_SetFontStyle(font, sc->style.style);
      if(buf->inverse_video){
        inv_cursor = TTF_RenderUNICODE_Shaded(font, str, sc->style.fg_color, sc->style.bg_color);
      } else {
        inv_cursor = TTF_RenderUNICODE_Shaded(font, str, sc->style.bg_color, sc->style.fg_color);
      }
      if(inv_cursor == NULL){
        PRINT(stderr, "Rendering failed for char %d\n", (int)sc->c);
      }
    }
    cursor_x = drawcols;
    cursor_y = buf->line - buf->top_line;
    destrect.x = cursor_x * advance;
    destrect.y = cursor_y * text_height;
    destrect.w = cursor->w;
    destrect.h = cursor->h;
    if(inv_cursor != NULL){
      SDL_BlitSurface(inv_cursor, NULL, screen, &destrect);
    } else {
      SDL_BlitSurface(buf->inverse_video ? blank_surface: cursor, NULL, screen, &destrect);
    }
  }

  if(metamode && metamode_cursor != NULL){
    /* draw the metamode cursor */
    destrect.x = (cols-1) * advance;
    destrect.y = 0;
    destrect.w = metamode_cursor->w;
    destrect.h = metamode_cursor->h;
    SDL_BlitSurface(metamode_cursor, NULL, screen, &destrect);
  }

  if(vmodifiers & KEYMOD_CTRL){
    destrect.x = (cols-1) * advance;
    destrect.y = 1 * text_height;
    destrect.w = ctrl_key_indicator->w;
    destrect.h = ctrl_key_indicator->h;
    SDL_BlitSurface(ctrl_key_indicator, NULL, screen, &destrect);
  }

  if(vmodifiers & KEYMOD_ALT){
    destrect.x = (cols-1) * advance;
    destrect.y = 2 * text_height;
    destrect.w = alt_key_indicator->w;
    destrect.h = alt_key_indicator->h;
    SDL_BlitSurface(alt_key_indicator, NULL, screen, &destrect);
  }

  if(vmodifiers & KEYMOD_SHIFT){
    destrect.x = (cols-1) * advance;
    destrect.y = 3 * text_height;
    destrect.w = shift_key_indicator->w;
    destrect.h = shift_key_indicator->h;
    SDL_BlitSurface(shift_key_indicator, NULL, screen, &destrect);
  }

  if(symmenu_show && symmenu_num_rows > 0){
  	for(j = 0; j < symmenu_num_rows; ++j){
			for(i = 0; i < symmenu_num_entries[j]; ++i){
				// Draw the background box
			  destrect.w = symmenu_entries[j][i].background->w;
				destrect.h = symmenu_entries[j][i].symbol->h + (2 * SYMMENU_KEY_BORDER); // be just bigger than the symbol
				destrect.x = i * symmenu_entries[j][i].background->w;
				destrect.y = screen->h - ((j+1) * (destrect.h));
				symmenu_srcrect.x = 0;
				symmenu_srcrect.y = 0;
				symmenu_srcrect.w = destrect.w;
				symmenu_srcrect.h = destrect.h;
				symmenu_entries[j][i].x = destrect.x;
				symmenu_entries[j][i].y = destrect.y;
				if(SDL_BlitSurface(symmenu_entries[j][i].background, &symmenu_srcrect, screen, &destrect) != 0){
					PRINT(stderr, "Blit Failed: %s\n", SDL_GetError());
				}

				// Fill the background of the symbol
				destrect.w = symmenu_entries[j][i].background->w - (2 * SYMMENU_KEY_BORDER);
				destrect.h = symmenu_entries[j][i].symbol->h;
				destrect.x += SYMMENU_KEY_BORDER;
				destrect.y += SYMMENU_KEY_BORDER;
				SDL_FillRect(screen, &destrect, SDL_MapRGB(screen->format, symmenu_background.r, symmenu_background.g, symmenu_background.b));

				// Draw the symbol
				destrect.x = (i * symmenu_entries[j][i].background->w) + symmenu_entries[j][i].off_x;
				destrect.w = symmenu_entries[j][i].symbol->w;
				destrect.h = symmenu_entries[j][i].symbol->h;
				if(symmenu_entries[j][i].flash){
				  symmenu_entries[j][i].flash = 0;
				  if(SDL_BlitSurface(symmenu_entries[j][i].flash_symbol, NULL, screen, &destrect) != 0){
				    PRINT(stderr, "Blit Failed: %s\n", SDL_GetError());
				  }
				} else {
				  if(SDL_BlitSurface(symmenu_entries[j][i].symbol, NULL, screen, &destrect) != 0){
				    PRINT(stderr, "Blit Failed: %s\n", SDL_GetError());
				  }
				}

				// Draw the key
        destrect.x = i * symmenu_entries[j][i].background->w + SYMMENU_KEY_BORDER;
				destrect.w = symmenu_entries[j][i].key->w;
				destrect.h = symmenu_entries[j][i].key->h;
				if(SDL_BlitSurface(symmenu_entries[j][i].key, NULL, screen, &destrect) != 0){
					PRINT(stderr, "Blit Failed: %s\n", SDL_GetError());
				}
			}
  	}
  }

  SDL_Flip(screen);

  if(flash){
    /* turn it off */
    flash = 0;
    /* write to the input pipe so we run again */
    indicate_event_input();
  }

}

int init_pty() {
  // Set up the ttys and fork

  struct winsize winp;

  /* some sensible defaults - we change these later */
  winp.ws_row = 24;
  winp.ws_col = 80;
  winp.ws_xpixel = 1024;
  winp.ws_ypixel = 600;

  int pty_ret;
  int fd;
  int uid = getuid();
  int gid = getgid();
  char cttyname[L_ctermid];
  char envstr[100];
  int slave_fd;
  int master_fd;

  pty_ret = openpty(&master_fd, &slave_fd, slave_ptyname, NULL, &winp);
  if (pty_ret != 0){
    // error
    PRINT(stderr, "openpty returned: %s\n", strerror(errno));
    close(master_fd);
    close(slave_fd);
    return TERM_FAILURE;
  } else {
    PRINT(stderr, "openpty returned name: %s\n", slave_ptyname);
  }

  // turn off blocking on the master pty
  fcntl(master_fd, F_SETFL, fcntl(master_fd, F_GETFL) | O_NONBLOCK);

  // store the master_fd in IO
  io_set_master(master_fd);

  // fork and exec
  child_pid = fork();

  if (child_pid == 0) {
    // Child
    /*
      struct termios tios;
      if (tcgetattr(STDIN_FILENO, &tios) >= 0)
      {
        tios.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
        tios.c_oflag &= ~(ONLCR);
        (void) tcsetattr(STDIN_FILENO, TCSANOW, &tios);
      }
     */

    PRINT(stderr, "fork returned in child\n");
    ctermid(cttyname);
    PRINT(stderr, "controlling tty is: %s\n", cttyname);

    if(setuid(uid)<0) {PRINT(stderr, "ERROR (setuid)\n");}
    if(setgid(gid)<0) {PRINT(stderr, "ERROR (setgid)\n");}
    if(setsid()<0) {PRINT(stderr, "ERROR (setsid)\n");}

    if(ioctl(slave_fd, TIOCSCTTY, NULL)) {
      PRINT(stderr, "ERROR! (ioctl): %s\n", strerror(errno));
    }

    dup2(slave_fd, STDIN_FILENO);
    dup2(slave_fd, STDOUT_FILENO);
    dup2(slave_fd, STDERR_FILENO);

    ecma48_setenv();

    /* add in our private binary path */
    char* home = getenv("SANDBOX");
    char* path = getenv("PATH");
    char* root = "app/native/root/bin";
    char* newpath;
    int err = 0;
    int newpath_len = 0;
    if(home == NULL || path == NULL){
      fprintf(stderr, "Could not get $HOME or $PATH - not setting private bin dir.\n");
    } else {
      newpath_len = strlen(home) + strlen(path) + strlen(root) + 10;
      newpath = calloc(newpath_len, sizeof(char));
      if(newpath == NULL){
        fprintf(stderr, "Could not calloc new $PATH - not setting private bin dir..\n");
      } else {
        err = snprintf(newpath, newpath_len, "PATH=%s/%s:%s\n", home, root, path);
        if(err > 0){
          err = putenv(strdup(newpath));
          if(err < 0){
            fprintf(stderr, "Error in putenv: %d\n%s - private bin may not bein $PATH.\n", errno, newpath);
          }
        } else {
          fprintf(stderr, "Error snprintf setting $PATH: %d\n", errno);
        }
        free(newpath);
      }
    }

    /* Set LC_CTYPE=en_US.UTF-8
     * Which can be overridden in .profile */
    setenv("LC_CTYPE", "en_US.UTF-8", 0);
    if(execl("../app/native/lib/mksh", "mksh", "-l", (char*)0) == -1){
      execl("/bin/sh", "sh", "-l", (char*)0);
    }
  }
  if (child_pid == -1){
    PRINT(stderr, "fork returned: %s\n", strerror(errno));
    return TERM_FAILURE;
  }

  // close the slave_fd, not needed anymore
  close(slave_fd);
  return TERM_SUCCESS;
}

extern int SDL_PrivateQuit(void);
void sig_child(int signo){
  int status;

  int old_errno = errno;

  if(waitpid(child_pid, &status, WNOHANG)){
    if(WIFEXITED(status)){
      PRINT(stderr, "Child %d exited normally with status %d\n", child_pid, WEXITSTATUS(status));
    } else {
      PRINT(stderr, "Child %d exited abnormally\n", child_pid);
    }
    exit_application = 1;
    SDL_PrivateQuit();
  } else {
    PRINT(stderr, "Got SIGCHILD for process other than %d\n", child_pid);
  }
  errno = old_errno;
}

/* This function is run in an SDL_Thread, and will check
 * for either input event indication or data from the
 * shell, then run the render loop
 */
int run_render(void* data){

  fd_set fds;
  char ev_buf[100];
  int n = 0;
  UChar lbuf[READ_BUFFER_SIZE];
  ssize_t num_chars = 0;
  int master = io_get_master();
  while(!exit_application){
    FD_ZERO(&fds);
    FD_SET(master, &fds);
    FD_SET(event_pipe[0], &fds);
    n = select(1+max(master, event_pipe[0]), &fds, NULL, NULL, NULL);
    if(n < 0){
      printf("Error calling select on inputs: %d\n", errno);
    } else {
      if(FD_ISSET(master, &fds)){
        lock_input();
        // Read anything from the child
        while ((num_chars = io_read_master(lbuf, READ_BUFFER_SIZE)) > 0){
          ecma48_filter_text(lbuf, num_chars);
        }
        unlock_input();
      }
      if(FD_ISSET(event_pipe[0], &fds)){
        // Just read the stuff and throw it away
        read(event_pipe[0], (void*)ev_buf, 99);
      }
    }
    PRINT(stderr, "Render Loop\n");
    lock_input();
    render();
    unlock_input();
  }
  /* never reached */
  return 0;
}

int main(int argc, char **argv) {
  int rc;

  /* Switch to our home directory */
  char* home = getenv("HOME");
  if(home != NULL){
    chdir(home);
  }

  /* set auto orientation */
  setenv("AUTO_ORIENTATION", "1", 0);

  // Initialize IO
  if (TERM_SUCCESS != io_init()) {
    PRINT(stderr, "Unable to initialize IO\n");
    uninit();
    return TERM_FAILURE;
  }

  // Initialize pty
  if (TERM_SUCCESS != init_pty()) {
    PRINT(stderr, "Unable to initialize pty/tty\n");
    uninit();
    return TERM_FAILURE;
  }

  PRINT(stderr, "Pty init\n");

  /* Install signal handler for SIGCHILD */
  struct sigaction act;
  act.sa_handler = &sig_child;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_NOCLDSTOP;
  if (sigaction(SIGCHLD, &act, NULL) < 0) {
    PRINT(stderr, "sigaction failed\n");
    uninit();
    return TERM_FAILURE;
  }

  PRINT(stderr, "Sig handler installed\n");

  //Initialize app data
  if (TERM_SUCCESS != init()) {
    PRINT(stderr, "Unable to initialize app logic\n");
    uninit();
    return TERM_FAILURE;
  }

  PRINT(stderr, "App init\n");

  init_virtualkeyboard();

  SDL_Thread *render_thread = SDL_CreateThread(run_render, NULL);
  while (!exit_application) {

    //Request and process all available events
    SDL_Event event;

    SDL_WaitEvent(&event);
    lock_input();
    switch (event.type) {
      case SDL_QUIT:
        exit_application = 1;
        break;
      case SDL_VIDEORESIZE:
        rescreen(event.resize.w, event.resize.h);
        break;
      case SDL_KEYDOWN:
      {
        fprintf(stderr, "SDL_KEYDOWN\n");
        UChar uc;
        char sdlkey = event.key.keysym.sym;
        uc = (UChar)sdlkey;
        io_write_master(&uc, 1);
      }
      break;
      case SDL_SYSWMEVENT:
      {
        bps_event_t* bps_event = event.syswm.msg->event;
        int screene_type;
        int domain = bps_event_get_domain(bps_event);
        PRINT(stderr, "Unhandled SYSWMEVENT: %d\n", domain);
      }
      break;
      case SDL_MOUSEBUTTONDOWN:
        handle_mousedown(event.button.x, event.button.y);
        break;
      case SDL_ACTIVEEVENT:
        handle_activeevent(event.active.gain, event.active.state);
        break;
      default:
        PRINT(stderr, "Unknown Event: %d\n", event.type);
        break;
    }
    indicate_event_input();
    unlock_input();
  }

  PRINT(stderr, "Exiting run loop\n");
  SDL_KillThread(render_thread);
  virtualkeyboard_hide();
  uninit();

  return 0;
}
