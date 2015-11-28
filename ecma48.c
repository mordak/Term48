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

#include <sys/keycodes.h>
#include <unicode/utf.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>


#include "SDL_ttf.h"

#include "terminal.h"
#include "buffer.h"
#include "io.h"

#include "ecma48.h"

#define ECMA48_TERMINFO "../app/native/terminfo"
#define ECMA48_STATE_NORMAL 0
#define ECMA48_STATE_C1 1
#define ECMA48_STATE_CSI 2
#define ECMA48_STATE_ANSI 3
#define ECMA48_STATE_ANSI_POUND 4
static char state = ECMA48_STATE_NORMAL;

#define BUFFER_NORMAL 0
#define BUFFER_OSC 1
static char writing_buffer = BUFFER_NORMAL;

static char autowrap = 1;
//static int tabstop = 8;

#define NUM_ESCAPE_ARGS 16
struct escape_arguments {
  char** args;
  int num;
  int pos;
};

static struct escape_arguments escape_args;

struct ecma48_modes {
  char BDSM;
  char CRM;
  char DCSM;
  char ERM;
  char FEAM;
  char FETM;
  char GATM;
  char GRCM;
  char HEM;
  char IRM;
  char KAM;
  char MATM;
  char PUM;
  char SATM;
  char SRM;
  char SRTM;
  char TSM;
  char LNM;
  char TTM;
  char VEM;
  char ZDM;
  char SIMD;
  char DECOM;
};

static struct ecma48_modes modes;
static struct font_style current_style;
static UChar last_char;

extern buf_t buf;
extern int rows;
extern int cols;
extern struct font_style default_text_style;
extern struct scroll_region sr;
extern char draw_cursor;
extern char flash;

void ecma48_escape_args_init(){
  int i = 0;
  for(i = 0; i < NUM_ESCAPE_ARGS; ++i){
      bzero(escape_args.args[i], NUM_ESCAPE_ARGS * sizeof(char));
  }
  escape_args.num = 0;
  escape_args.pos = 0;
}

void ecma48_parameter_arg_add(char c){
  if(escape_args.pos < NUM_ESCAPE_ARGS){
    escape_args.args[escape_args.num][escape_args.pos++] = c;
  }
}

void ecma48_parameter_arg_next(){
  if(escape_args.num < NUM_ESCAPE_ARGS){
    escape_args.args[escape_args.num++][escape_args.pos] = '\0';
  }
  escape_args.pos = 0;
}

/* Convenience function for clearing all lines on the screen */
void ecma48_clear_display(){
	int i = 0;
	for(i=0; i<rows; ++i){
		buf_erase_line(buf.text[buf.top_line + i], cols);
	}
}

/* Convenience function to set cursor origin. Calls CUP with no arguments. */
void ecma48_set_cursor_home(){
	escape_args.args[0][0] = '\0';
	escape_args.args[1][0] = '\0';
	ecma48_CUP();
}

void ecma48_init(){
  int i;
  /* malloc the escape code arguments */
  escape_args.args = (char**)calloc(NUM_ESCAPE_ARGS, sizeof(char*));
  for(i=0; i< NUM_ESCAPE_ARGS;++i){
    escape_args.args[i] = (char*)calloc(NUM_ESCAPE_ARGS, sizeof(char));
  }

  /* set the initial modes */
  modes.BDSM = 0;
  modes.CRM = 0;
  modes.DCSM = 0;
  modes.ERM = 0;
  modes.FEAM = 0;
  modes.FETM = 0;
  modes.GATM = 0;
  modes.GRCM = 0;
  modes.HEM = 0; /* 0 for FOLLOWING, 1 for PRECEDING */
  modes.IRM = 0;
  modes.KAM = 0;
  modes.MATM = 0;
  modes.PUM = 0;
  modes.SATM = 0;
  modes.SRM = 0;
  modes.SRTM = 0;
  modes.TSM = 0;
  modes.LNM = 0;
  modes.TTM = 0;
  modes.VEM = 0;
  modes.ZDM = 0;
  modes.SIMD = 0;

  current_style = default_text_style;
}

void ecma48_uninit(){
  int i;
  for(i=0; i< NUM_ESCAPE_ARGS;++i){
    if(escape_args.args[i] != NULL){
      free(escape_args.args[i]);
    }
  }
  if(escape_args.args != NULL){
    free(escape_args.args);
  }

}

void ecma48_setenv(){
  /* link in the .terminfo lib if it isn't there */
  struct stat terminfo_f;
  if(stat(".terminfo", &terminfo_f) == -1){
    /* assume it doesn't exist yet */
    if(symlink(ECMA48_TERMINFO, ".terminfo") == -1){
      fprintf(stderr, "Error linking terminfo database - terminal may be non-functional\n");
    }
  }
  setenv("TERM", "xterm-color", 1);
  /*
  if(system("/base/bin/stty +sane term=xterm-color erase=^H") == -1){
    PRINT(stderr, "Error invoking system(stty..)\n");
  }
  */
}

void ecma48_end_control(){
	state = ECMA48_STATE_NORMAL;
	ecma48_escape_args_init();
}

void ecma48_add_char(UChar c){

  if(writing_buffer == BUFFER_NORMAL){
    // check if we are being asked to write beyond the screen
    // if so, the program is not handling wrapping, so we wrap
  	if(buf.col == cols) {
			if(autowrap){ // wrap
				buf_increment_line();
				buf.col = 0;
			} else { // no autowrap means no wrapping
				// overwrite last char
				buf.col = cols -1;
			}
		}
    struct screenchar *sc = &(buf.text[buf.line][buf.col++]);

    /* free old char */
    buf_free_char(sc);
    /* write new one */
    sc->c = c;
    sc->style = current_style;
    /* cache for REP */
    last_char = c;
  } /* else { BUFFER_OSC, etc. -> ignore for now } */
}

int ecma48_RETURN(UChar* tbuf){
  int num_chars = 1;
  tbuf[0] = 015;
  return num_chars;
}

int ecma48_KEYPAD(UChar* tbuf, char code){
  int num_chars = 3;
  tbuf[0] = 033;
  tbuf[1] = 0117;
  tbuf[2] = 0100 + code;
  return num_chars;
}

int ecma48_KEYPAD_ENTER(UChar* tbuf){
  int num_chars;
  num_chars = ecma48_RETURN(tbuf);
  return num_chars;
}


int ecma48_CSI_KEY(UChar* tbuf, char code){

  int num_chars = 3;
  tbuf[0] = 033;
  tbuf[1] = 0133;
  tbuf[2] = code;
  return num_chars;
}

int ecma48_ESC_O_KEY(UChar* tbuf, char code){
  int num_chars = 3;
    tbuf[0] = 033;
    tbuf[1] = 0117;
    tbuf[2] = code;
    return num_chars;
}


int ecma48_parse_control_codes(int sym, int mod, UChar* tbuf){

  // by default, we just put the symbol in the buffer
  int num_chars = 1;
  tbuf[0] = (UChar)sym;

  // but if one of the modifier keys is down, we maybe do something different
  // Start by checking arrow keys and stuff
  switch (sym){
  	case KEYCODE_BACKSPACE:		tbuf[0] = 010; break;
  	case KEYCODE_TAB:					tbuf[0] = 011; break;
  	case KEYCODE_ESCAPE:			tbuf[0] = 033; break;
    case KEYCODE_UP:          return ecma48_CSI_KEY(tbuf, 0101); break;
    case KEYCODE_DOWN:        return ecma48_CSI_KEY(tbuf, 0102); break;
    case KEYCODE_RIGHT:       return ecma48_CSI_KEY(tbuf, 0103); break;
    case KEYCODE_LEFT:        return ecma48_CSI_KEY(tbuf, 0104); break;
    case KEYCODE_RETURN:      return ecma48_RETURN(tbuf); break;
    //case KEYCODE_KP_PLUS    :
    //case KEYCODE_KP_MINUS   : return ecma48_KEYPAD(tbuf, 055); break;
    //case KEYCODE_KP_MULTIPLY:
    //case KEYCODE_KP_DIVIDE  :
    case KEYCODE_KP_ENTER   : return ecma48_KEYPAD_ENTER(tbuf); break;
//    case KEYCODE_KP_HOME    : return ecma48_KEYPAD(tbuf, 067); break;
//    case KEYCODE_KP_UP      : return ecma48_KEYPAD(tbuf, 070); break;
//    case KEYCODE_KP_PG_UP   : return ecma48_KEYPAD(tbuf, 071); break;
//    case KEYCODE_KP_LEFT    : return ecma48_KEYPAD(tbuf, 064); break;
//    case KEYCODE_KP_FIVE    : return ecma48_KEYPAD(tbuf, 065); break;
//    case KEYCODE_KP_RIGHT   : return ecma48_KEYPAD(tbuf, 066); break;
//    case KEYCODE_KP_END     : return ecma48_KEYPAD(tbuf, 061); break;
//    case KEYCODE_KP_DOWN    : return ecma48_KEYPAD(tbuf, 062); break;
//    case KEYCODE_KP_PG_DOWN : return ecma48_KEYPAD(tbuf, 063); break;
//    case KEYCODE_KP_INSERT  : return ecma48_KEYPAD(tbuf, 060); break;
//    case KEYCODE_KP_DELETE  : return ecma48_KEYPAD(tbuf, 056); break;
    case KEYCODE_DELETE     : return ecma48_CSI_KEY(tbuf, 0120); break;
    case KEYCODE_INSERT     : return ecma48_CSI_KEY(tbuf, 0100); break;
    case KEYCODE_HOME       : return ecma48_CSI_KEY(tbuf, 0110); break;
    case KEYCODE_PG_UP      : return ecma48_CSI_KEY(tbuf, 0126); break;
    case KEYCODE_PG_DOWN    : return ecma48_CSI_KEY(tbuf, 0125); break;
    case KEYCODE_BACK_TAB   : return ecma48_CSI_KEY(tbuf, 0132); break;
    case KEYCODE_F1         : return ecma48_ESC_O_KEY(tbuf, 0120); break;
    case KEYCODE_F2         : return ecma48_ESC_O_KEY(tbuf, 0121); break;
    case KEYCODE_F3         : return ecma48_ESC_O_KEY(tbuf, 0122); break;
    case KEYCODE_F4         : return ecma48_ESC_O_KEY(tbuf, 0123); break;
    case KEYCODE_F5         : return ecma48_ESC_O_KEY(tbuf, 0124); break;
    case KEYCODE_F6         : return ecma48_ESC_O_KEY(tbuf, 0125); break;
    case KEYCODE_F7         : return ecma48_ESC_O_KEY(tbuf, 0126); break;
    case KEYCODE_F8         : return ecma48_ESC_O_KEY(tbuf, 0127); break;
    case KEYCODE_F9         : return ecma48_ESC_O_KEY(tbuf, 0130); break;
    case KEYCODE_F10        : return ecma48_ESC_O_KEY(tbuf, 0131); break;
    case KEYCODE_F11        : return ecma48_ESC_O_KEY(tbuf, 0132); break;
    case KEYCODE_F12        : return ecma48_ESC_O_KEY(tbuf, 0101); break;

  }

  /* handle the shift key being down */
	if((mod & KEYMOD_SHIFT) || (mod & KEYMOD_SHIFT_LOCK) || (mod & KEYMOD_CAPS_LOCK)){
		switch(sym){
			case KEYCODE_A: tbuf[0] = 0101; break;
			case KEYCODE_B: tbuf[0] = 0102; break;
			case KEYCODE_C: tbuf[0] = 0103; break;
			case KEYCODE_D: tbuf[0] = 0104; break;
			case KEYCODE_E: tbuf[0] = 0105; break;
			case KEYCODE_F: tbuf[0] = 0106; break;
			case KEYCODE_G: tbuf[0] = 0107; break;
			case KEYCODE_H: tbuf[0] = 0110; break;
			case KEYCODE_I: tbuf[0] = 0111; break;
			case KEYCODE_J: tbuf[0] = 0112; break;
			case KEYCODE_K: tbuf[0] = 0113; break;
			case KEYCODE_L: tbuf[0] = 0114; break;
			case KEYCODE_M: tbuf[0] = 0115; break;
			case KEYCODE_N: tbuf[0] = 0116; break;
			case KEYCODE_O: tbuf[0] = 0117; break;
			case KEYCODE_P: tbuf[0] = 0120; break;
			case KEYCODE_Q: tbuf[0] = 0121; break;
			case KEYCODE_R: tbuf[0] = 0122; break;
			case KEYCODE_S: tbuf[0] = 0123; break;
			case KEYCODE_T: tbuf[0] = 0124; break;
			case KEYCODE_U: tbuf[0] = 0125; break;
			case KEYCODE_V: tbuf[0] = 0126; break;
			case KEYCODE_W: tbuf[0] = 0127; break;
			case KEYCODE_X: tbuf[0] = 0130; break;
			case KEYCODE_Y: tbuf[0] = 0131; break;
			case KEYCODE_Z: tbuf[0] = 0132; break;
		}
		num_chars = 1;
	}

  // now look for control down
  if(mod & KEYMOD_CTRL){
    switch (sym) {
      case KEYCODE_SPACE: tbuf[0] = 000; break;
      case KEYCODE_A: tbuf[0] = 001; break;
      case KEYCODE_B: tbuf[0] = 002; break;
      case KEYCODE_C: tbuf[0] = 003; break;
      case KEYCODE_D: tbuf[0] = 004; break;
      case KEYCODE_E: tbuf[0] = 005; break;
      case KEYCODE_F: tbuf[0] = 006; break;
      case KEYCODE_G: tbuf[0] = 007; break;
      case KEYCODE_H: tbuf[0] = 010; break;
      case KEYCODE_I: tbuf[0] = 011; break;
      case KEYCODE_J: tbuf[0] = 012; break;
      case KEYCODE_K: tbuf[0] = 013; break;
      case KEYCODE_L: tbuf[0] = 014; break;
      case KEYCODE_M: tbuf[0] = 015; break;
      case KEYCODE_N: tbuf[0] = 016; break;
      case KEYCODE_O: tbuf[0] = 017; break;
      case KEYCODE_P: tbuf[0] = 020; break;
      case KEYCODE_Q: tbuf[0] = 021; break;
      case KEYCODE_R: tbuf[0] = 022; break;
      case KEYCODE_S: tbuf[0] = 023; break;
      case KEYCODE_T: tbuf[0] = 024; break;
      case KEYCODE_U: tbuf[0] = 025; break;
      case KEYCODE_V: tbuf[0] = 026; break;
      case KEYCODE_W: tbuf[0] = 027; break;
      case KEYCODE_X: tbuf[0] = 030; break;
      case KEYCODE_Y: tbuf[0] = 031; break;
      case KEYCODE_Z: tbuf[0] = 032; break;
      case KEYCODE_LEFT_BRACKET: tbuf[0] = 033; break;
      case KEYCODE_BACK_SLASH: tbuf[0] = 034; break;
      case KEYCODE_RIGHT_BRACKET: tbuf[0] = 035; break;
      case KEYCODE_GRAVE: if(mod & KEYMOD_SHIFT){tbuf[0] = 036;} break;
      case KEYCODE_SLASH:  if(mod & KEYMOD_SHIFT){tbuf[0] = 037;} break;
    }
    num_chars = 1;
  }
  return num_chars;
}

