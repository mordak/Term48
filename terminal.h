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

#ifndef TERMINAL_H_
#define TERMINAL_H_

/* #define DEBUGMSGS 0 */
#ifdef DEBUGMSGS
#define PRINT(args...) fprintf(args)
#else
#define PRINT(args...)
#endif

#define NIPRINT(args...)
//#define NIPRINT(args...) fprintf(args)

#define BETWEEN(x, low, high) ((x) >= (low) && (x) <= (high))

#define TERM_SUCCESS 0
#define TERM_FAILURE 1

#define READ_BUFFER_SIZE 5000
#define CHARACTER_BUFFER 10
#define MIN_FONT_SIZE 4

// sym = f0d3 // z = 0x007a
#define KEYCODE_BB_SYM_KEY 0xf0d3

void rescreen();
void setup_screen_size(int w, int h);
void set_screen_cols(int cols);
void first_run();

int init();
void uninit();

#define BLACK       {0,   0,    0,    0}
#define RED         {205, 0,    0,    0}
#define GREEN       {0,   205,  0,    0}
#define YELLOW      {205, 205,  0,    0}
#define BLUE        {0,   0,    238,  0}
#define MAGENTA     {205, 0,    205,  0}
#define CYAN        {0,   205,  205,  0}
#define GRAY        {229, 229,  229,  0}
#define BT_GRAY     {127, 127,  127,  0}
#define BT_RED      {255, 0,    0,    0}
#define BT_GREEN    {0,   255,  0,    0}
#define BT_YELLOW   {255, 255,  0,    0}
#define BT_BLUE     {92,  92,   255,  0}
#define BT_MAGENTA  {255, 0,    255,  0}
#define BT_CYAN     {0,   255,  255,  0}
#define WHITE       {255, 255,  255,  0}

#define SYMMENU_BORDER     {50,  50,  50,  0}
#define SYMMENU_BACKGROUND {38,  38,  38,  0}
#define SYMMENU_CORNER     {153, 153, 153, 0}
#define SYMMENU_FONT       {255, 255, 255, 0}

#endif /* TERMINAL_H_ */
