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

#ifndef IO_H_
#define IO_H_

int io_init();
void io_uninit();
void io_set_master(int master_fd);
int  io_get_master();
ssize_t io_write_master(const UChar *buf, size_t nUChar);
ssize_t io_read_master(UChar *buf, size_t nUChar);
/* output is stored in the UChar buf, which must be of size utf8len */
ssize_t io_read_utf8_string(const char* utf8, size_t utf8len, UChar* buf);
void io_paste_from_clipboard();

#endif /* IO_H_ */