/* PRINT_CONTROL_SEQUENCE
 * prints the control sequence and escape arg buffers
 */
void ecma48_PRINT_CONTROL_SEQUENCE(char* terminator){
  PRINT(stderr, "Control Sequence: ");
  switch (state){
    case ECMA48_STATE_C1: PRINT(stderr, "ESC "); break;
    case ECMA48_STATE_CSI: PRINT(stderr, "ESC [ "); break;
    case ECMA48_STATE_ANSI: PRINT(stderr, "ESC [ ? "); break;
    case ECMA48_STATE_ANSI_POUND: PRINT(stderr, "ESC # "); break;
  }

  	PRINT(stderr, "%s -- args: %s;%s;%s;%s;%s;%s;%s;%s;%s;%s (state=%d)\n",
  									terminator,
                    escape_args.args[0],
                    escape_args.args[1],
                    escape_args.args[2],
                    escape_args.args[3],
                    escape_args.args[4],
                    escape_args.args[5],
                    escape_args.args[6],
                    escape_args.args[7],
                    escape_args.args[8],
                    escape_args.args[9],
                    state
                    );
}

/* NOT_IMPLEMENTED
We use this function to indicate a control sequence we
do not or have not implemented
*/
void ecma48_NOT_IMPLEMENTED(char* function){
  fprintf(stderr, "NOT IMPLEMENTED: ");
  switch (state){
    case ECMA48_STATE_C1: fprintf(stderr, "ESC "); break;
    case ECMA48_STATE_CSI: fprintf(stderr, "ESC [ "); break;
    case ECMA48_STATE_ANSI: fprintf(stderr, "ESC [ ? "); break;
    case ECMA48_STATE_ANSI_POUND: fprintf(stderr, "ESC # "); break;
  }

  fprintf(stderr, "%s -- args: %s;%s;%s;%s;%s;%s;%s;%s;%s;%s (state=%d)\n",
                    function,
                    escape_args.args[0],
                    escape_args.args[1],
                    escape_args.args[2],
                    escape_args.args[3],
                    escape_args.args[4],
                    escape_args.args[5],
                    escape_args.args[6],
                    escape_args.args[7],
                    escape_args.args[8],
                    escape_args.args[9],
                    state
                    );
  ecma48_end_control();
}

void ecma48_reverse_video(){
  current_style.fg_color = default_text_style.bg_color;
  current_style.bg_color = default_text_style.fg_color;
}

void ecma48_normal_video(){
  current_style.fg_color = default_text_style.fg_color;
  current_style.bg_color = default_text_style.bg_color;
}

/* NUL - NULL
Notation: (C0)
Representation: 00/00
NUL is used for media-fill or time-fill. NUL characters may be inserted into, or
removed from, a data stream without affecting the information content of that
stream, but such action may affect the information layout and/or the control of
equipment.
*/
void ecma48_NUL(){
  ecma48_PRINT_CONTROL_SEQUENCE("NUL");
  ecma48_end_control();
}

/*
SOH - START OF HEADING
Notation: (C0)
Representation: 00/01
SOH is used to indicate the beginning of a heading.
The use of SOH is defined in ISO 1745.
*/
void ecma48_SOH(){
  ecma48_NOT_IMPLEMENTED("SOH");
}

/*
STX - START OF TEXT
Notation: (C0)
Representation: 00/02
STX is used to indicate the beginning of a text and the end of a heading.
The use of STX is defined in ISO 1745.
*/
void ecma48_STX(){
  ecma48_NOT_IMPLEMENTED("STX");
}

/*
ETX - END OF TEXT
Notation: (C0)
Representation: 00/03
ETX is used to indicate the end of a text.
The use of ETX is defined in ISO 1745.
*/
void ecma48_ETX(){
  ecma48_PRINT_CONTROL_SEQUENCE("ETX");
  ecma48_end_control();
}

/*
EOT - END OF TRANSMISSION
Notation: (C0)
Representation: 00/04
EOT is used to indicate the conclusion of the transmission of one or more texts.
The use of EOT is defined in ISO 1745.
*/
void ecma48_EOT(){
  ecma48_PRINT_CONTROL_SEQUENCE("EOT");
  ecma48_end_control();
}

/*
ENQ - ENQUIRY
Notation: (C0)
Representation: 00/05
ENQ is transmitted by a sender as a request for a response from a receiver.
The use of ENQ is defined in ISO 1745.
*/
void ecma48_ENQ(){
  ecma48_NOT_IMPLEMENTED("ENQ");
}

/*
ACK - ACKNOWLEDGE
Notation: (C0)
Representation: 00/06
ACK is transmitted by a receiver as an affirmative response to the sender.
The use of ACK is defined in ISO 1745.
*/
void ecma48_ACK(){
  ecma48_NOT_IMPLEMENTED("ACK");
}

/*
BEL - BELL
Notation: (C0)
Representation: 00/07
BEL is used when there is a need to call for attention; it may control alarm or
attention devices.
*/
void ecma48_BEL(){
  ecma48_PRINT_CONTROL_SEQUENCE("BEL");
  if(writing_buffer != BUFFER_NORMAL){
    /* BEL is used as ST sometimes... */
    ecma48_ST();
  } else {
    /* expected use */
    flash = 1;
  }
  ecma48_end_control();
}

/*
BS - BACKSPACE
Notation: (C0)
Representation: 00/08
BS causes the active data position to be moved one character position in the
data component in the direction opposite to that of the implicit movement.
The direction of the implicit movement depends on the parameter value of SELECT
IMPLICIT MOVEMENT DIRECTION (SIMD).
*/
void ecma48_BS_INTER(){
  ecma48_PRINT_CONTROL_SEQUENCE("BS");
  if(buf.col == cols){
    // backup twice, as per vt100 compat
    buf.col--;
  }
  buf.col--;
  if (buf.col < 0) {
    buf.col = 0;
  }
}
void ecma48_BS(){
  ecma48_BS_INTER();
  ecma48_end_control();
}

/*
HT - CHARACTER TABULATION
Notation: (C0)
Representation: 00/09
HT causes the active presentation position to be moved to the following
character tabulation stop in the presentation component.
In addition, if that following character tabulation stop has been set by
TABULATION ALIGN CENTRE (TAC), TABULATION ALIGN LEADING EDGE (TALE), TABULATION
ALIGN TRAILING EDGE (TATE) or TABULATION CENTRED ON CHARACTER (TCC), HT
indicates the beginning of a string of text which is to be positioned within a
line according to the properties of that tabulation stop. The end of the string
is indicated by the next occurrence of HT or CARRIAGE RETURN (CR) or NEXT LINE
(NEL) in the data stream.
*/
void ecma48_HT_INTER(){
  ecma48_PRINT_CONTROL_SEQUENCE("HT");
  int x = screen_next_tab_x();
  buf.col = screen_to_buf_col(x);
}
void ecma48_HT(){
  ecma48_HT_INTER();
  ecma48_end_control();
}

/*
LF - LINE FEED
Notation: (C0)
Representation: 00/10
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to PRESENTATION, LF causes the
active presentation position to be moved to the corresponding character position
of the following line in the presentation component.
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to DATA, LF causes the active
data position to be moved to the corresponding character position of the
following line in the data component.
*/
void ecma48_LF_INTER(){
  ecma48_PRINT_CONTROL_SEQUENCE("LF");

  if(buf.col < cols){
    // emulate xenl/xn newline glitch at the right margin
    buf_increment_line();
  }
}
void ecma48_LF(){
  ecma48_LF_INTER();
  ecma48_end_control();
}

/*
VT - LINE TABULATION
Notation: (C0)
Representation: 00/11
VT causes the active presentation position to be moved in the presentation
component to the corresponding character position on the line at which the
following line tabulation stop is set.
*/
void ecma48_VT_INTER(){
  ecma48_PRINT_CONTROL_SEQUENCE("LF");
  int row = buf_to_screen_row(-1);
  int next_vtab = buf_next_vtab(row);
  if(next_vtab > 0){
    buf.line = screen_to_buf_row(next_vtab);
  }
}
void ecma48_VT(){
  ecma48_VT_INTER();
  ecma48_end_control();
}

/*
FF - FORM FEED
Notation: (C0)
Representation: 00/12
FF causes the active presentation position to be moved to the corresponding
character position of the line at the page home position of the next form or
page in the presentation component. The page home position is established by the
parameter value of SET PAGE HOME (SPH).
*/
void ecma48_FF(){
  ecma48_NOT_IMPLEMENTED("FF");
}

/*
CR - CARRIAGE RETURN
Notation: (C0)
Representation: 00/13
The effect of CR depends on the setting of the DEVICE COMPONENT SELECT MODE
(DCSM) and on the parameter value of SELECT IMPLICIT MOVEMENT DIRECTION (SIMD).
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to PRESENTATION and with the
parameter value of SIMD equal to 0, CR causes the active presentation position
to be moved to the line home position of the same line in the presentation
component. The line home position is established by the parameter value of SET
LINE HOME (SLH).
With a parameter value of SIMD equal to 1, CR causes the active presentation
position to be moved to the line limit position of the same line in the
presentation component. The line limit position is established by the parameter
value of SET LINE LIMIT (SLL).
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to DATA and with a parameter
value of SIMD equal to 0, CR causes the active data position to be moved to the
line home position of the same line in the data component. The line home
position is established by the parameter value of SET LINE HOME (SLH).
With a parameter value of SIMD equal to 1, CR causes the active data position to
be moved to the line limit position of the same line in the data component. The
line limit position is established by the parameter value of SET LINE LIMIT
(SLL).
*/
void ecma48_CR_INTER(){
  ecma48_PRINT_CONTROL_SEQUENCE("CR");
  buf.col = 0;
}
void ecma48_CR(){
  ecma48_CR_INTER();
  ecma48_end_control();
}

/*
SO - SHIFT-OUT
Notation: (C0)
Representation: 00/14
SO is used for code extension purposes. It causes the meanings of the bit
combinations following it in the data stream to be changed.
The use of SO is defined in Standard ECMA-35.
NOTE SO is used in 7-bit environments only; in 8-bit environments LOCKING-SHIFT
ONE (LS1) is used instead.
*/
void ecma48_SO(){
  ecma48_NOT_IMPLEMENTED("SO");
}

/*
SI - SHIFT-IN
Notation: (C0)
Representation: 00/15
SI is used for code extension purposes. It causes the meanings of the bit
combinations following it in the data stream to be changed.
The use of SI is defined in Standard ECMA-35.
NOTE SI is used in 7-bit environments only; in 8-bit environments LOCKING-SHIFT
ZERO (LS0) is used instead.
*/
void ecma48_SI(){
  ecma48_NOT_IMPLEMENTED("SI");
}

/*
DLE - DATA LINK ESCAPE
Notation: (C0)
Representation: 01/00
DLE is used exclusively to provide supplementary transmission control functions.
The use of DLE is defined in ISO 1745.
*/
void ecma48_DLE(){
  ecma48_NOT_IMPLEMENTED("DLE");
}

/*
DC1 - DEVICE CONTROL ONE
Notation: (C0)
Representation: 01/01
DC1 is primarily intended for turning on or starting an ancillary device. If it
is not required for this purpose, it may be used to restore a device to the
basic mode of operation (see also DC2 and DC3), or any other device control
function not provided by other DCs.
NOTE When used for data flow control, DC1 is sometimes called "X-ON".
*/
void ecma48_DC1(){
  ecma48_NOT_IMPLEMENTED("DC1");
}

/*
DC2 - DEVICE CONTROL TWO
Notation: (C0)
Representation: 01/02
DC2 is primarily intended for turning on or starting an ancillary device. If it
is not required for this purpose, it may be used to set a device to a special
mode of operation (in which case DC1 is used to restore the device to the basic
mode), or for any other device control function not provided by other DCs.
*/
void ecma48_DC2(){
  ecma48_NOT_IMPLEMENTED("DC2");
}

/*
DC3 - DEVICE CONTROL THREE
Notation: (C0)
Representation: 01/03
DC3 is primarily intended for turning off or stopping an ancillary device. This
function may be a secondary level stop, for example wait, pause, stand-by or
halt (in which case DC1 is used to restore normal operation). If it is not
required for this purpose, it may be used for any other device control function
not provided by other DCs.
NOTE When used for data flow control, DC3 is sometimes called "X-OFF".
*/
void ecma48_DC3(){
  ecma48_NOT_IMPLEMENTED("DC3");
}

/*
DC4 - DEVICE CONTROL FOUR
Notation: (C0)
Representation: 01/04
DC4 is primarily intended for turning off, stopping or interrupting an ancillary
device. If it is not required for this purpose, it may be used for any other
device control function not provided by other DCs.
*/
void ecma48_DC4(){
  ecma48_NOT_IMPLEMENTED("DC4");
}

/*
NAK - NEGATIVE ACKNOWLEDGE
Notation: (C0)
Representation: 01/05
NAK is transmitted by a receiver as a negative response to the sender.
The use of NAK is defined in ISO 1745.
*/
void ecma48_NAK(){
  ecma48_NOT_IMPLEMENTED("NAK");
}

/*
SYN - SYNCHRONOUS IDLE
Notation: (C0)
Representation: 01/06
SYN is used by a synchronous transmission system in the absence of any other
character (idle condition) to provide a signal from which synchronism may be
achieved or retained between data terminal equipment.
The use of SYN is defined in ISO 1745.
*/
void ecma48_SYN(){
  ecma48_NOT_IMPLEMENTED("SYN");
}

/*
ETB - END OF TRANSMISSION BLOCK
Notation: (C0)
Representation: 01/07
ETB is used to indicate the end of a block of data where the data are divided
into such blocks for transmission purposes.
The use of ETB is defined in ISO 1745.
*/
void ecma48_ETB(){
  ecma48_NOT_IMPLEMENTED("ETB");
}

/*
CAN - CANCEL
Notation: (C0)
Representation: 01/08
CAN is used to indicate that the data preceding it in the data stream is in
error. As a result, this data shall be ignored. The specific meaning of this
control function shall be defined for each application and/or between sender and
recipient.
*/
void ecma48_CAN(){
  ecma48_PRINT_CONTROL_SEQUENCE("CAN");
  ecma48_end_control();
}

/*
EM - END OF MEDIUM
Notation: (C0)
Representation: 01/09
EM is used to identify the physical end of a medium, or the end of the used
portion of a medium, or the end of the wanted portion of data recorded on a
medium.
*/
void ecma48_EM(){
  ecma48_NOT_IMPLEMENTED("EM");
}

/*
SUB - SUBSTITUTE
Notation: (C0)
Representation: 01/10
SUB is used in the place of a character that has been found to be invalid or in
error. SUB is intended to be introduced by automatic means.
*/
void ecma48_SUB(){
  ecma48_PRINT_CONTROL_SEQUENCE("SUB");
  ecma48_end_control();
}

/*
ESC - ESCAPE
Notation: (C0)
Representation: 01/11
ESC is used for code extension purposes. It causes the meanings of a limited
number of bit combinations following it in the data stream to be changed.
The use of ESC is defined in Standard ECMA-35.
*/
void ecma48_ESC(){
  ecma48_PRINT_CONTROL_SEQUENCE("ESC");
  //ecma48_escape_args_init();
  state = ECMA48_STATE_C1;
}

