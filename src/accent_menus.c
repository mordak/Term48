#include <unicode/utf.h>
#include "types.h"

const int lowercase_accent_row_lens[] = {  \
	/* a */ 1, /* b */ 1, /* c */ 1, /* d */ 1, /* e */ 5, /* f */ 1, \
	/* g */ 1, /* h */ 1, /* i */ 1, /* j */ 1, /* k */ 1, /* l */ 1, \
	/* m */ 1, /* n */ 1, /* o */ 1, /* p */ 1, /* q */ 1, /* r */ 1, \
	/* s */ 1, /* t */ 1, /* u */ 1, /* v */ 1, /* w */ 1, /* x */ 1, \
	/* y */ 1, /* z */ 1 };

const int uppercase_accent_row_lens[] = { \
	/* a */ 1, /* b */ 1, /* c */ 1, /* d */ 1, /* e */ 1, /* f */ 1, \
	/* g */ 1, /* h */ 1, /* i */ 1, /* j */ 1, /* k */ 1, /* l */ 1, \
	/* m */ 1, /* n */ 1, /* o */ 1, /* p */ 1, /* q */ 1, /* r */ 1, \
	/* s */ 1, /* t */ 1, /* u */ 1, /* v */ 1, /* w */ 1, /* x */ 1, \
	/* y */ 1, /* z */ 1 };

keymap_t lc_a_entry[] = {{'q', "A"}};
keymap_t lc_b_entry[] = {{'q', "B"}};
keymap_t lc_c_entry[] = {{'q', "C"}};
keymap_t lc_d_entry[] = {{'q', "D"}};
keymap_t lc_e_entry[] = {{'q', "E"}, {'w', "\u00E9"}, {'e', "\u00E8"}, {'r', "\u00EA"}, {'t', "\u00EB"}};
keymap_t lc_f_entry[] = {{'q', "F"}};
keymap_t lc_g_entry[] = {{'q', "G"}};
keymap_t lc_h_entry[] = {{'q', "H"}};
keymap_t lc_i_entry[] = {{'q', "I"}};
keymap_t lc_j_entry[] = {{'q', "J"}};
keymap_t lc_k_entry[] = {{'q', "K"}};
keymap_t lc_l_entry[] = {{'q', "L"}};
keymap_t lc_m_entry[] = {{'q', "M"}};
keymap_t lc_n_entry[] = {{'q', "N"}};
keymap_t lc_o_entry[] = {{'q', "O"}};
keymap_t lc_p_entry[] = {{'q', "P"}};
keymap_t lc_q_entry[] = {{'q', "Q"}};
keymap_t lc_r_entry[] = {{'q', "R"}};
keymap_t lc_s_entry[] = {{'q', "S"}};
keymap_t lc_t_entry[] = {{'q', "T"}};
keymap_t lc_u_entry[] = {{'q', "U"}};
keymap_t lc_v_entry[] = {{'q', "V"}};
keymap_t lc_w_entry[] = {{'q', "W"}};
keymap_t lc_x_entry[] = {{'q', "X"}};
keymap_t lc_y_entry[] = {{'q', "Y"}};
keymap_t lc_z_entry[] = {{'q', "Z"}};

const keymap_t* lowercase_accent_entries[26] = {
	lc_a_entry, lc_b_entry, lc_c_entry, lc_d_entry, lc_e_entry, lc_f_entry, lc_g_entry,
	lc_h_entry, lc_i_entry, lc_j_entry, lc_k_entry, lc_l_entry, lc_m_entry, lc_n_entry,
	lc_o_entry, lc_p_entry, lc_q_entry, lc_r_entry, lc_s_entry, lc_t_entry, lc_u_entry,
	lc_v_entry, lc_w_entry, lc_x_entry, lc_y_entry, lc_z_entry
};
