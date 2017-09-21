#ifndef _TYPES_H
#define _TYPES_H

#include "SDL.h"

typedef struct _keymap_t {
	char from;
	char *to;
} keymap_t;

typedef struct _hitbox_t {
	int x, y, w, h;
} hitbox_t;

typedef struct _symkey_t {
	char flash;
	keymap_t *map; /* pointer to corresponding map */
	hitbox_t hitbox; /* used for mousedown */
	UChar *uc;
} symkey_t;

typedef struct _symmenu_t {
	/* row terminated by NULL pointer, col by NULL symkey_t->map */
	symkey_t **keys; 
	keymap_t *entries; /* terminated by NULL keymap_t.to */
	SDL_Surface *surface;
} symmenu_t;

typedef struct _pref_t {
	char *font_path;
	int font_size, *text_color, *background_color, screen_idle_awake,
		auto_show_vkb, metamode_doubletap_key, metamode_doubletap_delay,
		keyhold_actions, metamode_hold_key, allow_resize_columns;
	hitbox_t *metamode_hitbox;
	char *tty_encoding;
	
	/* terminated by NULL pointer */
	keymap_t *metamode_keys, *metamode_sticky_keys, *metamode_func_keys;
	
	symmenu_t *main_symmenu;
	symmenu_t *q_accent_menu, *w_accent_menu, *e_accent_menu, *r_accent_menu;
	symmenu_t *t_accent_menu, *y_accent_menu, *u_accent_menu, *i_accent_menu;
	symmenu_t *o_accent_menu, *p_accent_menu, *a_accent_menu, *s_accent_menu;
	symmenu_t *d_accent_menu, *f_accent_menu, *g_accent_menu, *h_accent_menu;
	symmenu_t *j_accent_menu, *k_accent_menu, *l_accent_menu, *z_accent_menu;
	symmenu_t *x_accent_menu, *c_accent_menu, *v_accent_menu, *b_accent_menu;
	symmenu_t *n_accent_menu, *m_accent_menu;
	
	int sticky_sym_key, sticky_shift_key, sticky_alt_key;
	int *keyhold_actions_exempt; /* terminated by -1 */
	int rescreen_for_symmenu, prefs_version;
} pref_t;

#endif