/*
IS1 - INFORMATION SEPARATOR ONE (US - UNIT SEPARATOR)
Notation: (C0)
Representation: 01/15
IS1 is used to separate and qualify data logically; its specific meaning has to
be defined for each application. If this control function is used in
hierarchical order, it may delimit a data item called a unit, see 8.2.10.
*/
void ecma48_IS1(){
  ecma48_NOT_IMPLEMENTED("IS1");
}

/*
IS2 - INFORMATION SEPARATOR TWO (RS - RECORD SEPARATOR)
Notation: (C0)
Representation: 01/14
IS2 is used to separate and qualify data logically; its specific meaning has to
be defined for each application. If this control function is used in
hierarchical order, it may delimit a data item called a record, see 8.2.10.
*/
void ecma48_IS2(){
  ecma48_NOT_IMPLEMENTED("IS2");
}

/*
IS3 - INFORMATION SEPARATOR THREE (GS - GROUP SEPARATOR)
Notation: (C0)
Representation: 01/13
IS3 is used to separate and qualify data logically; its specific meaning has to
be defined for each application. If this control function is used in
hierarchical order, it may delimit a data item called a group, see 8.2.10.
*/
void ecma48_IS3(){
  ecma48_NOT_IMPLEMENTED("IS3");
}

/*
IS4 - INFORMATION SEPARATOR FOUR (FS - FILE SEPARATOR)
Notation: (C0)
Representation: 01/12
IS4 is used to separate and qualify data logically; its specific meaning has to
be defined for each application. If this control function is used in
hierarchical order, it may delimit a data item called a file, see 8.2.10.
*/
void ecma48_IS4(){
  ecma48_NOT_IMPLEMENTED("IS4");
}

/*
BPH - BREAK PERMITTED HERE
Notation: (C1)
Representation: 08/02 or ESC 04/02
BPH is used to indicate a point where a line break may occur when text is
formatted. BPH may occur between two graphic characters, either or both of which
may be SPACE.
*/
void ecma48_BPH(){
  ecma48_NOT_IMPLEMENTED("BPH");
}

/*
NBH - NO BREAK HERE
Notation: (C1)
Representation: 08/03 or ESC 04/03
NBH is used to indicate a point where a line break shall not occur when text is
formatted. NBH may occur between two graphic characters either or both of which
may be SPACE.
*/
void ecma48_NBH(){
  ecma48_NOT_IMPLEMENTED("BPH");
}

/*
NEL - NEXT LINE
Notation: (C1)
Representation: 08/05 or ESC 04/05
The effect of NEL depends on the setting of the DEVICE COMPONENT SELECT MODE
(DCSM) and on the parameter value of SELECT IMPLICIT MOVEMENT DIRECTION (SIMD).
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to PRESENTATION and with a
parameter value of SIMD equal to 0, NEL causes the active presentation position
to be moved to the line home position of the following line in the presentation
component. The line home position is established by the parameter value of SET
LINE HOME (SLH).
With a parameter value of SIMD equal to 1, NEL causes the active presentation
position to be moved to the line limit position of the following line in the
presentation component. The line limit position is established by the parameter
value of SET LINE LIMIT (SLL).
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to DATA and with a parameter
value of SIMD equal to 0, NEL causes the active data position to be moved to the
line home position of the following line in the data component. The line home
position is established by the parameter value of SET LINE HOME (SLH).
With a parameter value of SIMD equal to 1, NEL causes the active data position
to be moved to the line limit position of the following line in the data
component. The line limit position is established by the parameter value of SET
LINE LIMIT (SLL).
*/
void ecma48_NEL(){
  ecma48_PRINT_CONTROL_SEQUENCE("NEL");
  buf.col = 0;
  buf_increment_line();
  ecma48_end_control();
}

/*
SSA - START OF SELECTED AREA
Notation: (C1)
Representation: 08/06 or ESC 04/06
SSA is used to indicate that the active presentation position is the first of a
string of character positions in the presentation component, the contents of
which are eligible to be transmitted in the form of a data stream or transferred
to an auxiliary input/output device.
The end of this string is indicated by END OF SELECTED AREA (ESA). The string of
characters actually transmitted or transferred depends on the setting of the
GUARDED AREA TRANSFER MODE (GATM) and on any guarded areas established by DEFINE
AREA QUALIFICATION (DAQ), or by START OF GUARDED AREA (SPA) and END OF GUARDED
AREA (EPA).
NOTE The control functions for area definition (DAQ, EPA, ESA, SPA, SSA) should
not be used within an SRS string or an SDS string.
*/
void ecma48_SSA(){
  ecma48_NOT_IMPLEMENTED("SSA");
}

/*
ESA - END OF SELECTED AREA
Notation: (C1)
Representation: 08/07 or ESC 04/07
ESA is used to indicate that the active presentation position is the last of a
string of character positions in the presentation component, the contents of
which are eligible to be transmitted in the form of a data stream or transferred
to an auxiliary input/output device. The beginning of this string is indicated
by START OF SELECTED AREA (SSA).
NOTE The control function for area definition (DAQ, EPA, ESA, SPA, SSA) should
not be used within an SRS string or an SDS string.
*/
void ecma48_ESA(){
  ecma48_NOT_IMPLEMENTED("SSA");
}

/*
HTS - CHARACTER TABULATION SET
Notation: (C1)
Representation: 08/08 or ESC 04/08
HTS causes a character tabulation stop to be set at the active presentation
position in the presentation component.
The number of lines affected depends on the setting of the TABULATION STOP MODE
(TSM).
*/
void ecma48_HTS(){
  ecma48_PRINT_CONTROL_SEQUENCE("HTS");
  int row = buf_to_screen_row(-1);
  int col = buf_to_screen_col(-1);
  tabs[row][col] = 1;
  ecma48_end_control();
}

/*
HTJ - CHARACTER TABULATION WITH JUSTIFICATION
Notation: (C1)
Representation: 08/09 or ESC 04/09
HTJ causes the contents of the active field (the field in the presentation
component that contains the active presentation position) to be shifted forward
so that it ends at the character position preceding the following character
tabulation stop. The active presentation position is moved to that following
character tabulation stop. The character positions which precede the beginning
of the shifted string are put into the erased state.
*/
void ecma48_HTJ(){
  ecma48_NOT_IMPLEMENTED("HTJ");
}

/*
VTS - LINE TABULATION SET
Notation: (C1)
Representation: 08/10 or ESC 04/10
VTS causes a line tabulation stop to be set at the active line (the line that
contains the active presentation position).
*/
void ecma48_VTS(){
  ecma48_NOT_IMPLEMENTED("VTS");
}

/*
PLD - PARTIAL LINE FORWARD
Notation: (C1)
Representation: 08/11 or ESC 04/11
PLD causes the active presentation position to be moved in the presentation
component to the corresponding position of an imaginary line with a partial
offset in the direction of the line progression. This offset should be
sufficient either to image following characters as subscripts until the first
following occurrence of PARTIAL LINE BACKWARD (PLU) in the data stream, or, if
preceding characters were imaged as superscripts, to restore imaging of
following characters to the active line (the line that contains the active
presentation position).
Any interactions between PLD and format effectors other than PLU are not defined
by this Standard.
*/
void ecma48_PLD(){
  ecma48_NOT_IMPLEMENTED("PLD");
}

/*
PLU - PARTIAL LINE BACKWARD
Notation: (C1)
Representation: 08/12 or ESC 04/12
PLU causes the active presentation position to be moved in the presentation
component to the corresponding position of an imaginary line with a partial
offset in the direction opposite to that of the line progression. This offset
should be sufficient either to image following characters as superscripts until
the first following occurrence of PARTIAL LINE FORWARD (PLD) in the data stream,
or, if preceding characters were imaged as subscripts, to restore imaging of
following characters to the active line (the line that contains the active
presentation position).
Any interactions between PLU and format effectors other than PLD are not defined
by this Standard.
*/
void ecma48_PLU(){
  ecma48_NOT_IMPLEMENTED("PLU");
}

/*
RI - REVERSE LINE FEED
Notation: (C1)
Representation: 08/13 or ESC 04/13
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to PRESENTATION, RI causes the
active presentation position to be moved in the presentation component to the
corresponding character position of the preceding line.
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to DATA, RI causes the active
data position to be moved in the data component to the corresponding character
position of the preceding line.
*/
void ecma48_RI(){
  ecma48_PRINT_CONTROL_SEQUENCE("RI");
  buf_decrement_line();
  ecma48_end_control();
}

/*
SS2 - SINGLE-SHIFT TWO
Notation: (C1)
Representation: 08/14 or ESC 04/14
SS2 is used for code extension purposes. It causes the meanings of the bit
combinations following it in the data stream to be changed.
The use of SS2 is defined in Standard ECMA-35.
*/
void ecma48_SS2(){
  ecma48_NOT_IMPLEMENTED("SS2");
}

/*
SS3 - SINGLE-SHIFT THREE
Notation: (C1)
Representation: 08/15 or ESC 04/15
SS3 is used for code extension purposes. It causes the meanings of the bit
combinations following it in the data stream to be changed.
The use of SS3 is defined in Standard ECMA-35.
*/
void ecma48_SS3(){
  ecma48_NOT_IMPLEMENTED("SS3");
}

/*
DCS - DEVICE CONTROL STRING
Notation: (C1)
Representation: 09/00 or ESC 05/00
DCS is used as the opening delimiter of a control string for device control use.
The command string following may consist of bit combinations in the range 00/08
to 00/13 and 02/00 to 07/14. The control string is closed by the terminating
delimiter STRING TERMINATOR (ST).
The command string represents either one or more commands for the receiving
device, or one or more status reports from the sending device. The purpose and
the format of the command string are specified by the most recent occurrence of
IDENTIFY DEVICE CONTROL STRING (IDCS), if any, or depend on the sending and/or
the receiving device.
*/
void ecma48_DCS(){
  ecma48_NOT_IMPLEMENTED("DCS");
}

/*
PU1 - PRIVATE USE ONE
Notation: (C1)
Representation: 09/01 or ESC 05/01
PU1 is reserved for a function without standardized meaning for private use as
required, subject to the prior agreement between the sender and the recipient of
the data.
*/
void ecma48_PU1(){
  ecma48_NOT_IMPLEMENTED("PU1");
}

/*
PU2 - PRIVATE USE TWO
Notation: (C1)
Representation: 09/02 or ESC 05/02
PU2 is reserved for a function without standardized meaning for private use as
required, subject to the prior agreement between the sender and the recipient of
the data.
*/
void ecma48_PU2(){
  ecma48_NOT_IMPLEMENTED("PU2");
}

/*
STS - SET TRANSMIT STATE
Notation: (C1)
Representation: 09/03 or ESC 05/03
STS is used to establish the transmit state in the receiving device. In this
state the transmission of data from the device is possible.
The actual initiation of transmission of data is performed by a data
communication or input/output interface control procedure which is outside the
scope of this Standard.
The transmit state is established either by STS appearing in the received data
stream or by the operation of an appropriate key on a keyboard.
*/
void ecma48_STS(){
  ecma48_NOT_IMPLEMENTED("STS");
}

/*
CCH - CANCEL CHARACTER
Notation: (C1)
Representation: 09/04 or ESC 05/04
CCH is used to indicate that both the preceding graphic character in the data
stream, (represented by one or more bit combinations) including SPACE, and the
control function CCH itself are to be ignored for further interpretation of the
data stream.
If the character preceding CCH in the data stream is a control function
(represented by one or more bit combinations), the effect of CCH is not defined
by this Standard.
*/
void ecma48_CCH(){
  ecma48_NOT_IMPLEMENTED("CCH");
}

/*
MW - MESSAGE WAITING
Notation: (C1)
Representation: 09/05 or ESC 05/05
MW is used to set a message waiting indicator in the receiving device. An
appropriate acknowledgement to the receipt of MW may be given by using DEVICE
STATUS REPORT (DSR).
*/
void ecma48_MW(){
  ecma48_NOT_IMPLEMENTED("MW");
}

/*
SPA - START OF GUARDED AREA
Notation: (C1)
Representation: 09/06 or ESC 05/06
SPA is used to indicate that the active presentation position is the first of a
string of character positions in the presentation component, the contents of
which are protected against manual alteration, are guarded against transmission
or transfer, depending on the setting of the GUARDED AREA TRANSFER MODE (GATM)
and may be protected against erasure, depending on the setting of the ERASURE
MODE (ERM). The end of this string is indicated by END OF GUARDED AREA (EPA).
NOTE The control functions for area definition (DAQ, EPA, ESA, SPA, SSA) should
not be used within an SRS string or an SDS string.
*/
void ecma48_SPA(){
  ecma48_NOT_IMPLEMENTED("SPA");
}

/*
EPA - END OF GUARDED AREA
Notation: (C1)
Representation: 09/07 or ESC 05/07
EPA is used to indicate that the active presentation position is the last of a
string of character positions in the presentation component, the contents of
which are protected against manual alteration, are guarded against transmission
or transfer, depending on the setting of the GUARDED AREA TRANSFER MODE (GATM),
and may be protected against erasure, depending on the setting of the ERASURE
MODE (ERM). The beginning of this string is indicated by START OF GUARDED AREA
(SPA).
NOTE The control functions for area definition (DAQ, EPA, ESA, SPA, SSA) should
not be used within an SRS string or an SDS string.
*/
void ecma48_EPA(){
  ecma48_NOT_IMPLEMENTED("SPA");
}

/*
SOS - START OF STRING
Notation: (C1)
Representation: 09/08 or ESC 05/08
SOS is used as the opening delimiter of a control string. The character string
following may consist of any bit combination, except those representing SOS or
STRING TERMINATOR (ST). The control string is closed by the terminating
delimiter STRING TERMINATOR (ST). The interpretation of the character string
depends on the application.
*/
void ecma48_SOS(){
  ecma48_NOT_IMPLEMENTED("SOS");
}

/*
SCI - SINGLE CHARACTER INTRODUCER
Notation: (C1)
Representation: 09/10 or ESC 05/10
SCI and the bit combination following it are used to represent a control
function or a graphic character. The bit combination following SCI must be from
00/08 to 00/13 or 02/00 to 07/14. The use of SCI is reserved for future
standardization.
*/
void ecma48_SCI(){
  ecma48_NOT_IMPLEMENTED("SCI");
}

/*
CSI - CONTROL SEQUENCE INTRODUCER
Notation: (C1)
Representation: 09/11 or ESC 05/11
CSI is used as the first character of a control sequence, see 5.4.
*/
void ecma48_CSI(){
  state = ECMA48_STATE_CSI;
}

/*
ST - STRING TERMINATOR
Notation: (C1)
Representation: 09/12 or ESC 05/12
ST is used as the closing delimiter of a control string opened by APPLICATION
PROGRAM COMMAND (APC), DEVICE CONTROL STRING (DCS), OPERATING SYSTEM COMMAND
(OSC), PRIVACY MESSAGE (PM), or START OF STRING (SOS).
*/
void ecma48_ST(){
  ecma48_PRINT_CONTROL_SEQUENCE("ST");
  writing_buffer = BUFFER_NORMAL;
  ecma48_end_control();

}

