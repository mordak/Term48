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

#include <strings.h>

#include "SDL.h"
#include "SDL_ttf.h"
#include "terminal.h"

#include "buffer.h"

static uint32_t saved_buf_p;
static saved_buf_t* saved_buf;
static buf_t* screens;

buf_t* buf;
int rows;
int cols;
char ** tabs;
struct scroll_region sr;

int MAX_COLS;
int MAX_ROWS;
int TEXT_BUFFER_SIZE;

/* from main.c */
extern SDL_Surface* blank_surface;
extern struct font_style default_text_style;

/* assumes that MAX_COLS, MAX_ROWS, TEXT_BUFFER_SIZE are set already */
int buf_init(){

  int i,n;
  screens = (buf_t*)calloc(NUM_BUFFERS, sizeof(buf_t));
  for(n = 0; n < NUM_BUFFERS; ++n){
    buf = &screens[n];
    buf->line = 0;
    buf->col = 0;
    buf->top_line = 0;
    buf->inverse_video = 0;
    buf->origin = 0;
    buf->current_style = default_text_style;
    /* malloc the text buf structures */
    buf->text = (struct screenchar**)calloc(TEXT_BUFFER_SIZE + 1, sizeof(struct screenchar*));
    if(buf->text == NULL){ return TERM_FAILURE;}
    for(i=0; i< TEXT_BUFFER_SIZE + 1;++i){
      buf->text[i] = (struct screenchar*)calloc(MAX_COLS+1, sizeof(struct screenchar));
      if(buf->text[i] == NULL){ return TERM_FAILURE;}
      buf_erase_line(buf->text[i], (size_t)MAX_COLS);
    }
  }
  buf = &screens[0];
  /* FIXME: scroll regions and tab stops should be in buf_t */

  /* initialize the scroll_region */
  sr.top = 1;
  sr.bottom = rows;

  /* initialize the tab stops
   * tab stops are in screen coords (1,1)->(cols,rows)
   * We use the extra column for vertical tabs
   * */
  tabs = (char**)calloc((MAX_ROWS+1), sizeof(char*));
  for(i=1; i <= MAX_ROWS; ++i){
    tabs[i] = (char*)calloc((MAX_COLS+1), sizeof(char));
    if(tabs[i] == NULL){ return TERM_FAILURE; }
    buf_init_tabstops(tabs[i]);
    if(i % TAB_HEIGHT == 0){
      buf_init_vtab(i);
    }
  }
  saved_buf_p = 0;
  saved_buf = (saved_buf_t*)calloc(NUM_BUFFERS, sizeof(saved_buf_t));
  /*
  fprintf(stderr, "init buffers sb_bif:%x\n", (uint)saved_buf);
  fprintf(stderr, "  - saved_buf[0] at:%x\n", (uint)&(saved_buf[0]));
  fprintf(stderr, "  - saved_buf[1] at:%x\n", (uint)&(saved_buf[1]));
  */
  /* if we restore without ever saving, then we should
   * get good values. Note that this assumes current_style
   * is valid, which means font_init comes before buf_init */
  for(i = 0; i < NUM_BUFFERS; ++i){
    saved_buf_p = i;
    buf_save_cursor();
  }
  saved_buf_p = 0;

  return TERM_SUCCESS;
}

void buf_uninit(){
  int i, n;
  for(n = 0; n < NUM_BUFFERS; ++n){
    buf = &screens[n];
    for(i=0; i< TEXT_BUFFER_SIZE + 1;++i){
      if(buf->text[i] != NULL){
        buf_erase_line(buf->text[i], (size_t)MAX_COLS);
        free(buf->text[i]);
      }
    }
    free(buf->text);
  }
  for(i=1; i <= MAX_ROWS; ++i){
    free(tabs[i]);
  }
  free(tabs);
}

/* returns the bottom buffer line showing on the screen */
int buf_bottom_line(){
  return buf->top_line + rows - 1;
}

/* Screen coordinates and buffer coordinates.
 *
 * In ANSI terminals (ECMA48, etc), the screen starts
 * at 1,1, and goes to cols, rows.
 * In our buffer, we start at 0,0 and go to cols-1, rows-1.
 *
 * These functions translate between the two. You can pass
 * -1 to these functions to get the current insertion position,
 * which is where the cursor is.
 */
int buf_to_screen_col(int x_buf){
  if(x_buf < 0){
    x_buf = buf->col;
  }
  return x_buf >= cols ? cols : x_buf + 1;
}

