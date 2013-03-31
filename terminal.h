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

#define DEBUGMSGS 0
#if DEBUGMSGS
#define PRINT(args...) fprintf(args)
#else
#define PRINT(args...)
#endif

#define TERM_SUCCESS 0
#define TERM_FAILURE 1

#define READ_BUFFER_SIZE 5000
#define CHARACTER_BUFFER 10


void rescreen();
void setup_screen_size(int w, int h);
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

#endif /* TERMINAL_H_ */