/*
OSC - OPERATING SYSTEM COMMAND
Notation: (C1)
Representation: 09/13 or ESC 05/13
OSC is used as the opening delimiter of a control string for operating system
use. The command string following may consist of a sequence of bit combinations
in the range 00/08 to 00/13 and 02/00 to 07/14. The control string is closed by
the terminating delimiter STRING TERMINATOR (ST). The interpretation of the
command string depends on the relevant operating system.
*/
void ecma48_OSC(){
  ecma48_PRINT_CONTROL_SEQUENCE("OSC");
  writing_buffer = BUFFER_OSC;
  ecma48_end_control();
}

/*
PM - PRIVACY MESSAGE
Notation: (C1)
Representation: 09/14 or ESC 05/14
PM is used as the opening delimiter of a control string for privacy message use.
The command string following may consist of a sequence of bit combinations in
the range 00/08 to 00/13 and 02/00 to 07/14. The control string is closed by the
terminating delimiter STRING TERMINATOR (ST). The interpretation of the command
string depends on the relevant privacy discipline.
*/
void ecma48_PM(){
  ecma48_NOT_IMPLEMENTED("PM");
}

/*
APC - APPLICATION PROGRAM COMMAND
Notation: (C1)
Representation: 09/15 or ESC 05/15
APC is used as the opening delimiter of a control string for application program
use. The command string following may consist of bit combinations in the range
00/08 to 00/13 and 02/00 to 07/14. The control string is closed by the
terminating delimiter STRING TERMINATOR (ST). The interpretation of the command
string depends on the relevant application program.
*/
void ecma48_APC(){
  ecma48_NOT_IMPLEMENTED("APC");
}

/*
DMI - DISABLE MANUAL INPUT
Notation: (Fs)
Representation: ESC 06/00
DMI causes the manual input facilities of a device to be disabled.
*/
void ecma48_DMI(){
  ecma48_NOT_IMPLEMENTED("DMI");
}

/*
INT - INTERRUPT
Notation: (Fs)
Representation: ESC 06/01
INT is used to indicate to the receiving device that the current process is to
be interrupted and an agreed procedure is to be initiated. This control function
is applicable to either direction of transmission.
*/
void ecma48_INT(){
  ecma48_NOT_IMPLEMENTED("INT");
}

/*
EMI - ENABLE MANUAL INPUT
Notation: (Fs)
Representation: ESC 06/02
EMI is used to enable the manual input facilities of a device.
*/
void ecma48_EMI(){
  ecma48_NOT_IMPLEMENTED("EMI");
}

/*
RIS - RESET TO INITIAL STATE
Notation: (Fs)
Representation: ESC 06/03
RIS causes a device to be reset to its initial state, i.e. the state it has
after it is made operational. This may imply, if applicable: clear tabulation
stops, remove qualified areas, reset graphic rendition, put all character
positions into the erased state, move the active presentation position to the
first position of the first line in the presentation component, move the active
data position to the first character position of the first line in the data
component, set the modes into the reset state, etc.
*/
void ecma48_RIS(){
  ecma48_NOT_IMPLEMENTED("RIS");
}

/*
CMD - CODING METHOD DELIMITER
Notation: (Fs)
Representation: ESC 06/04
CMD is used as the delimiter of a string of data coded according to Standard
ECMA-35 and to switch to a general level of control.
The use of CMD is not mandatory if the higher level protocol defines means of
delimiting the string, for instance, by specifying the length of the string.
*/
void ecma48_CMD(){
  ecma48_NOT_IMPLEMENTED("CMD");
}

/*
LS2 - LOCKING-SHIFT TWO
Notation: (Fs)
Representation: ESC 06/14
LS2 is used for code extension purposes. It causes the meanings of the bit
combinations following it in the data stream to be changed.
The use of LS2 is defined in Standard ECMA-35.
*/
void ecma48_LS2(){
  ecma48_NOT_IMPLEMENTED("LS2");
}

/*
LS3 - LOCKING-SHIFT THREE
Notation: (Fs)
Representation: ESC 06/15
LS3 is used for code extension purposes. It causes the meanings of the bit
combinations following it in the data stream to be changed.
The use of LS3 is defined in Standard ECMA-35.
*/
void ecma48_LS3(){
  ecma48_NOT_IMPLEMENTED("LS3");
}

/*
LS3R - LOCKING-SHIFT THREE RIGHT
Notation: (Fs)
Representation: ESC 07/12
LS3R is used for code extension purposes. It causes the meanings of the bit
combinations following it in the data stream to be changed.
The use of LS3R is defined in Standard ECMA-35.
*/
void ecma48_LS3R(){
  ecma48_NOT_IMPLEMENTED("LS3R");
}

/*
LS2R - LOCKING-SHIFT TWO RIGHT
Notation: (Fs)
Representation: ESC 07/13
LS2R is used for code extension purposes. It causes the meanings of the bit
combinations following it in the data stream to be changed.
The use of LS2R is defined in Standard ECMA-35.
*/
void ecma48_LS2R(){
  ecma48_NOT_IMPLEMENTED("LS2R");
}

/*
LS1R - LOCKING-SHIFT ONE RIGHT
Notation: (Fs)
Representation: ESC 07/14
LS1R is used for code extension purposes. It causes the meanings of the bit
combinations following it in the data stream to be changed.
The use of LS1R is defined in Standard ECMA-35.
*/
void ecma48_LS1R(){
  ecma48_NOT_IMPLEMENTED("LS1R");
}

/*
ICH - INSERT CHARACTER
Notation: (Pn)
Representation: CSI Pn 04/00
Parameter default value: Pn = 1
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to PRESENTATION, ICH is used
to prepare the insertion of n characters, by putting into the erased state the
active presentation position and, depending on the setting of the CHARACTER
EDITING MODE (HEM), the n-1 preceding or following character positions in the
presentation component, where n equals the value of Pn. The previous contents of
the active presentation position and an adjacent string of character positions
are shifted away from the active presentation position. The contents of n
character positions at the other end of the shifted part are removed. The active
presentation position is moved to the line home position in the active line. The
line home position is established by the parameter value of SET LINE HOME (SLH).
The extent of the shifted part is established by SELECT EDITING EXTENT (SEE).
The effect of ICH on the start or end of a selected area, the start or end of a
qualified area, or a tabulation stop in the shifted part, is not defined by this
Standard.
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to DATA, ICH is used to
prepare the insertion of n characters, by putting into the erased state the
active data position and, depending on the setting of the CHARACTER EDITING MODE
(HEM), the n-1 preceding or following character positions in the data component,
where n equals the value of Pn. The previous contents of the active data
position and an adjacent string of character positions are shifted away from the
active data position. The contents of n character positions at the other end of
the shifted part are removed. The active data position is moved to the line home
position in the active line. The line home position is established by the
parameter value of SET LINE HOME (SLH).
*/
void ecma48_ICH(){
  ecma48_PRINT_CONTROL_SEQUENCE("ICH");
  int Pn = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0], NULL, 10) : 1;
  if(!modes.HEM){
    buf_insert_character_following(Pn);
  } else {
    buf_insert_character_preceding(Pn);
  }
  /* FIXME: What to do about moving the cursor to the line home position? Seems wrong.. */
  ecma48_end_control();
}

/*
CUU - CURSOR UP
Notation: (Pn)
Representation: CSI Pn 04/01
Parameter default value: Pn = 1
CUU causes the active presentation position to be moved upwards in the
presentation component by n line positions if the character path is horizontal,
or by n character positions if the character path is vertical, where n equals
the value of Pn.
*/
void ecma48_CUU(){
  ecma48_PRINT_CONTROL_SEQUENCE("CUU");
  int Pn = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0], NULL, 10) : 1;
  int top = buf.top_line;
  if(Pn == 0){
  	// VT100 Compat
  	Pn = 1;
  }
  if(buf_in_scroll_region()){
    top = screen_to_buf_row(sr.top);
  }
  buf.line -= Pn;
  if (buf.line < top) {
    buf.line = top;
  }
  ecma48_end_control();
}

/*
CUD - CURSOR DOWN
Notation: (Pn)
Representation: CSI Pn 04/02
Parameter default value: Pn = 1
CUD causes the active presentation position to be moved downwards in the
presentation component by n line positions if the character path is horizontal,
or by n character positions if the character path is vertical, where n equals
the value of Pn.
*/
void ecma48_CUD(){
  ecma48_PRINT_CONTROL_SEQUENCE("CUD");
  int Pn = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0], NULL, 10) : 1;
  int bot = buf_bottom_line();
  if(Pn == 0){
  	// VT100 Compat
  	Pn = 1;
  }
  if(buf_in_scroll_region()){
    bot = screen_to_buf_row(sr.bottom);
  }
  buf.line += Pn;
  if (buf.line > bot) {
    buf.line = bot;
  }
  ecma48_end_control();
}

/*
CUF - CURSOR RIGHT
Notation: (Pn)
Representation: CSI Pn 04/03
Parameter default value: Pn = 1
CUF causes the active presentation position to be moved rightwards in the
presentation component by n character positions if the character path is
horizontal, or by n line positions if the character path is vertical, where n
equals the value of Pn.
*/
void ecma48_CUF(){
  ecma48_PRINT_CONTROL_SEQUENCE("CUF");
  int Pn = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0], NULL, 10) : 1;
  if(Pn == 0){
  	// VT100 Compat
  	Pn = 1;
  }
  buf.col += Pn;
    if (buf.col > cols) {
      buf.col = cols - 1;
  }
  ecma48_end_control();
}

/*
CUB - CURSOR LEFT
Notation: (Pn)
Representation: CSI Pn 04/04
Parameter default value: Pn = 1
CUB causes the active presentation position to be moved leftwards in the
presentation component by n character positions if the character path is
horizontal, or by n line positions if the character path is vertical, where n
equals the value of Pn.
*/
void ecma48_CUB(){
  ecma48_PRINT_CONTROL_SEQUENCE("CUB");
  int Pn = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0], NULL, 10) : 1;
  if(Pn == 0){
  	// VT100 Compat
  	Pn = 1;
  }
  buf.col -= Pn;
    if (buf.col < 0) {
      buf.col = 0;
  }
  ecma48_end_control();
}

/*
CNL - CURSOR NEXT LINE
Notation: (Pn)
Representation: CSI Pn 04/05
Parameter default value: Pn = 1
CNL causes the active presentation position to be moved to the first character
position of the n-th following line in the presentation component, where n
equals the value of Pn.
*/
void ecma48_CNL(){
  ecma48_PRINT_CONTROL_SEQUENCE("CNL");
  int Pn = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0], NULL, 10) : 1;
  buf.line += Pn;
  if (buf.line > buf.top_line + rows - 1) {
    buf.line = buf.top_line + rows - 1;
  }
  buf.col = 0;
  ecma48_end_control();
}

/*
CPL - CURSOR PRECEDING LINE
Notation: (Pn)
Representation: CSI Pn 04/06
Parameter default value: Pn = 1
CPL causes the active presentation position to be moved to the first character
position of the n-th preceding line in the presentation component, where n
equals the value of Pn.
*/
void ecma48_CPL(){
  ecma48_PRINT_CONTROL_SEQUENCE("CPL");
  int Pn = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0], NULL, 10) : 1;
  buf.line -= Pn;
  if (buf.line < buf.top_line) {
    buf.line = buf.top_line;
  }
  buf.col = 0;
  ecma48_end_control();
}

/*
CHA - CURSOR CHARACTER ABSOLUTE
Notation: (Pn)
Representation: CSI Pn 04/07
Parameter default value: Pn = 1
CHA causes the active presentation position to be moved to character position n
in the active line in the presentation component, where n equals the value of
Pn.
*/
void ecma48_CHA(){
  ecma48_PRINT_CONTROL_SEQUENCE("CHA");
  int Pn = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0], NULL, 10) : 1;
  if(Pn > 0 && Pn <= cols){
    buf.col = Pn - 1;
  }
  ecma48_end_control();
}

/*
CUP - CURSOR POSITION
Notation: (Pn1;Pn2)
Representation: CSI Pn1;Pn2 04/08
Parameter default values: Pn1 = 1; Pn2 = 1
CUP causes the active presentation position to be moved in the presentation
component to the n-th line position according to the line progression and to the
m-th character position according to the character path, where n equals the
value of Pn1 and m equals the value of Pn2.
*/
void ecma48_CUP(){
  ecma48_PRINT_CONTROL_SEQUENCE("CUP");
  int Pn1 = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0],NULL, 10) : 1;
  int Pn2 = escape_args.args[1][0] != '\0' ? (int)strtol(escape_args.args[1],NULL, 10) : 1;
  buf.line = buf.top_line + Pn1 - 1;
  buf.col = Pn2 - 1;
  // sanity check
  PRINT(stderr, "Got move to buf.line %d buf.col %d - arg[0]=%d, arg[1]=%d\n",
        buf.line,
        buf.col,
        Pn1,
        Pn2);
  if(buf.line < 0){
    buf.line = 0;
  }
  if((buf.line - buf.top_line) >= rows){
    buf.line = buf.top_line + rows - 1;
  }
  if(modes.DECOM){
  	// DEC ORIGIN MODE - coordinates are relative to scroll region.
  	// We increment buf.line + (sr.top - 1).
  	buf.line += (sr.top - 1);
  	PRINT(stderr, "Moving within origin mode - buf.line = %d\n", buf.line);
  }
  if(buf.col < 0){
    buf.col = 0;
  }
  if(buf.col >= cols){
    buf.col = cols -1;
  }
  PRINT(stderr, "Moving cursor to line %d, column %d\n", buf.line, buf.col);
  PRINT(stderr, "buf.top_line is %d\n", buf.top_line);
  ecma48_end_control();
}

/*
CHT - CURSOR FORWARD TABULATION
Notation: (Pn)
Representation: CSI Pn 04/09
Parameter default value: Pn = 1
CHT causes the active presentation position to be moved to the character
position corresponding to the n-th following character tabulation stop in the
presentation component, according to the character path, where n equals the
value of Pn.
*/
void ecma48_CHT(){
  ecma48_PRINT_CONTROL_SEQUENCE("CHT");
  int Pn1 = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0],NULL, 10) : 1;
  int i;
  for(i = 1; i <= Pn1; ++i){
    ecma48_HT();
  }
  ecma48_end_control();
}

/*
ED - ERASE IN PAGE
Notation: (Ps)
Representation: CSI Ps 04/10
Parameter default value: Ps = 0
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to PRESENTATION, ED causes
some or all character positions of the active page (the page which contains the
active presentation position in the presentation component) to be put into the
erased state, depending on the parameter values:
0 the active presentation position and the character positions up to the end of
the page are put into the erased state
1 the character positions from the beginning of the page up to and including the
active presentation position are put into the erased state
2 all character positions of the page are put into the erased state
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to DATA, ED causes some or all
character positions of the active page (the page which contains the active data
position in the data component) to be put into the erased state, depending on
the parameter values:
0 the active data position and the character positions up to the end of the page
are put into the erased state
1 the character positions from the beginning of the page up to and including the
active data position are put into the erased state
2 all character positions of the page are put into the erased state
Whether the character positions of protected areas are put into the erased
state, or the character positions of unprotected areas only, depends on the
setting of the ERASURE MODE (ERM).
*/
void ecma48_ED(){
  ecma48_PRINT_CONTROL_SEQUENCE("ED");
  int i;
  int Pn = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0], NULL, 10) : 0;
  switch (Pn) {
    case 0: // from cursor to end of screen
      buf_erase_line(&buf.text[buf.line][buf.col], (cols-buf.col));
      for(i=(buf.line-buf.top_line + 1); i < rows; ++i){
        buf_erase_line(buf.text[buf.top_line + i], cols );
      }
      break;
    case 1: // from top of screen to cursor
      buf_erase_line(buf.text[buf.line], (buf.col+1));
      for(i= 0; i < (buf.line-buf.top_line); ++i){
        buf_erase_line(buf.text[buf.top_line + i], cols);
      }
      break;
    case 2: // entire screen
      ecma48_clear_display();
      break;
  };
  ecma48_end_control();
}