/* return value < 1 indicates off screen */
int buf_to_screen_row(int y_buf){
  if(y_buf < 0){
    y_buf = buf->line;
  }
  return y_buf > buf->top_line + rows - 1 ? rows : y_buf - buf->top_line + 1;
}

int buf_to_screen_row_origin(int y_buf){
  int r = 0;
  if(y_buf < 0){
    y_buf = buf->line;
  }
  r = buf_to_screen_row(y_buf);
  if(buf->origin && buf_in_scroll_region()){
    r = r - sr.top + 1;
  }
  return r;
}

int screen_to_buf_col(int x_screen){
  if(x_screen < 0){
    return buf->col;
  }
  return x_screen > cols ? cols - 1 : x_screen - 1;
}

int screen_to_buf_row(int y_screen){
  if(y_screen < 0){
    return buf->line;
  }
  return y_screen > rows ? buf_bottom_line() : buf->top_line + y_screen - 1;
}


void buf_free_char(struct screenchar* sc){
  if((sc->surface != NULL) && (sc->surface != blank_surface)){
    SDL_FreeSurface(sc->surface);
  }
  sc->surface = NULL;
}

void buf_erase_line(struct screenchar* sc, size_t n){
  size_t i;
  for(i = 0; i < n; ++i){
    buf_free_char(&sc[i]);
    sc[i].c = ' ';
    sc[i].style = buf->current_style;
  }
}

void buf_erase_lines(int start_line, int num){
	int i;
	for(i = 0; i < num; ++i){
		buf_erase_line(buf->text[start_line + i], cols);
	}
}


void buf_init_tabstops(char* tabs){
  int i;
  for(i=1; i <= MAX_COLS; i += TAB_WIDTH){
    tabs[i] = 1;
  }
}

int screen_next_tab_x(){
  int screenx;
  int screeny = buf_to_screen_row(-1);
  char* tabline = tabs[screeny];
  for(screenx = buf_to_screen_col(-1) + 1; screenx <= cols; ++screenx){
    if(tabline[screenx]){
      return screenx;
    }
  }
  return cols;
}

int screen_prev_tab_x(){
  int screenx;
  int screeny = buf_to_screen_row(-1);
  char* tabline = tabs[screeny];
  for(screenx = buf_to_screen_col(-1) - 1; screenx >= 1; --screenx){
    if(tabline[screenx]){
      return screenx;
    }
  }
  return 1;
}

void buf_init_vtab(int i){
  tabs[i][MAX_COLS] = 1;
}

void buf_clear_vtab(int i){
  tabs[i][MAX_COLS] = 0;
}

void buf_clear_all_vtabs(){
  int i = 0;
  for(i=1; i<=rows; ++i) {
    buf_clear_vtab(i);
  }
}

int buf_next_vtab(int row){
  int i = 0;
  for(i = row+1; i <= rows; ++i){
    if(tabs[i][MAX_COLS] == 1){
      return i;
    }
  }
  /* no vtab - return -1 */
  return -1;
}

void clear_char_tabstop_at(int row, int col){
  tabs[row][col] = 0;
}
void clear_char_tabstops_on_row(int row){
  bzero(tabs[row], (MAX_COLS) * sizeof(char));
}
void clear_all_char_tabstops(){
  int i;
  for(i=1; i<=rows; ++i) {
    clear_char_tabstops_on_row(i);
  }
}

void buf_check_screen_scroll(){

  struct screenchar* tmp;

  // check if we are out of buffer
  if(buf->line >= TEXT_BUFFER_SIZE - 1) {
    // then rotate the buffer space
    // top half of the buffer is pushed into the bottom
    int shift = TEXT_BUFFER_SIZE / 2;
    int i = 0;
    for(i = 0; i < shift; ++i){
      tmp = buf->text[i];
      buf->text[i] = buf->text[i+shift];
      buf->text[i+shift] = tmp;
      buf_erase_line(buf->text[i+shift], MAX_COLS);
    }
    // and update the pointers
    buf->top_line -= shift;
    buf->line -= shift;
  }

  // and check if we have scrolled the screen
  if(buf->line - buf->top_line >= rows){
    // if the buffer last line is now behind the writing line
    // increment the buffer top line
    buf->top_line = buf->line - (rows - 1);
    // and erase the newly revealed line
    buf_erase_line(buf->text[buf->line], cols);
  }

  PRINT(stderr, "top line = %d, text_line = %d\n", buf->top_line, buf->line);
}

