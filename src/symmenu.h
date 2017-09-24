#ifndef SYMMENU_H_
#define SYMMENU_H_


#include <sys/keycodes.h>
#include <libconfig.h>
#include <unicode/utf.h>
#include <errno.h>

#include "types.h"
#include "SDL.h"
#include "preferences.h"

SDL_Surface *render_symmenu(SDL_Surface *screen, pref_t *prefs, symmenu_t *menu);
void destroy_symmenu(symmenu_t *menu);

#endif