/*
EL - ERASE IN LINE
Notation: (Ps)
Representation: CSI Ps 04/11
Parameter default value: Ps = 0
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to PRESENTATION, EL causes
some or all character positions of the active line (the line which contains the
active presentation position in the presentation component) to be put into the
erased state, depending on the parameter values:
0 the active presentation position and the character positions up to the end of
the line are put into the erased state
1 the character positions from the beginning of the line up to and including the
active presentation position are put into the erased state
2 all character positions of the line are put into the erased state
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to DATA, EL causes some or all
character positions of the active line (the line which contains the active data
position in the data component) to be put into the erased state, depending on
the parameter values:
0 the active data position and the character positions up to the end of the line
are put into the erased state
1 the character positions from the beginning of the line up to and including the
active data position are put into the erased state
2 all character positions of the line are put into the erased state
Whether the character positions of protected areas are put into the erased
state, or the character positions of unprotected areas only, depends on the
setting of the ERASURE MODE (ERM).
*/
void ecma48_EL(){
  ecma48_PRINT_CONTROL_SEQUENCE("EL");
  int Pn = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0], NULL, 10) : 0;
  switch (Pn) {
    case 0: // from cursor to end of line
      buf_erase_line(&buf.text[buf.line][buf.col], (cols-buf.col));
      break;
    case 1: // from start of line to cursor
      buf_erase_line(buf.text[buf.line], (buf.col+1));
      break;
    case 2: // entire line
      buf_erase_line(buf.text[buf.line], cols);
      //buf.col = 0;
      break;
  };
  ecma48_end_control();
}

/*
IL - INSERT LINE
Notation: (Pn)
Representation: CSI Pn 04/12
Parameter default value: Pn = 1
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to PRESENTATION, IL is used to
prepare the insertion of n lines, by putting into the erased state in the
presentation component the active line (the line that contains the active
presentation position) and, depending on the setting of the LINE EDITING MODE
(VEM), the n-1 preceding or following lines, where n equals the value of Pn. The
previous contents of the active line and of adjacent lines are shifted away from
the active line. The contents of n lines at the other end of the shifted part
are removed. The active presentation position is moved to the line home position
in the active line. The line home position is established by the parameter value
of SET LINE HOME (SLH).
The extent of the shifted part is established by SELECT EDITING EXTENT (SEE).
Any occurrences of the start or end of a selected area, the start or end of a
qualified area, or a tabulation stop in the shifted part, are also shifted.
If the TABULATION STOP MODE (TSM) is set to SINGLE, character tabulation stops
are cleared in the lines that are put into the erased state.
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to DATA, IL is used to prepare
the insertion of n lines, by putting into the erased state in the data component
the active line (the line that contains the active data position) and, depending
on the setting of the LINE EDITING MODE (VEM), the n-1 preceding or following
lines, where n equals the value of Pn. The previous contents of the active line
and of adjacent lines are shifted away from the active line. The contents of n
lines at the other end of the shifted part are removed. The active data position
is moved to the line home position in the active line. The line home position is
established by the parameter value of SET LINE HOME (SLH).
*/
void ecma48_IL(){
  ecma48_PRINT_CONTROL_SEQUENCE("IL");
  int Pn = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0], NULL, 10) : 1;
  int i = 0, j = 0;
  struct screenchar* tmp;
  do {
    /* swap the last line (which will scroll off) into the new line */
    // cache
    tmp = buf.text[buf.top_line + sr.bottom - 1];
    // scroll
    for(j= buf.top_line + sr.bottom - 2; j >= buf.line ; --j){
      buf.text[j+1] = buf.text[j];
    }
    // and insert blank line
    buf.text[buf.line] = tmp;
    buf_erase_line(buf.text[buf.line], cols);
    ++i;
  } while (i < Pn);
  buf.col = 0;
  ecma48_end_control();
}

/*
DL - DELETE LINE
Notation: (Pn)
Representation: CSI Pn 04/13
Parameter default value: Pn = 1
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to PRESENTATION, DL causes the
contents of the active line (the line that contains the active presentation
position) and, depending on the setting of the LINE EDITING MODE (VEM), the
contents of the n-1 preceding or following lines to be removed from the
presentation component, where n equals the value of Pn. The resulting gap is
closed by shifting the contents of a number of adjacent lines towards the active
line. At the other end of the shifted part, n lines are put into the erased
state.
The active presentation position is moved to the line home position in the
active line. The line home position is established by the parameter value of SET
LINE HOME (SLH). If the TABULATION STOP MODE (TSM) is set to SINGLE, character
tabulation stops are cleared in the lines that are put into the erased state.
The extent of the shifted part is established by SELECT EDITING EXTENT (SEE).
Any occurrences of the start or end of a selected area, the start or end of a
qualified area, or a tabulation stop in the shifted part, are also shifted.
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to DATA, DL causes the
contents of the active line (the line that contains the active data position)
and, depending on the setting of the LINE EDITING MODE (VEM), the contents of
the n-1 preceding or following lines to be removed from the data component,
where n equals the value of Pn. The resulting gap is closed by shifting the
contents of a number of adjacent lines towards the active line. At the other end
of the shifted part, n lines are put into the erased state. The active data
position is moved to the line home position in the active line. The line home
position is established by the parameter value of SET LINE HOME (SLH).
*/
void ecma48_DL(){
  ecma48_PRINT_CONTROL_SEQUENCE("DL");
  int Pn = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0], NULL, 10) : 1;
  int i = 0, j = 0;
  struct screenchar* tmp;
  do {
    /* grab the line being deleted, and move it into the bottom of the screen */
    // cache
    tmp = buf.text[buf.line];
    // scroll
    for(j= buf.line; j < buf.top_line + sr.bottom - 1 ; ++j){
      buf.text[j] = buf.text[j+1];
    }
    // and insert blank line
    buf.text[buf.top_line + sr.bottom - 1] = tmp;
    buf_erase_line(buf.text[buf.top_line + sr.bottom - 1], cols);
    ++i;
  } while (i < Pn);
  buf.col = 0;
  ecma48_end_control();
}

/*
EF - ERASE IN FIELD
Notation: (Ps)
Representation: CSI Ps 04/14
Parameter default value: Ps = 0
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to PRESENTATION, EF causes
some or all character positions of the active field (the field which contains
the active presentation position in the presentation component) to be put into
the erased state, depending on the parameter values:
0 the active presentation position and the character positions up to the end of
the field are put into the erased state
1 the character positions from the beginning of the field up to and including
the active presentation position are put into the erased state
2 all character positions of the field are put into the erased state
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to DATA, EF causes some or all
character positions of the active field (the field which contains the active
data position in the data component) to be put into the erased state, depending
on the parameter values:
0 the active data position and the character positions up to the end of the
field are put into the erased state
1 the character positions from the beginning of the field up to and including
the active data position are put into the erased state
2 all character positions of the field are put into the erased state
Whether the character positions of protected areas are put into the erased
state, or the character positions of unprotected areas only, depends on the
setting of the ERASURE MODE (ERM).
*/
void ecma48_EF(){
  ecma48_NOT_IMPLEMENTED("EF");
}

/*
EA - ERASE IN AREA
Notation: (Ps)
Representation: CSI Ps 04/15
Parameter default value: Ps = 0
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to PRESENTATION, EA causes
some or all character positions in the active qualified area (the qualified area
in the presentation component which contains the active presentation position)
to be put into the erased state, depending on the parameter values:
0 the active presentation position and the character positions up to the end of
the qualified area are put into the erased state
1 the character positions from the beginning of the qualified area up to and
including the active presentation position are put into the erased state
2 all character positions of the qualified area are put into the erased state
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to DATA, EA causes some or all
character positions in the active qualified area (the qualified area in the data
component which contains the active data position) to be put into the erased
state, depending on the parameter values:
0 the active data position and the character positions up to the end of the
qualified area are put into the erased state
1 the character positions from the beginning of the qualified area up to and
including the active data position are put into the erased state
2 all character positions of the qualified area are put into the erased state
Whether the character positions of protected areas are put into the erased
state, or the character positions of unprotected areas only, depends on the
setting of the ERASURE MODE (ERM).
*/
void ecma48_EA(){
  ecma48_NOT_IMPLEMENTED("EA");
}

/*
DCH - DELETE CHARACTER
Notation: (Pn)
Representation: CSI Pn 05/00
Parameter default value: Pn = 1
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to PRESENTATION, DCH causes
the contents of the active presentation position and, depending on the setting
of the CHARACTER EDITING MODE (HEM), the contents of the n-1 preceding or
following character positions to be removed from the presentation component,
where n equals the value of Pn. The resulting gap is closed by shifting the
contents of the adjacent character positions towards the active presentation
position. At the other end of the shifted part, n character positions are put
into the erased state.
The extent of the shifted part is established by SELECT EDITING EXTENT (SEE).
The effect of DCH on the start or end of a selected area, the start or end of a
qualified area, or a tabulation stop in the shifted part is not defined by this
Standard.
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to DATA, DCH causes the
contents of the active data position and, depending on the setting of the
CHARACTER EDITING MODE (HEM), the contents of the n-1 preceding or following
character positions to be removed from the data component, where n equals the
value of Pn. The resulting gap is closed by shifting the contents of the
adjacent character positions towards the active data position. At the other end
of the shifted part, n character positions are put into the erased state.
*/
void ecma48_DCH(){
  ecma48_PRINT_CONTROL_SEQUENCE("DCH");
  int Pn = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0], NULL, 10) : 1;
  if(!modes.HEM){
    buf_delete_character_following(Pn);
  } else {
    buf_delete_character_preceding(Pn);
  }
  ecma48_end_control();

}

/*
SEE - SELECT EDITING EXTENT
Notation: (Ps)
Representation: CSI Ps 05/01
Parameter default value: Ps = 0
SEE is used to establish the editing extent for subsequent character or line
insertion or deletion. The established extent remains in effect until the next
occurrence of SEE in the data stream. The editing extent depends on the
parameter value:
0 the shifted part is limited to the active page in the presentation component
1 the shifted part is limited to the active line in the presentation component
2 the shifted part is limited to the active field in the presentation component
3 the shifted part is limited to the active qualified area
4 the shifted part consists of the relevant part of the entire presentation
component.
*/
void ecma48_SEE(){
  ecma48_NOT_IMPLEMENTED("SEE");
}

/*
CPR - ACTIVE POSITION REPORT
Notation: (Pn1;Pn2)
Representation: CSI Pn1;Pn2 05/02
Parameter default values: Pn1 = 1; Pn2 = 1
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to PRESENTATION, CPR is used
to report the active presentation position of the sending device as residing in
the presentation component at the n-th line position according to the line
progression and at the m-th character position according to the character path,
where n equals the value of Pn1 and m equals the value of Pn2.
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to DATA, CPR is used to report
the active data position of the sending device as residing in the data component
at the n-th line position according to the line progression and at the m-th
character position according to the character progression, where n equals the
value of Pn1 and m equals the value of Pn2.
CPR may be solicited by a DEVICE STATUS REPORT (DSR) or be sent unsolicited.
*/
void ecma48_CPR(){
  ecma48_NOT_IMPLEMENTED("CPR");
}

/*
SU - SCROLL UP
Notation: (Pn)
Representation: CSI Pn 05/03
Parameter default value: Pn = 1
SU causes the data in the presentation component to be moved by n line positions
if the line orientation is horizontal, or by n character positions if the line
orientation is vertical, such that the data appear to move up; where n equals
the value of Pn.
The active presentation position is not affected by this control function.
*/
void ecma48_SU(){
  ecma48_PRINT_CONTROL_SEQUENCE("SU");
  int Pn = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0], NULL, 10) : 1;
  int i = 0, j = 0;
  struct screenchar* tmp;
  do {
    /* swap the top line (which will scroll off) into the new line */
    // cache
    tmp = buf.text[buf.top_line + sr.top - 1];
    // scroll
    for(j= buf.top_line + sr.top -1; j <= buf.top_line + sr.bottom -1 ; ++j){
      buf.text[j] = buf.text[j+1];
    }
    // and insert blank line
    buf.text[buf.top_line + sr.bottom -1] = tmp;
    buf_erase_line(buf.text[buf.top_line + sr.bottom -1], cols);
    ++i;
  } while (i < Pn);
  ecma48_end_control();
}

/*
SD - SCROLL DOWN
Notation: (Pn)
Representation: CSI Pn 05/04
Parameter default value: Pn = 1
SD causes the data in the presentation component to be moved by n line positions
if the line orientation is horizontal, or by n character positions if the line
orientation is vertical, such that the data appear to move down; where n equals
the value of Pn.
The active presentation position is not affected by this control function.
*/
void ecma48_SD(){
  ecma48_PRINT_CONTROL_SEQUENCE("SD");
  int Pn = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0], NULL, 10) : 1;
  int i = 0, j = 0;
  struct screenchar* tmp;
  do {
    /* swap the last line (which will scroll off) into the new line */
    // cache
    tmp = buf.text[buf.top_line + sr.bottom - 1];
    // scroll
    for(j= buf.top_line + sr.bottom - 2; j >= buf.top_line + sr.top -1; --j){
      buf.text[j+1] = buf.text[j];
    }
    // and insert blank line
    buf.text[buf.top_line + sr.top -1] = tmp;
    buf_erase_line(buf.text[buf.top_line + sr.top -1], cols);
    ++i;
  } while (i < Pn);
  ecma48_end_control();
}

/*
NP - NEXT PAGE
Notation: (Pn)
Representation: CSI Pn 05/05
Parameter default value: Pn = 1
NP causes the n-th following page in the presentation component to be displayed,
where n equals the value of Pn.
The effect of this control function on the active presentation position is not
defined by this Standard.
*/
void ecma48_NP(){
  ecma48_NOT_IMPLEMENTED("NP");
}

/*
PP - PRECEDING PAGE
Notation: (Pn)
Representation: CSI Pn 05/06
Parameter default value: Pn = 1
PP causes the n-th preceding page in the presentation component to be displayed,
where n equals the value of Pn. The effect of this control function on the
active presentation position is not defined by this Standard.
*/
void ecma48_PP(){
  ecma48_NOT_IMPLEMENTED("PP");
}

/*
CTC - CURSOR TABULATION CONTROL
Notation: (Ps...)
Representation: CSI Ps... 05/07
Parameter default value: Ps = 0
CTC causes one or more tabulation stops to be set or cleared in the presentation
component, depending on the parameter values:
0 a character tabulation stop is set at the active presentation position
1 a line tabulation stop is set at the active line (the line that contains the
active presentation position)
2 the character tabulation stop at the active presentation position is cleared
3 the line tabulation stop at the active line is cleared
4 all character tabulation stops in the active line are cleared
5 all character tabulation stops are cleared
6 all line tabulation stops are cleared
In the case of parameter values 0, 2 or 4 the number of lines affected depends
on the setting of the TABULATION STOP MODE (TSM).
*/
void ecma48_CTC(){
  ecma48_NOT_IMPLEMENTED("CTC");
}