void buf_check_screen_rscroll(){

  struct screenchar* tmp;

  // check if we are out of buffer
  if(buf->line < 0) {
    int shift = TEXT_BUFFER_SIZE / 2;
    int i = 0;
    for(i = 0; i < shift; ++i){
      tmp = buf->text[i + shift];
      buf->text[i+shift] = buf->text[i];
      buf->text[i] = tmp;
      buf_erase_line(buf->text[i], cols);
    }
    // and update the pointers
    buf->top_line += shift;
    buf->line += shift;
  }

  // and check if we have scrolled the screen upwards
  if(buf->line < buf->top_line){
    // if the buffer writing line is now above the top line
    // move the top line to the writing line
    buf->top_line = buf->line;
    // and erase the newly revealed line
    buf_erase_line(buf->text[buf->line], cols);
  }

  PRINT(stderr, "top line = %d, text_line = %d\n", buf->top_line, buf->line);
}

int buf_scroll_region_set(){
  return !(sr.top == 1 && sr.bottom == rows);
}

int buf_in_scroll_region(){
  return (((buf->line - buf->top_line + 1) <= sr.bottom) && ((buf->line - buf->top_line + 1) >= sr.top));
}

int buf_at_top_of_scroll_region(){
  return ((buf->line - buf->top_line + 1) == sr.top);
}

int buf_at_end_of_scroll_region(){
  return ((buf->line - buf->top_line + 1) == sr.bottom);
}

void buf_scroll_scroll_region(){
  struct screenchar* tmp = buf->text[buf->top_line + (sr.top - 1)];
  int j;
  for(j=sr.top; j<sr.bottom; ++j){
    buf->text[buf->top_line + j - 1] = buf->text[buf->top_line + j];
  }
  buf->text[buf->top_line + sr.bottom - 1] = tmp;
  buf_erase_line(buf->text[buf->top_line + sr.bottom - 1], cols);
}

void buf_rscroll_scroll_region(){
  struct screenchar* tmp = buf->text[buf->top_line + (sr.bottom - 1)];
  int j;
  for(j=sr.bottom-1; j>=sr.top; --j){
    buf->text[buf->top_line + j] = buf->text[buf->top_line + j -1];
  }
  buf->text[buf->top_line + sr.top - 1] = tmp;
  buf_erase_line(buf->text[buf->top_line + sr.top - 1], cols);
}

void buf_increment_line(){

  if(!buf_scroll_region_set() || ((buf->line - buf->top_line) >= (rows-1))){
    // if the scrolling region is the whole screen
    // or we are writing to the bottom of the screen
    // then just scroll the whole buffer
    ++buf->line;
    buf_check_screen_scroll();
  } else {
    if (buf_in_scroll_region()){
      // we have some kind of scrolling region that we are inside
      if (buf_at_end_of_scroll_region()){
        // and we are at the end of it, so scroll
        buf_scroll_scroll_region();
      } else {
        // we're not scrolling the scroll region
        // just increment
        ++buf->line;
      }
    } else {
      // we are incrementing outside the scroll region
      // increment and hope the program knows what its doing
      ++buf->line;
    }
  }
}

void buf_decrement_line(){
  if(!buf_scroll_region_set() || (buf->line < buf->top_line)){
    // if the scrolling region is the whole screen
    // or we are writing to the top of the screen
    // then just scroll the whole buffer
    --buf->line;
    buf_check_screen_rscroll();
  } else {
    if (buf_in_scroll_region()){
      // we have some kind of scrolling region that we are inside
      if (buf_at_top_of_scroll_region()){
        // and we are at the top of it, so scroll
        buf_rscroll_scroll_region();
      } else {
        // we're not scrolling the scroll region
        // just decrement
        --buf->line;
      }
    } else {
      // we are decrementing outside the scroll region
      // decrement and hope the program knows what its doing
      --buf->line;
    }
  }
}

/* pass 0 to shift following characters or
 * pass 1 to shift preceding characters
 */
void buf_delete_character(char shift_preceding){

  /* start by blanking out the char under the cursor */
  buf_erase_line(&buf->text[buf->line][buf->col], 1);
  /* then shift */
  struct screenchar sc = buf->text[buf->line][buf->col];
  int i;
  if(shift_preceding){
    for(i = buf->col; i > 0; --i){
      buf->text[buf->line][i] = buf->text[buf->line][i-1];
    }
    buf->text[buf->line][0] = sc;
  } else {
    for(i = buf->col; i < cols - 1; ++i){
      buf->text[buf->line][i] = buf->text[buf->line][i+1];
    }
    buf->text[buf->line][cols-1] = sc;
  }
}

