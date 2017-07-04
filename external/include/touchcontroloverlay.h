/*
 * Copyright (c) 2011 Research In Motion Limited.
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

#ifndef _TOUCHCONTROLOVERLAY_H_INCLUDED
#define _TOUCHCONTROLOVERLAY_H_INCLUDED

#include <sys/platform.h>
#include <screen/screen.h>

__BEGIN_DECLS

/**
 * tco library functions that may fail
 * return \c TCO_SUCCESS on success.
 */
#define TCO_SUCCESS (0)

/**
 * tco library functions that may fail return
 * \c TCO_FAILURE on error and
 * may set errno.
 */
#define TCO_FAILURE (-1)

#define TCO_UNHANDLED (2)

/**
 * File version number
 */
#define TCO_FILE_VERSION 1

struct tco_callbacks {
	int (*handleKeyFunc)(int sym, int mod, int scancode, uint16_t unicode, int event);
	int (*handleDPadFunc)(int angle, int event);
	int (*handleTouchFunc)(int dx, int dy);
	int (*handleMouseButtonFunc)(int button, int mask, int event); // TODO: Unify keyboard mod with mouse mask
	int (*handleTapFunc)();
	int (*handleTouchScreenFunc)(int x, int y, int tap, int hold);
};

enum EmuKeyButtonState {
	TCO_KB_DOWN = 0,
	TCO_KB_UP = 1
};

enum EmuMouseButton {
	TCO_MOUSE_LEFT_BUTTON = 0,
	TCO_MOUSE_RIGHT_BUTTON = 1,
	TCO_MOUSE_MIDDLE_BUTTON = 2
};

enum EmuKeyMask {
	TCO_SHIFT = 0x1,
	TCO_CTRL = 0x2,
	TCO_ALT = 0x4
};

enum EmuMouseButtonState {
	TCO_MOUSE_BUTTON_DOWN = 0,
	TCO_MOUSE_BUTTON_UP = 1
};

typedef void * tco_context_t;
/**
 * Initialize the touch control overlay layer and context.
 */
int tco_initialize(tco_context_t *context, screen_context_t screenContext, struct tco_callbacks callbacks);

/**
 * Load the controls from a file.
 */
int tco_loadcontrols(tco_context_t context, const char* filename);

/**
 * Load default controls as a fallback.
 */
int tco_loadcontrols_default(tco_context_t context);

/**
 * Show configuration window
 * NOTE: the window MUST have a window group set already.
 */
int tco_swipedown(tco_context_t context, screen_window_t window);

/**
 * Show overlay labels
 */
int tco_showlabels(tco_context_t context, screen_window_t window);

/**
 * Provide touch events
 */
int tco_touch(tco_context_t context, screen_event_t event);

/**
 * Cleanup and shutdown
 */
void tco_shutdown(tco_context_t context);

__END_DECLS

#endif /* _TOUCHCONTROLOVERLAY_H_INCLUDED */