/*
ECH - ERASE CHARACTER
Notation: (Pn)
Representation: CSI Pn 05/08
Parameter default value: Pn = 1
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to PRESENTATION, ECH causes
the active presentation position and the n-1 following character positions in
the presentation component to be put into the erased state, where n equals the
value of Pn.
If the DEVICE COMPONENT SELECT MODE (DCSM) is set to DATA, ECH causes the active
data position and the n-1 following character positions in the data component to
be put into the erased state, where n equals the value of Pn.
Whether the character positions of protected areas are put into the erased
state, or the character positions of unprotected areas only, depends on the
setting of the ERASURE MODE (ERM).
*/
void ecma48_ECH(){
  ecma48_PRINT_CONTROL_SEQUENCE("SD");
  int Pn = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0], NULL, 10) : 1;
  /* Make sure not to overrun the end of the line */
  int max = cols - buf.col;
  Pn = Pn > max ? max : Pn;
  buf_erase_line(&buf.text[buf.line][buf.col], Pn);
  ecma48_end_control();
}

/*
CVT - CURSOR LINE TABULATION
Notation: (Pn)
Representation: CSI Pn 05/09
Parameter default value: Pn = 1
CVT causes the active presentation position to be moved to the corresponding
character position of the line corresponding to the n-th following line
tabulation stop in the presentation component, where n equals the value of Pn.
*/
void ecma48_CVT(){
  ecma48_NOT_IMPLEMENTED("CVT");
}

/*
CBT - CURSOR BACKWARD TABULATION
Notation: (Pn)
Representation: CSI Pn 05/10
Parameter default value: Pn = 1
CBT causes the active presentation position to be moved to the character
position corresponding to the n-th preceding character tabulation stop in the
presentation component, according to the character path, where n equals the
value of Pn.
*/
void ecma48_CBT(){
  ecma48_PRINT_CONTROL_SEQUENCE("CBT");
  int Pn1 = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0],NULL, 10) : 1;
  int i, x;
  for(i = 1; i <= Pn1; ++i){
    x = screen_prev_tab_x();
    buf.col = screen_to_buf_col(x);
  }
  ecma48_end_control();
}

/*
SRS - START REVERSED STRING
Notation: (Ps)
Representation: CSI Ps 05/11
Parameter default value: Ps = 0
SRS is used to establish in the data component the beginning and the end of a
string of characters as well as the direction of the string. This direction is
opposite to that currently established. The indicated string follows the
preceding text. The established character progression is not affected.
The beginning of a reversed string is indicated by SRS with a parameter value of
1. A reversed string may contain one or more nested strings. These nested
strings may be reversed strings the beginnings of which are indicated by SRS
with a parameter value of 1, or directed strings the beginnings of which are
indicated by START DIRECTED STRING (SDS) with a parameter value not equal to 0.
Every beginning of such a string invokes the next deeper level of nesting.
This Standard does not define the location of the active data position within
any such nested string.
The end of a reversed string is indicated by SRS with a parameter value of 0.
Every end of such a string re-establishes the next higher level of nesting (the
one in effect prior to the string just ended). The direction is re-established
to that in effect prior to the string just ended. The active data position is
moved to the character position following the characters of the string just
ended.
The parameter values are:
0 end of a reversed string; re-establish the previous direction
1 beginning of a reversed string; reverse the direction.
NOTE 1 The effect of receiving a CVT, HT, SCP, SPD or VT control function within
an SRS string is not defined by this Standard.
NOTE 2 The control functions for area definition (DAQ, EPA, ESA, SPA, SSA)
should not be used within an SRS string.
*/
void ecma48_SRS(){
  ecma48_NOT_IMPLEMENTED("SRS");
}

/*
PTX - PARALLEL TEXTS
Notation: (Ps)
Representation: CSI Ps 05/12
Parameter default value: Ps = 0
- 54 -
PTX is used to delimit strings of graphic characters that are communicated one
after another in the data stream but that are intended to be presented in
parallel with one another, usually in adjacent lines.
The parameter values are
0 end of parallel texts
1 beginning of a string of principal parallel text
2 beginning of a string of supplementary parallel text
3 beginning of a string of supplementary Japanese phonetic annotation
4 beginning of a string of supplementary Chinese phonetic annotation
5 end of a string of supplementary phonetic annotations
PTX with a parameter value of 1 indicates the beginning of the string of
principal text intended to be presented in parallel with one or more strings of
supplementary text.
PTX with a parameter value of 2, 3 or 4 indicates the beginning of a string of
supplementary text that is intended to be presented in parallel with either a
string of principal text or the immediately preceding string of supplementary
text, if any; at the same time it indicates the end of the preceding string of
principal text or of the immediately preceding string of supplementary text, if
any. The end of a string of supplementary text is indicated by a subsequent
occurrence of PTX with a parameter value other than 1.
PTX with a parameter value of 0 indicates the end of the strings of text
intended to be presented in parallel with one another.
NOTE PTX does not explicitly specify the relative placement of the strings of
principal and supplementary parallel texts, or the relative sizes of graphic
characters in the strings of parallel text. A string of supplementary text is
normally presented in a line adjacent to the line containing the string of
principal text, or adjacent to the line containing the immediately preceding
string of supplementary text, if any. The first graphic character of the string
of principal text and the first graphic character of a string of supplementary
text are normally presented in the same position of their respective lines.
However, a string of supplementary text longer (when presented) than the
associated string of principal text may be centred on that string. In the case
of long strings of text, such as paragraphs in different languages, the strings
may be presented in successive lines in parallel columns, with their beginnings
aligned with one another and the shorter of the paragraphs followed by an
appropriate amount of "white space".
Japanese phonetic annotation typically consists of a few half-size or smaller
Kana characters which indicate the pronunciation or interpretation of one or
more Kanji characters and are presented above those Kanji characters if the
character path is horizontal, or to the right of them if the character path is
vertical.
Chinese phonetic annotation typically consists of a few Pinyin characters which
indicate the pronunciation of one or more Hanzi characters and are presented
above those Hanzi characters. Alternatively, the Pinyin characters may be
presented in the same line as the Hanzi characters and following the respective
Hanzi characters. The Pinyin characters will then be presented within enclosing
pairs of parentheses.
*/
void ecma48_PTX(){
  ecma48_NOT_IMPLEMENTED("SRS");
}

/*
SDS - START DIRECTED STRING
Notation: (Ps)
Representation: CSI Ps 05/13
Parameter default value: Ps = 0
SDS is used to establish in the data component the beginning and the end of a
string of characters as well as the direction of the string. This direction may
be different from that currently established. The indicated string follows the
preceding text. The established character progression is not affected.
The beginning of a directed string is indicated by SDS with a parameter value
not equal to 0. A directed string may contain one or more nested strings. These
nested strings may be directed strings the beginnings of which are indicated by
SDS with a parameter value not equal to 0, or reversed strings the beginnings of
which are indicated by START REVERSED STRING (SRS) with a parameter value of 1.
Every beginning of such a string invokes the next deeper level of nesting.
This Standard does not define the location of the active data position within
any such nested string.
The end of a directed string is indicated by SDS with a parameter value of 0.
Every end of such a string re-establishes the next higher level of nesting (the
one in effect prior to the string just ended). The direction is re-established
to that in effect prior to the string just ended. The active data position is
moved to the character position following the characters of the string just
ended.
The parameter values are:
0 end of a directed string; re-establish the previous direction
1 start of a directed string; establish the direction left-to-right
2 start of a directed string; establish the direction right-to-left
NOTE 1 The effect of receiving a CVT, HT, SCP, SPD or VT control function within
an SDS string is not defined by this Standard.
NOTE 2 The control functions for area definition (DAQ, EPA, ESA, SPA, SSA)
should not be used within an SDS string.
*/
void ecma48_SDS(){
  ecma48_NOT_IMPLEMENTED("SDS");
}

/*
SIMD - SELECT IMPLICIT MOVEMENT DIRECTION
Notation: (Ps)
Representation: CSI Ps 05/14
Parameter default value: Ps = 0
SIMD is used to select the direction of implicit movement of the data position
relative to the character progression. The direction selected remains in effect
until the next occurrence of SIMD.
The parameter values are:
0 the direction of implicit movement is the same as that of the character
progression
1 the direction of implicit movement is opposite to that of the character
progression.
*/
void ecma48_SIMD(){
  ecma48_NOT_IMPLEMENTED("SIMD");
}

/*
HPA - CHARACTER POSITION ABSOLUTE
Notation: (Pn)
Representation: CSI Pn 06/00
Parameter default value: Pn = 1
HPA causes the active data position to be moved to character position n in the
active line (the line in the data component that contains the active data
position), where n equals the value of Pn.
*/
void ecma48_HPA(){
  ecma48_NOT_IMPLEMENTED("HPA");
}

/*
HPR - CHARACTER POSITION FORWARD
Notation: (Pn)
Representation: CSI Pn 06/01
Parameter default value: Pn = 1
HPR causes the active data position to be moved by n character positions in the
data component in the direction of the character progression, where n equals the
value of Pn.

VT100 - DA - Device Attributes
This expects the terminal to return a code indicating supported options. We don't
do it because we're not VT100, but things like vttest expect us to respond.
*/
void ecma48_HPR(){
  ecma48_NOT_IMPLEMENTED("HPR");
}

/*
REP - REPEAT
Notation: (Pn)
Representation: CSI Pn 06/02
Parameter default value: Pn = 1
REP is used to indicate that the preceding character in the data stream, if it
is a graphic character (represented by one or more bit combinations) including
SPACE, is to be repeated n times, where n equals the value of Pn. If the
character preceding REP is a control function or part of a control function, the
effect of REP is not defined by this Standard.
*/
void ecma48_REP(){
  ecma48_PRINT_CONTROL_SEQUENCE("REP");
  int Pn = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0], NULL, 10) : 1;
  int i = 0;
  for(i = 0; i < Pn; ++i){
    ecma48_add_char(last_char);
  }
  ecma48_end_control();
  //ecma48_NOT_IMPLEMENTED("REP");
}

/*
DA - DEVICE ATTRIBUTES
Notation: (Ps)
Representation: CSI Ps 06/03
Parameter default value: Ps = 0
With a parameter value not equal to 0, DA is used to identify the device which
sends the DA. The parameter value is a device type identification code according
to a register which is to be established. If the parameter value is 0, DA is
used to request an identifying DA from a device.
*/
void ecma48_DA(){
  ecma48_PRINT_CONTROL_SEQUENCE("DA");
  int Pn = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0], NULL, 10) : 0;
  switch(Pn){
    case 0:  io_write_master_char(TERMID, sizeof(TERMID)); break;
    default: fprintf(stderr, "-- Unhandled code in ecma48_DA: %d\n", Pn); break;
  };
  ecma48_end_control();
}

/*
VPA - LINE POSITION ABSOLUTE
Notation: (Pn)
Representation: CSI Pn 06/04
Parameter default value: Pn = 1
VPA causes the active data position to be moved to line position n in the data
component in a direction parallel to the line progression, where n equals the
value of Pn.
*/
void ecma48_VPA(){
  ecma48_NOT_IMPLEMENTED("VPA");
}

/*
VPR - LINE POSITION FORWARD
Notation: (Pn)
Representation: CSI Pn 06/05
Parameter default value: Pn = 1
VPR causes the active data position to be moved by n line positions in the data
component in a direction parallel to the line progression, where n equals the
value of Pn.
*/
void ecma48_VPR(){
  ecma48_NOT_IMPLEMENTED("VPR");
}

/*
HVP - CHARACTER AND LINE POSITION
Notation: (Pn1;Pn2)
Representation: CSI Pn1;Pn2 06/06
Parameter default values: Pn1 = 1; Pn2 = 1
HVP causes the active data position to be moved in the data component to the
n-th line position according to the line progression and to the m-th character
position according to the character progression, where n equals the value of Pn1
and m equals the value of Pn2.
*/
void ecma48_HVP(){

  ecma48_PRINT_CONTROL_SEQUENCE("HVP");
  ecma48_CUP();

  //ecma48_end_control();
}

/*
TBC - TABULATION CLEAR
Notation: (Ps)
Representation: CSI Ps 06/07
Parameter default value: Ps = 0
TBC causes one or more tabulation stops in the presentation component to be
cleared, depending on the parameter value:
0 the character tabulation stop at the active presentation position is cleared
1 the line tabulation stop at the active line is cleared
2 all character tabulation stops in the active line are cleared
3 all character tabulation stops are cleared
4 all line tabulation stops are cleared
5 all tabulation stops are cleared
In the case of parameter value 0 or 2 the number of lines affected depends on
the setting of the TABULATION STOP MODE (TSM)
*/
void ecma48_TBC(){
  ecma48_PRINT_CONTROL_SEQUENCE("TBC");
  int Pn = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0], NULL, 10) : 0;
  int i;
  switch (Pn){
    case 0 : clear_char_tabstop_at(buf_to_screen_row(-1), buf_to_screen_col(-1)); break;
    case 1 : buf_clear_vtab(buf_to_screen_row(-1)); break;
    case 2 : break;//clear_char_tabstops_on_row(buf_to_screen_row(-1)); break;
    case 3 : clear_all_char_tabstops() ; break;
    case 4 : buf_clear_all_vtabs(); break;
    case 5 : clear_all_char_tabstops(); buf_clear_all_vtabs(); break;
  }
  ecma48_end_control();
}

/*
SM - SET MODE
Notation: (Ps...)
Representation: CSI Ps... 06/08
No parameter default value.
SM causes the modes of the receiving device to be set as specified by the
parameter values:
1 GUARDED AREA TRANSFER MODE (GATM)
2 KEYBOARD ACTION MODE (KAM)
3 CONTROL REPRESENTATION MODE (CRM)
4 INSERTION REPLACEMENT MODE (IRM)
5 STATUS REPORT TRANSFER MODE (SRTM)
6 ERASURE MODE (ERM)
7 LINE EDITING MODE (VEM)
8 BI-DIRECTIONAL SUPPORT MODE (BDSM)
9 DEVICE COMPONENT SELECT MODE (DCSM)
10 CHARACTER EDITING MODE (HEM)
11 POSITIONING UNIT MODE (PUM) (see F.4.1 in annex F)
12 SEND/RECEIVE MODE (SRM)
13 FORMAT EFFECTOR ACTION MODE (FEAM)
14 FORMAT EFFECTOR TRANSFER MODE (FETM)
15 MULTIPLE AREA TRANSFER MODE (MATM)
16 TRANSFER TERMINATION MODE (TTM)
17 SELECTED AREA TRANSFER MODE (SATM)
18 TABULATION STOP MODE (TSM)
19 (Shall not be used; see F.5.1 in annex F)
20 (Shall not be used; see F.5.2 in annex F)
20 (vt100) LNM – Line Feed/New Line Mode - when reset, LF=>LF, when set LF=>CRLF
21 GRAPHIC RENDITION COMBINATION (GRCM)
22 ZERO DEFAULT MODE (ZDM) (see F.4.2 in annex F)
NOTE Private modes may be implemented using private parameters, see 5.4.1 and
7.4.
*/
void ecma48_SM(){
  ecma48_PRINT_CONTROL_SEQUENCE("SM");
  int Pn = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0], NULL, 10) : 0;
  switch(Pn){
    case 1: modes.GATM = 1; break;
    case 2: modes.KAM = 1; break;
    case 3: modes.CRM = 1; break;
    case 4: modes.IRM = 1; break;
    case 5: modes.SRTM = 1; break;
    case 6: modes.ERM = 1; break;
    case 7: modes.VEM = 1; break;
    case 8: modes.BDSM = 1; break;
    case 9: modes.DCSM = 1; break;
    case 10: modes.HEM = 1; break;
    case 11: modes.PUM = 1; break;
    case 12: modes.SRM = 1; break;
    case 13: modes.FEAM = 1; break;
    case 14: modes.FETM = 1; break;
    case 15: modes.MATM = 1; break;
    case 16: modes.TTM = 1; break;
    case 17: modes.SATM = 1; break;
    case 18: modes.TSM = 1; break;
    case 20: modes.LNM = 1; break;
    case 21: modes.GRCM = 1; break;
    case 22: modes.ZDM = 1; break;
  }
  ecma48_end_control();

}

