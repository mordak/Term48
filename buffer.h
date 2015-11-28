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

#ifndef BUFFER_H_
#define BUFFER_H_

#include <unicode/utf.h>

#define NUM_BUFFERS 2

struct font_style {
  SDL_Color fg_color;
  SDL_Color bg_color;
  int style;
};
extern struct font_style default_text_style;

struct screenchar {
  UChar c;
  struct font_style style;
  SDL_Surface* surface;
};

struct text {
  struct screenchar** text;
  struct screenchar** screens[NUM_BUFFERS];
  int line;
  int col;
  int top_line;
  char inverse_video;
  char origin;
  struct font_style current_style;
};
typedef struct text buf_t;

struct scroll_region {
  int top;
  int bottom;
};
struct scroll_region sr;

buf_t buf;
buf_t saved_buf;
int rows;
int cols;
char ** tabs;

SDL_Surface* blank_surface;

int MAX_COLS;
int MAX_ROWS;
int TEXT_BUFFER_SIZE;

#define TAB_WIDTH 8
#define TAB_HEIGHT 2

int buf_bottom_line();
void buf_erase_line(struct screenchar* sc, size_t n);
void buf_erase_lines(int start_line, int num);
void buf_free_char(struct screenchar* sc);
void buf_check_screen_scroll();
void buf_check_screen_rscroll();
void buf_increment_line();
void buf_decrement_line();
void buf_delete_character_following(int n);
void buf_delete_character_preceding(int n);
void buf_insert_character_following(int n);
void buf_insert_character_preceding(int n);
void buf_insert_character(char shift_preceding);


int buf_to_screen_col(int col_buf);
int buf_to_screen_row(int row_buf);
int buf_to_screen_row_origin(int row_buf);
int screen_to_buf_col(int col_screen);
int screen_to_buf_row(int row_screen);
int buf_in_scroll_region();

void buf_init_tabstops(char* tabs);
int screen_next_tab_x();
int screen_prev_tab_x();
void buf_init_vtab(int i);
int buf_next_vtab(int row);
void buf_clear_vtab(int i);
void buf_clear_all_vtabs();

void clear_char_tabstop_at(int row, int col);
void clear_char_tabstops_on_row(int row);
void clear_all_char_tabstops();
void buf_clear_all_renders();

void buf_save_text();
void buf_restore_text();

#endif /* BUFFER_H_ */