void buf_delete_character_following(int n){
  /* sanity check n */
  n = n > cols - buf->col ? cols - buf->col : n;
  int i;
  for(i = 0; i < n; ++i){
    buf_delete_character(0);
  }
}

void buf_delete_character_preceding(int n){
  /* sanity check n */
  n = n > buf->col + 1 ? buf->col + 1 : n;
  int i;
  for(i = 0; i < n; ++i){
    buf_delete_character(1);
  }
}


/* pass 0 to shift following characters or
 * pass 1 to shift preceding characters
 */
void buf_insert_character(char shift_preceding){

  /* shift */
  struct screenchar sc;
  int i;
  if(shift_preceding){
    sc = buf->text[buf->line][0];
    for(i = 0; i < buf->col; ++i){
      buf->text[buf->line][i] = buf->text[buf->line][i+1];
    }
    buf->text[buf->line][buf->col] = sc;
  } else {
    sc = buf->text[buf->line][cols-1];
    for(i = cols-1; i > buf->col; --i){
      buf->text[buf->line][i] = buf->text[buf->line][i-1];
    }
    buf->text[buf->line][buf->col] = sc;
  }
  /* end by blanking out the char under the cursor */
  buf_erase_line(&buf->text[buf->line][buf->col], 1);
}

void buf_insert_character_following(int n){
  /* sanity check n */
  n = n > cols - buf->col ? cols - buf->col : n;
  int i;
  for(i = 0; i < n; ++i){
    buf_insert_character(0);
  }
}

void buf_insert_character_preceding(int n){
  /* sanity check n */
  n = n > buf->col + 1 ? buf->col + 1 : n;
  int i;
  for(i = 0; i < n; ++i){
    buf_insert_character(1);
  }
}

void buf_clear_all_renders(){
  int i = 0;
  int j = 0;
  for(i=0; i<TEXT_BUFFER_SIZE; ++i){
    for(j=0; j<MAX_COLS;++j){
      buf_free_char(&(buf->text[i][j]));
    }
  }
}

void buf_reset_text_buffer(buf_t* toclear){
  int i = 0;
  int j = 0;
  for(i=0; i<TEXT_BUFFER_SIZE; ++i){
    for(j=0; j<MAX_COLS;++j){
      buf_free_char(&(toclear->text[i][j]));
      (toclear->text[i][j]).c = 0;
      (toclear->text[i][j]).style = default_text_style;
    }
  }
  toclear->top_line = 0;
  toclear->line = 0;
  toclear->col = 0;
  toclear->current_style = default_text_style;
  toclear->inverse_video = 0;
  toclear->origin = 0;
}

void buf_save_text(){
  saved_buf_p = (saved_buf_p + 1) % NUM_BUFFERS;
  /* clear the new buffer */
  buf = &(screens[saved_buf_p]);
  buf_reset_text_buffer(buf);
}

void buf_restore_text(){
  /* clear the old buffer */
  buf_reset_text_buffer(buf);
  /* and swap */
  saved_buf_p = (saved_buf_p - 1) % NUM_BUFFERS;
  buf = &(screens[saved_buf_p]);
}

void buf_save_cursor(){
  PRINT(stderr, "\nSaving buffer variables: top_line: %d, line: %d\n", buf->top_line, buf->line);
  saved_buf_t *sb = &(saved_buf[saved_buf_p]);
  sb->row = buf_to_screen_row(buf->line);
  sb->col = buf_to_screen_col(buf->col);
  sb->origin = buf->origin;
  sb->current_style = buf->current_style;
  sb->inverse_video = buf->inverse_video;
  PRINT(stderr, "\nSaved_buf_stuff: saved_buf_p: %d, row:%d, sb_mem:%x\n", saved_buf_p, sb->row, (uint32_t)sb);
}

void buf_restore_cursor(){
  saved_buf_t *sb = &(saved_buf[saved_buf_p]);
  PRINT(stderr, "\nRestoring saved_buf_stuff: saved_buf_p: %d, row:%d, sb_mem:%x\n", saved_buf_p, sb->row, (uint32_t)sb);
  buf->line = screen_to_buf_row(sb->row);
  buf->col  = screen_to_buf_col(sb->col);
  buf->origin = sb->origin;
  buf->current_style = sb->current_style;
  buf->inverse_video = sb->inverse_video;
  PRINT(stderr, "\n Restored buffer variables: top_line: %d, line: %d\n", buf->top_line, buf->line);
  if(buf->line < 0){
    PRINT(stderr, " --- Error - buf->line was < 0, this shouldn't happen\n");
    buf->line = buf->top_line;
  }
}