/*
MC - MEDIA COPY
Notation: (Ps)
Representation: CSI Ps 06/09
Parameter default value: Ps = 0
MC is used either to initiate a transfer of data from or to an auxiliary
input/output device or to enable or disable the relay of the received data
stream to an auxiliary input/output device, depending on the parameter value:
0 initiate transfer to a primary auxiliary device
1 initiate transfer from a primary auxiliary device
2 initiate transfer to a secondary auxiliary device
3 initiate transfer from a secondary auxiliary device
4 stop relay to a primary auxiliary device
5 start relay to a primary auxiliary device
6 stop relay to a secondary auxiliary device
7 start relay to a secondary auxiliary device
This control function may not be used to switch on or off an auxiliary device.
*/
void ecma48_MC(){
  ecma48_NOT_IMPLEMENTED("MC");
}

/*
HPB - CHARACTER POSITION BACKWARD
Notation: (Pn)
Representation: CSI Pn 06/10
Parameter default value: Pn = 1
HPB causes the active data position to be moved by n character positions in the
data component in the direction opposite to that of the character progression,
where n equals the value of Pn.
*/
void ecma48_HPB(){
  ecma48_NOT_IMPLEMENTED("HPB");
}

/*
VPB - LINE POSITION BACKWARD
Notation: (Pn)
Representation: CSI Pn 06/11
Parameter default value: Pn = 1
VPB causes the active data position to be moved by n line positions in the data
component in a direction opposite to that of the line progression, where n
equals the value of Pn.
*/
void ecma48_VPB(){
  ecma48_NOT_IMPLEMENTED("HPB");
}

/*
RM - RESET MODE
Notation: (Ps...)
Representation: CSI Ps... 06/12
No parameter default value.
RM causes the modes of the receiving device to be reset as specified by the
parameter values:
1 GUARDED AREA TRANSFER MODE (GATM)
2 KEYBOARD ACTION MODE (KAM)
3 CONTROL REPRESENTATION MODE (CRM)
4 INSERTION REPLACEMENT MODE (IRM)
5 STATUS REPORT TRANSFER MODE (SRTM)
6 ERASURE MODE (ERM)
7 LINE EDITING MODE (VEM)
8 BI-DIRECTIONAL SUPPORT MODE (BDSM)
9 DEVICE COMPONENT SELECT MODE (DCSM)
10 CHARACTER EDITING MODE (HEM)
11 POSITIONING UNIT MODE (PUM) (see F.4.1 in annex F)
12 SEND/RECEIVE MODE (SRM)
13 FORMAT EFFECTOR ACTION MODE (FEAM)
14 FORMAT EFFECTOR TRANSFER MODE (FETM)
15 MULTIPLE AREA TRANSFER MODE (MATM)
16 TRANSFER TERMINATION MODE (TTM)
17 SELECTED AREA TRANSFER MODE (SATM)
18 TABULATION STOP MODE (TSM)
19 (Shall not be used; see F.5.1 in annex F)
20 (Shall not be used; see F.5.2 in annex F)
21 GRAPHIC RENDITION COMBINATION MODE (GRCM)
22 ZERO DEFAULT MODE (ZDM) (see F.4.2 in annex F)
NOTE Private modes may be implemented using private parameters, see 5.4.1 and
7.4.
*/
void ecma48_RM(){
  ecma48_PRINT_CONTROL_SEQUENCE("RM");
  int Pn = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0], NULL, 10) : 0;
  switch(Pn){
    case 1: modes.GATM = 0; break;
    case 2: modes.KAM = 0; break;
    case 3: modes.CRM = 0; break;
    case 4: modes.IRM = 0; break;
    case 5: modes.SRTM = 0; break;
    case 6: modes.ERM = 0; break;
    case 7: modes.VEM = 0; break;
    case 8: modes.BDSM = 0; break;
    case 9: modes.DCSM = 0; break;
    case 10: modes.HEM = 0; break;
    case 11: modes.PUM = 0; break;
    case 12: modes.SRM = 0; break;
    case 13: modes.FEAM = 0; break;
    case 14: modes.FETM = 0; break;
    case 15: modes.MATM = 0; break;
    case 16: modes.TTM = 0; break;
    case 17: modes.SATM = 0; break;
    case 18: modes.TSM = 0; break;
    case 20: modes.LNM = 0; break;
    case 21: modes.GRCM = 0; break;
    case 22: modes.ZDM = 0; break;
  }
  ecma48_end_control();
}

/*
SGR - SELECT GRAPHIC RENDITION
Notation: (Ps...)
Representation: CSI Ps... 06/13
Parameter default value: Ps = 0
SGR is used to establish one or more graphic rendition aspects for subsequent
text. The established aspects remain in effect until the next occurrence of SGR
in the data stream, depending on the setting of the GRAPHIC RENDITION
COMBINATION MODE (GRCM). Each graphic rendition aspect is specified by a
parameter value:
0 default rendition (implementation-defined), cancels the effect of any
preceding occurrence of SGR in the data stream regardless of the setting of the
GRAPHIC RENDITION COMBINATION MODE (GRCM)
1 bold or increased intensity
2 faint, decreased intensity or second colour
3 italicized
4 singly underlined
5 slowly blinking (less then 150 per minute)
6 rapidly blinking (150 per minute or more)
7 negative image
8 concealed characters
9 crossed-out (characters still legible but marked as to be deleted)
10 primary (default) font
11 first alternative font
12 second alternative font
13 third alternative font
14 fourth alternative font
15 fifth alternative font
16 sixth alternative font
17 seventh alternative font
18 eighth alternative font
19 ninth alternative font
20 Fraktur (Gothic)
21 doubly underlined
22 normal colour or normal intensity (neither bold nor faint)
23 not italicized, not fraktur
24 not underlined (neither singly nor doubly)
25 steady (not blinking)
26 (reserved for proportional spacing as specified in CCITT Recommendation T.61)
27 positive image
28 revealed characters
29 not crossed out
30 black display
31 red display
32 green display
33 yellow display
34 blue display
35 magenta display
36 cyan display
37 white display
38 (reserved for future standardization; intended for setting character
foreground colour as specified in ISO 8613-6 [CCITT Recommendation T.416])
39 default display colour (implementation-defined)
40 black background
41 red background
42 green background
43 yellow background
44 blue background
45 magenta background
46 cyan background
47 white background
48 (reserved for future standardization; intended for setting character
background colour as specified in ISO 8613-6 [CCITT Recommendation T.416])
49 default background colour (implementation-defined)
50 (reserved for cancelling the effect of the rendering aspect established by
parameter value 26)
51 framed
52 encircled
53 overlined
54 not framed, not encircled
55 not overlined
56 (reserved for future standardization)
57 (reserved for future standardization)
58 (reserved for future standardization)
59 (reserved for future standardization)
60 ideogram underline or right side line
61 ideogram double underline or double line on the right side
62 ideogram overline or left side line
63 ideogram double overline or double line on the left side
64 ideogram stress marking
65 cancels the effect of the rendition aspects established by parameter values
60 to 64
NOTE The usable combinations of parameter values are determined by the
implementation.
*/
void ecma48_SGR(){
  ecma48_PRINT_CONTROL_SEQUENCE("SGR");
  int Pn[NUM_ESCAPE_ARGS];
  SDL_Color tmpcolor;
  int i;

  // set the default
  Pn[0] = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0], NULL, 10) : 0;
  // now process anything else
  for(i=1; i < NUM_ESCAPE_ARGS; ++i){
    Pn[i] = escape_args.args[i][0] != '\0' ? (int)strtol(escape_args.args[i], NULL, 10) : -1;
  }
  for(i=0; i < NUM_ESCAPE_ARGS; ++i){
    if(Pn[i] >= 0){
      //fprintf(stderr, "SGR: %u\n", Pn[i]);
      switch (Pn[i]){
        case 0: // 0 default rendition
          current_style = default_text_style;
          break;
        case 1: // 1 bold or increased intensity
          current_style.style |= TTF_STYLE_BOLD;
          break;
        case 2: // 2 faint, decreased intensity or second colour
          break;
        case 3: // 3 italicized
          current_style.style |= TTF_STYLE_ITALIC;
          break;
        case 4: // 4 singly underlined
          current_style.style |= TTF_STYLE_UNDERLINE;
          break;
        case 5: // 5 slowly blinking (less then 150 per minute)
          break;
        case 6: // 6 rapidly blinking (150 per minute or more)
          break;
        case 7: // 7 negative image
          ecma48_reverse_video();
          break;
        case 8: // 8 concealed characters
          break;
        case 9: // 9 crossed-out
          current_style.style |= TTF_STYLE_STRIKETHROUGH;
          break;
        case 10: // 10 primary (default) font
          ecma48_SI();
          break;
        case 11: // 11 first alternative font
          break;
        case 12: // 12 second alternative font
          ecma48_SO();
          break;
        case 13: // 13 third alternative font
          break;
        case 14: // 14 fourth alternative font
          break;
        case 15: // 15 fifth alternative font
          break;
        case 16: // 16 sixth alternative font
          break;
        case 17: // 17 seventh alternative font
          break;
        case 18: // 18 eighth alternative font
          break;
        case 19: // 19 ninth alternative font
          break;
        case 20: // 20 Fraktur (Gothic)
          break;
        case 21: // 21 doubly underlined
          break;
        case 22: // 22 normal colour or normal intensity (neither bold nor faint)
          current_style.style ^= TTF_STYLE_BOLD;
          break;
        case 23: // 23 not italicized, not fraktur
          current_style.style ^= TTF_STYLE_ITALIC;
          break;
        case 24: // 24 not underlined (neither singly nor doubly)
          current_style.style ^= TTF_STYLE_UNDERLINE;
          break;
        case 25: // 25 steady (not blinking)
          break;
        case 26: break;
        case 27: // 27 positive image
          ecma48_normal_video();
          break;
        case 28: // 28 revealed characters
          break;
        case 29: // 29 not crossed out
          current_style.style ^= TTF_STYLE_STRIKETHROUGH;
          break;
        case 30: // 30 black display [0,0,0] b [127,127,127]
          current_style.fg_color = (SDL_Color)BLACK;
          break;
        case 31: // 31 red display [205,0,0] b [255,0,0]
          current_style.fg_color = (SDL_Color)RED;
          break;
        case 32: // 32 green display [0,205,0] b [0,255,0]
          current_style.fg_color = (SDL_Color)GREEN;
          break;
        case 33: // 33 yellow display [205,205,0] b [255,255,0]
          current_style.fg_color = (SDL_Color)YELLOW;
          break;
        case 34: // 34 blue display [0,0,238] b [92,92,255]
          current_style.fg_color = (SDL_Color)BLUE;
          break;
        case 35: // 35 magenta display [205,0,205] b [255,0,255]
          current_style.fg_color = (SDL_Color)MAGENTA;
          break;
        case 36: // 36 cyan display [0,205,205] b [0,255,255]
          current_style.fg_color = (SDL_Color)CYAN;
          break;
        case 37: // 37 white display [229,229,229] b [255,255,255]
          current_style.fg_color = (SDL_Color)WHITE;
          break;
        case 38: break;
        case 39: // 39 default display colour [255,255,255]
          current_style.fg_color = default_text_style.fg_color;
          break;
        case 40: // 40 black background [0,0,0]
          current_style.bg_color = (SDL_Color)BLACK;
          break;
        case 41: // 41 red background [205,0,0]
          current_style.bg_color = (SDL_Color)RED;
          break;
        case 42: // 42 green background [0,205,0]
          current_style.bg_color = (SDL_Color)GREEN;
          break;
        case 43: // 43 yellow background [205,205,0]
          current_style.bg_color = (SDL_Color)YELLOW;
          break;
        case 44: // 44 blue background [0,0,238]
          current_style.bg_color = (SDL_Color)BLUE;
          break;
        case 45: // 45 magenta background [205,0,205]
          current_style.bg_color = (SDL_Color)MAGENTA;
          break;
        case 46: // 46 cyan background [0,205,205]
          current_style.bg_color = (SDL_Color)CYAN;
          break;
        case 47: // 47 white background [255,225,255]
          current_style.bg_color = (SDL_Color)WHITE;
          break;
        case 48: break;
        case 49: // 49 default background colour
          current_style.bg_color = default_text_style.bg_color;
          break;
        case 50: break;
        case 51: // 51 framed
          break;
        case 52: // 52 encircled
          break;
        case 53: // 53 overlined
          break;
        case 54: // 54 not framed, not encircled
          break;
        case 55: // 55 not overlined
          break;
        case 56: break;
        case 57: break;
        case 58: break;
        case 59: break;
        case 60: // 60 ideogram underline or right side line
          break;
        case 61: // 61 ideogram double underline or double line on the right side
          break;
        case 62: // 62 ideogram overline or left side line
          break;
        case 63: // 63 ideogram double overline or double line on the left side
          break;
        case 64: // 64 ideogram stress marking
          break;
        case 65: // 65 cancels the effect parameter values 60 to 64
          break;
      };
    }
  }
  ecma48_end_control();
}

/*
DSR - DEVICE STATUS REPORT
Notation: (Ps)
Representation: CSI Ps 06/14
Parameter default value: Ps = 0
DSR is used either to report the status of the sending device or to request a
status report from the receiving device, depending on the parameter values:
0 ready, no malfunction detected
1 busy, another DSR must be requested later
2 busy, another DSR will be sent later
3 some malfunction detected, another DSR must be requested later
4 some malfunction detected, another DSR will be sent later
5 a DSR is requested
6 a report of the active presentation position or of the active data position in
the form of ACTIVE POSITION REPORT (CPR) is requested
DSR with parameter value 0, 1, 2, 3 or 4 may be sent either unsolicited or as a
response to a request such as a DSR with a parameter value 5 or MESSAGE WAITING
(MW).
*/
void ecma48_DSR(){
  ecma48_NOT_IMPLEMENTED("DSR");
}

