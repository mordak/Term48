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
#include <unicode/utf.h>
#include <unicode/ucnv.h>
#include <unicode/ustring.h>
#include <clipboard/clipboard.h>


#include "terminal.h"
#include "preferences.h"

#include "io.h"

static int master_fd;
static UConverter* tty_conv;
static UErrorCode  tty_conv_err = U_ZERO_ERROR;

static UConverter* utf8_conv;
static UErrorCode  utf8_conv_err = U_ZERO_ERROR;

static char readbuf[READ_BUFFER_SIZE];
static char writebuf[CHARACTER_BUFFER * U8_MAX_LENGTH];
static char* writebufLimit;

int io_init(){

	// create converters
  tty_conv  = ucnv_open(preferences_get_string(preference_keys.tty_encoding), &tty_conv_err);
  utf8_conv  = ucnv_open("UTF-8", &utf8_conv_err);
	return TERM_SUCCESS;
}

void io_uninit(){

	// free converts
	ucnv_close(tty_conv);
	ucnv_close(utf8_conv);
  close(master_fd);
}

void io_set_master(int fd){
	master_fd = fd;
}

int io_get_master(){
	return master_fd;
}

/* The last thing we wrote is still in writebuf.
 * This function will set buf to the character _after_ the
 * last output character, which lets the caller find the
 * upcase length. The return value is the length of the
 * last written string, which tells the caller how many
 * backspaces to send in order to erase it.
 * A return value of 0 indicates no upcase representation,
 * or an error occurred. */
int32_t io_upcase_last_write(UChar **buf, int32_t nUChar){
	UChar c[CHARACTER_BUFFER];
	UChar *target;
  UChar *targetLimit;
	const char *source;
	const char *sourceLimit;
  int32_t uCaseLen;
  int32_t lCaseLen;
  UErrorCode uCaseErr = U_ZERO_ERROR;

	/* Convert from output encoding to UChar first, then upcase */
	target = c;
	targetLimit = c + CHARACTER_BUFFER;
	source = writebuf;
	sourceLimit = writebufLimit;
	ucnv_toUnicode(tty_conv, &target, targetLimit, &source, sourceLimit, NULL, FALSE, &tty_conv_err);
	if(tty_conv_err == U_BUFFER_OVERFLOW_ERROR){
		fprintf(stderr, "ucnv_toUnicode() in io_upcase_last_write ran out of target buffer\n");
		tty_conv_err = U_ZERO_ERROR;
	}
	lCaseLen = (int32_t)(target - c);
	uCaseLen = u_strToUpper(*buf, nUChar, c, lCaseLen, NULL, &uCaseErr);
	if (uCaseLen > nUChar){
		// output was truncated
		uCaseLen = nUChar;
	}
	/* If we got something */ //, and it isn't the same as before */
	if ((uCaseLen > 0)     ){ // && (u_strCompare(*buf, uCaseLen, c, lCaseLen, TRUE) != 0)){
		// point _after_ the last UChar
		*buf = *buf + uCaseLen;
		return lCaseLen;
	}
	// no upcase value / error / no dice..
	return 0;
}

ssize_t io_write_master(const UChar *buf, size_t nUChar){

	char* target = writebuf;
	char* targetLimit = writebuf + (CHARACTER_BUFFER * U8_MAX_LENGTH);
	const UChar* source = buf;
	const UChar* sourceLimit = buf + nUChar;

	ucnv_fromUnicode(tty_conv, &target, targetLimit, &source, sourceLimit, NULL, TRUE, &tty_conv_err);

  if(tty_conv_err == U_BUFFER_OVERFLOW_ERROR){
  	fprintf(stderr, "ucnv_fromUnicode() in io_write_master ran out of target buffer\n");
  	tty_conv_err = U_ZERO_ERROR;
  }

  writebufLimit = target;
	return write(master_fd, writebuf, (size_t)(target - writebuf));
}

ssize_t io_write_master_char(const char *buf, size_t n){
  return write(master_fd, buf, n);
}

ssize_t io_read_master(UChar *buf, size_t nUChar){
	const char *source;
	const char *sourceLimit;
	int32_t count;
  UChar *target;
  UChar *targetLimit;

  /* Read nUChar bytes, which necessarily translates into <= nUChar UChars */
	count = read(master_fd, readbuf, nUChar);
	if(count <= 0){
		return count;
	}
	// else

	source = readbuf;
	sourceLimit = readbuf + count;

	target = buf;
	targetLimit = buf + nUChar;

  ucnv_toUnicode(tty_conv, &target, targetLimit, &source, sourceLimit, NULL, FALSE, &tty_conv_err);

  if(tty_conv_err == U_BUFFER_OVERFLOW_ERROR){
  	fprintf(stderr, "ucnv_toUnicode() in io_read_master ran out of target buffer\n");
  	tty_conv_err = U_ZERO_ERROR;
  }

  return (ssize_t)(target - buf);
}

ssize_t io_read_utf8_string(const char* utf8, size_t utf8len, UChar* buf){

	const char *source;
	const char *sourceLimit;
  UChar *target;
  UChar *targetLimit;


	source = utf8;
	sourceLimit = utf8 + utf8len;

	target = buf;
	targetLimit = buf + utf8len;

  ucnv_toUnicode(utf8_conv, &target, targetLimit, &source, sourceLimit, NULL, TRUE, &utf8_conv_err);

  if(utf8_conv_err == U_BUFFER_OVERFLOW_ERROR){
  	fprintf(stderr, "ucnv_toUnicode() in io_read_utf8_string ran out of target buffer when converting from utf8\n");
  	utf8_conv_err = U_ZERO_ERROR;
  }

  return (ssize_t)(target - buf);
}

void io_paste_from_clipboard(){
  char* buffer = NULL;
  int ret;
  if(is_clipboard_format_present("text/plain") == 0){
    ret = get_clipboard_data("text/plain", &buffer);
    if(ret > 0 && buffer != NULL){
    	/* Paste and Pray. The encoding of the byte stream will be whatever
    	 * the copied source was - hopefully tty_encoding..
    	 */
      write(master_fd, buffer, ret * sizeof(char));
      free(buffer);
    }
  }
}