/*
DAQ - DEFINE AREA QUALIFICATION
Notation: (Ps...)
Representation: CSI Ps... 06/15
Parameter default value: Ps = 0
DAQ is used to indicate that the active presentation position in the
presentation component is the first character position of a qualified area. The
last character position of the qualified area is the character position in the
presentation component immediately preceding the first character position of the
following qualified area.
The parameter value designates the type of qualified area:
0 unprotected and unguarded
1 protected and guarded
2 graphic character input
3 numeric input
4 alphabetic input
5 input aligned on the last character position of the qualified area
6 fill with ZEROs
7 set a character tabulation stop at the active presentation position (the first
character position of the qualified area) to indicate the beginning of a field
8 protected and unguarded
9 fill with SPACEs
10 input aligned on the first character position of the qualified area
11 the order of the character positions in the input field is reversed, i.e. the
last position in each line becomes the first and vice versa; input begins at the
new first position.
- 38 -
This control function operates independently of the setting of the TABULATION
STOP MODE (TSM). The character tabulation stop set by parameter value 7 applies
to the active line only.
NOTE The control functions for area definition (DAQ, EPA, ESA, SPA, SSA) should
not be used within an SRS string or an SDS string.
*/
void ecma48_DAQ(){
  ecma48_NOT_IMPLEMENTED("DAQ");
}

void ecma48_UNRECOGNIZED_CONTROL(char terminator){
  fprintf(stderr, "** UNRECOGNIZED control sequence ESC %s;%s;%s;%s;%s;%s;%s;%s;%s;%s %c (0x%x) - NumArgs=%d, state=%d\n",
                  escape_args.args[0],
                  escape_args.args[1],
                  escape_args.args[2],
                  escape_args.args[3],
                  escape_args.args[4],
                  escape_args.args[5],
                  escape_args.args[6],
                  escape_args.args[7],
                  escape_args.args[8],
                  escape_args.args[9],
                  terminator,
                  terminator,
                  escape_args.num + 1,
                  state);
  // and exit escape mode
  ecma48_end_control();
}

void ecma48_ANSI(){
  state = ECMA48_STATE_ANSI;
}

/* ansi_SM (set mode)
 * Handles all of the ESC[? Pn h codes
 */
void ansi_SM(){
  ecma48_PRINT_CONTROL_SEQUENCE("ansi_SM");
  int Pn = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0], NULL, 10) : 1;
  switch(Pn){
    case 1:  break; // DECCKM ignored
    //case 2:  break; // DECANM
  	case 3:  ecma48_clear_display(); // Clear the screen and set 132 chars
  	         ecma48_set_cursor_home();
  	         set_screen_cols(132);
  	         break;
    case 4:  break; // DECSCLM ignored
    case 5:  ecma48_reverse_video(); break; // DECSCNM
  	case 6:  modes.DECOM = 1; ecma48_set_cursor_home();break;// DECOM Set origin relative
    case 7:  autowrap = 1; break;
    case 8:  break; // DECARM ignored
    //case 12: break;
    case 25: draw_cursor = 1; break;
    //case 40: break; /* Enable 80/132 switch (xterm) */
    //case 45: break; /* Enable reverse wrap (xterm) */
    default: fprintf(stderr, "-- Unhandled code in ansi_SM: %d\n", Pn); break;
  };
  ecma48_end_control();
}

/* ansi_RM (reset mode)
 * Handles all of the ESC[? Pn l codes
 */
void ansi_RM(){
  ecma48_PRINT_CONTROL_SEQUENCE("ansi_RM");
  int Pn = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0], NULL, 10) : 1;
  switch(Pn){
    case 1:  break; // DECCKM ignored
  	case 3:  ecma48_clear_display(); // Clear the screen and set 80 chars
  	         ecma48_set_cursor_home();
             set_screen_cols(80);
  	         break;
    case 4:  break; // DECSCLM ignored
    case 5:  ecma48_normal_video(); break; // DECSCNM
  	case 6:  modes.DECOM = 0; ecma48_set_cursor_home(); break; // DECOM Set origin absolute
    case 7:  autowrap = 0; break;
    case 8:  break; // DECARM ignored
    //case 12: break; // comes after \E?25h for ansi_SM
    case 25: draw_cursor = 0; break;
    //case 40: break; /* Disable 80/132 switch (xterm) */
    //case 45: break; /* Disable reverse wrap (xterm) */
    default: fprintf(stderr, "-- Unhandled code in ansi_RM: %d\n", Pn); break;
  };
  ecma48_end_control();
}
void ansi_CSR(){
  ecma48_PRINT_CONTROL_SEQUENCE("CSR");
  int Pn = escape_args.args[0][0] != '\0' ? (int)strtol(escape_args.args[0], NULL, 10) : 1;
  int Pn2 = escape_args.args[1][0] != '\0' ? (int)strtol(escape_args.args[1],NULL, 10) : rows;
  if ((Pn2 > Pn) && (Pn >= 1) && (Pn2 <= rows)){
    sr.top = Pn;
    sr.bottom = Pn2;
  }
  ecma48_set_cursor_home();
  ecma48_end_control();
}

void ansi_CUU(){
  ecma48_PRINT_CONTROL_SEQUENCE("ansi_CUU");
  ecma48_CUU();
}

void ansi_CUD(){
  ecma48_PRINT_CONTROL_SEQUENCE("ansi_CUD");
  ecma48_CUD();
}

void ansi_POUND(){
  state = ECMA48_STATE_ANSI_POUND;
}

void ansi_FUNCKEY(){
  ecma48_NOT_IMPLEMENTED("FUNCKEY");
}

void xterm_SC(){
  ecma48_NOT_IMPLEMENTED("SC");
}

void xterm_RC(){
  ecma48_NOT_IMPLEMENTED("RC");
}

/* Fill the screen with 'E', and set the cursor to the home position */
void ansi_DECALN(){

  ecma48_PRINT_CONTROL_SEQUENCE("ansi_DECALN");
  /* hard set the cursor back to the zero position */
  buf.top_line = 0;
  buf.line = 0;
  buf.col = 0;
	int x, y;
	struct screenchar *sc;
	UChar pattern = 'E';
	for(x = 0; x < cols; ++x){
		for(y = 0; y < rows; ++y){
			sc = &(buf.text[y][x]);
			/* free old char */
			buf_free_char(sc);
			/* write new one */
			sc->c = pattern;
			sc->style = current_style;
		}
	}
	buf.top_line = 0;
	buf.line = 0;
	buf.col = 0;

	ecma48_end_control();
}

void ecma48_filter_text(UChar* tbuf, ssize_t chars){

  ssize_t i;

  PRINT(stderr, "Got characters:");
  for(i = 0; i < chars; ++i){
    PRINT(stderr, "%x:", tbuf[i]);

    switch(state){
      case ECMA48_STATE_NORMAL:
        switch(tbuf[i]){
          case 0x00: ecma48_NUL(); break;
          case 0x01: ecma48_SOH(); break;
          case 0x02: ecma48_STX(); break;
          case 0x03: ecma48_ETX(); break;
          case 0x04: ecma48_EOT(); break;
          case 0x05: ecma48_ENQ(); break;
          case 0x06: ecma48_ACK(); break;
          case 0x07: ecma48_BEL(); break;
          case 0x08: ecma48_BS(); break;
          case 0x09: ecma48_HT(); break;
          case 0x0a: ecma48_LF(); break;
          case 0x0b: ecma48_VT(); break;
          case 0x0c: ecma48_FF(); break;
          case 0x0d: ecma48_CR(); break;
          case 0x0e: ecma48_SO(); break;
          case 0x0f: ecma48_SI(); break;
          case 0x10: ecma48_DLE(); break;
          case 0x11: ecma48_DC1(); break;
          case 0x12: ecma48_DC2(); break;
          case 0x13: ecma48_DC3(); break;
          case 0x14: ecma48_DC4(); break;
          case 0x15: ecma48_NAK(); break;
          case 0x16: ecma48_SYN(); break;
          case 0x17: ecma48_ETB(); break;
          case 0x18: ecma48_CAN(); break;
          case 0x19: ecma48_EM(); break;
          case 0x1a: ecma48_SUB(); break;
          case 0x1b: ecma48_ESC(); break;
          case 0x1c: ecma48_IS4(); break;
          case 0x1d: ecma48_IS3(); break;
          case 0x1e: ecma48_IS2(); break;
          case 0x1f: ecma48_IS1(); break;
          default: ecma48_add_char(tbuf[i]); break;
        }; break;
      case ECMA48_STATE_C1:
        switch(tbuf[i]){
          case 0x23: ansi_POUND(); break;
          case 0x37: xterm_SC(); break;
          case 0x38: xterm_RC(); break;
          case 0x41: ansi_CUU(); break;
          case 0x42: ecma48_BPH(); break;
          case 0x43: ecma48_NBH(); break;
          case 0x44: ansi_CUD(); break; // VT100 IND (CUD), VT52 CUB
          case 0x45: ecma48_NEL(); break;
          case 0x46: ecma48_SSA(); break;
          case 0x47: ecma48_ESA(); break;
          case 0x48: ecma48_HTS(); break;
          case 0x49: ecma48_HTJ(); break;
          case 0x4a: ecma48_VTS(); break;
          case 0x4b: ecma48_PLD(); break;
          case 0x4c: ecma48_PLU(); break;
          case 0x4d: ecma48_RI(); break;
          case 0x4e: ecma48_SS2(); break;
          case 0x4f: ecma48_SS3(); break;
          case 0x50: ecma48_DCS(); break;
          case 0x51: ecma48_PU1(); break;
          case 0x52: ecma48_PU2(); break;
          case 0x53: ecma48_STS(); break;
          case 0x54: ecma48_CCH(); break;
          case 0x55: ecma48_MW(); break;
          case 0x56: ecma48_SPA(); break;
          case 0x57: ecma48_EPA(); break;
          case 0x58: ecma48_SOS(); break;
          case 0x5a: ecma48_SCI(); break;
          case 0x5b: ecma48_CSI(); break;
          case 0x5c: ecma48_ST(); break;
          case 0x5d: ecma48_OSC(); break;
          case 0x5e: ecma48_PM(); break;
          case 0x5f: ecma48_APC(); break;
          /* independent control functions here */
          case 0x60: ecma48_DMI(); break;
          case 0x61: ecma48_INT(); break;
          case 0x62: ecma48_EMI(); break;
          case 0x63: ecma48_RIS(); break;
          case 0x64: ecma48_CMD(); break;
          case 0x6e: ecma48_LS2(); break;
          case 0x6f: ecma48_LS3(); break;
          case 0x7c: ecma48_LS3R(); break;
          case 0x7d: ecma48_LS2R(); break;
          case 0x7e: ecma48_LS1R(); break;
          default: ecma48_UNRECOGNIZED_CONTROL(tbuf[i]); break;
        }; break;
      case ECMA48_STATE_CSI:
        switch(tbuf[i]){
          case 0x08: ecma48_BS_INTER(); break;
          case 0x09: ecma48_HT_INTER(); break;
          case 0x0a: ecma48_LF_INTER(); break;
          case 0x0b: ecma48_VT_INTER(); break;
          case 0x0d: ecma48_CR_INTER(); break;
          /* intermediate bytes */
          case 0x20:
          case 0x21:
          case 0x22:
          case 0x23:
          case 0x24:
          case 0x25:
          case 0x26:
          case 0x27:
          case 0x28:
          case 0x29:
          case 0x2a:
          case 0x2b:
          case 0x2c:
          case 0x2d:
          case 0x2e:
          case 0x2f: ecma48_UNRECOGNIZED_CONTROL(tbuf[i]); break;
          /* parameter bytes */
          case 0x30:
          case 0x31:
          case 0x32:
          case 0x33:
          case 0x34:
          case 0x35:
          case 0x36:
          case 0x37:
          case 0x38:
          case 0x39:
          case 0x3a: ecma48_parameter_arg_add((char)tbuf[i]); break;
          case 0x3b: ecma48_parameter_arg_next(); break;
          /* ANSI compatibility */
          case 0x3f: ecma48_ANSI(); break;
          /* final bytes */
          case 0x40: ecma48_ICH(); break;
          case 0x41: ecma48_CUU(); break;
          case 0x42: ecma48_CUD(); break;
          case 0x43: ecma48_CUF(); break;
          case 0x44: ecma48_CUB(); break;
          case 0x45: ecma48_CNL(); break;
          case 0x46: ecma48_CPL(); break;
          case 0x47: ecma48_CHA(); break;
          case 0x48: ecma48_CUP(); break;
          case 0x49: ecma48_CHT(); break;
          case 0x4a: ecma48_ED(); break;
          case 0x4b: ecma48_EL(); break;
          case 0x4c: ecma48_IL(); break;
          case 0x4d: ecma48_DL(); break;
          case 0x4e: ecma48_EF(); break;
          case 0x4f: ecma48_EA(); break;
          case 0x50: ecma48_DCH(); break;
          case 0x51: ecma48_SEE(); break;
          case 0x52: ecma48_CPR(); break;
          case 0x53: ecma48_SU(); break;
          case 0x54: ecma48_SD(); break;
          case 0x55: ecma48_NP(); break;
          case 0x56: ecma48_PP(); break;
          case 0x57: ecma48_CTC(); break;
          case 0x58: ecma48_ECH(); break;
          case 0x59: ecma48_CVT(); break;
          case 0x5a: ecma48_CBT(); break;
          case 0x5b: ecma48_SRS(); break;
          case 0x5c: ecma48_PTX(); break;
          case 0x5d: ecma48_SDS(); break;
          case 0x5e: ecma48_SIMD(); break;
          case 0x60: ecma48_HPA(); break;
          case 0x61: ecma48_HPR(); break;
          case 0x62: ecma48_REP(); break;
          case 0x63: ecma48_DA(); break;
          case 0x64: ecma48_VPA(); break;
          case 0x65: ecma48_VPR(); break;
          case 0x66: ecma48_HVP(); break;
          case 0x67: ecma48_TBC(); break;
          case 0x68: ecma48_SM(); break;
          case 0x69: ecma48_MC(); break;
          case 0x6a: ecma48_HPB(); break;
          case 0x6b: ecma48_VPB(); break;
          case 0x6c: ecma48_RM(); break;
          case 0x6d: ecma48_SGR(); break;
          case 0x6e: ecma48_DSR(); break;
          case 0x6f: ecma48_DAQ(); break;
          case 0x72: ansi_CSR(); break;
          case 0x7e: ansi_FUNCKEY(); break;
          default: ecma48_UNRECOGNIZED_CONTROL(tbuf[i]); break;
        }; break;
      case ECMA48_STATE_ANSI:
        switch(tbuf[i]){
          /* parameter bytes */
          case 0x30:
          case 0x31:
          case 0x32:
          case 0x33:
          case 0x34:
          case 0x35:
          case 0x36:
          case 0x37:
          case 0x38:
          case 0x39:
          case 0x3a: ecma48_parameter_arg_add((char)tbuf[i]); break;
          case 0x3b: ecma48_parameter_arg_next(); break;
          /* final bytes */
          case 0x68: ansi_SM(); break;
          case 0x6c: ansi_RM(); break;
          default: ecma48_UNRECOGNIZED_CONTROL(tbuf[i]); break;
        }; break;
      case ECMA48_STATE_ANSI_POUND:
        switch(tbuf[i]){
          case 0x38: ansi_DECALN(); break;
          default: ecma48_UNRECOGNIZED_CONTROL(tbuf[i]); break;
        }; break;
    }
  }
  PRINT(stderr, "\n");
}


















