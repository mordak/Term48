#include <unicode/utf.h>
#include "types.h"

const int accent_row_lens[] = {  \
	/* a */ 8, /* b */ 1, /* c */ 3, /* d */ 2, /* e */ 5, /* f */ 1, \
	/* g */ 1, /* h */ 1, /* i */ 5, /* j */ 1, /* k */ 1, /* l */ 1, \
	/* m */ 1, /* n */ 2, /* o */ 8, /* p */ 2, /* q */ 1, /* r */ 1, \
	/* s */ 3, /* t */ 3, /* u */ 5, /* v */ 1, /* w */ 1, /* x */ 1, \
	/* y */ 3, /* z */ 1 };

keymap_t lc_a_entry[] = {{'q', "A"}, {'w', "\u00E0"}, {'e', "\u00E1"}, {'r', "\u00E2"}, {'t', "\u00E3"}, {'y', "\u00E4"}, {'u', "\u00E5"}, {'i', "\u00E6"}};
keymap_t lc_b_entry[] = {{'q', "B"}};
keymap_t lc_c_entry[] = {{'q', "C"}, {'w', "\u00E7"}, {'e', "\u00A9"}};
keymap_t lc_d_entry[] = {{'q', "D"}, {'w', "\u00F0"}};
keymap_t lc_e_entry[] = {{'q', "E"}, {'w', "\u00E9"}, {'e', "\u00E8"}, {'r', "\u00EA"}, {'t', "\u00EB"}};
keymap_t lc_f_entry[] = {{'q', "F"}};
keymap_t lc_g_entry[] = {{'q', "G"}};
keymap_t lc_h_entry[] = {{'q', "H"}};
keymap_t lc_i_entry[] = {{'q', "I"}, {'w', "\u00EC"}, {'e', "\u00ED"}, {'r', "\u00EE"}, {'t', "\u00EF"}};
keymap_t lc_j_entry[] = {{'q', "J"}};
keymap_t lc_k_entry[] = {{'q', "K"}};
keymap_t lc_l_entry[] = {{'q', "L"}};
keymap_t lc_m_entry[] = {{'q', "M"}};
keymap_t lc_n_entry[] = {{'q', "N"}, {'w', "\u00F1"}};
keymap_t lc_o_entry[] = {{'q', "O"}, {'w', "\u00F2"}, {'e', "\u00F3"}, {'r', "\u00F4"}, {'t', "\u00F5"}, {'y', "\u00F6"}, {'u', "\u0153"}, {'i', "\u00F8"}};
keymap_t lc_p_entry[] = {{'q', "P"}, {'w', "\u00B6"}};
keymap_t lc_q_entry[] = {{'q', "Q"}};
keymap_t lc_r_entry[] = {{'q', "R"}};
keymap_t lc_s_entry[] = {{'q', "S"}, {'w', "\u00A7"}, {'e', "\u00DF"}};
keymap_t lc_t_entry[] = {{'q', "T"}, {'w', "\u00DE"}};
keymap_t lc_u_entry[] = {{'q', "U"}, {'w', "\u00F9"}, {'e', "\u00FA"}, {'r', "\u00FB"}, {'t', "\u00FC"}};
keymap_t lc_v_entry[] = {{'q', "V"}};
keymap_t lc_w_entry[] = {{'q', "W"}};
keymap_t lc_x_entry[] = {{'q', "X"}};
keymap_t lc_y_entry[] = {{'q', "Y"}, {'w', "\u00FD"}, {'e', "\u00FF"}};
keymap_t lc_z_entry[] = {{'q', "Z"}};

const keymap_t* lowercase_accent_entries[26] = {
	lc_a_entry, lc_b_entry, lc_c_entry, lc_d_entry, lc_e_entry, lc_f_entry, lc_g_entry,
	lc_h_entry, lc_i_entry, lc_j_entry, lc_k_entry, lc_l_entry, lc_m_entry, lc_n_entry,
	lc_o_entry, lc_p_entry, lc_q_entry, lc_r_entry, lc_s_entry, lc_t_entry, lc_u_entry,
	lc_v_entry, lc_w_entry, lc_x_entry, lc_y_entry, lc_z_entry
};

keymap_t uc_a_entry[] = {{'q', "A"}, {'w', "\u00C0"}, {'e', "\u00C1"}, {'r', "\u00C2"}, {'t', "\u00C3"}, {'y', "\u00C4"}, {'u', "\u00C5"}, {'i', "\u00C6"}};
keymap_t uc_b_entry[] = {{'q', "B"}};
keymap_t uc_c_entry[] = {{'q', "C"}, {'w', "\u00C7"}, {'e', "\u00A9"}};
keymap_t uc_d_entry[] = {{'q', "D"}, {'w', "\u00D0"}};
keymap_t uc_e_entry[] = {{'q', "E"}, {'w', "\u00C9"}, {'e', "\u00C8"}, {'r', "\u00CA"}, {'t', "\u00CB"}};
keymap_t uc_f_entry[] = {{'q', "F"}};
keymap_t uc_g_entry[] = {{'q', "G"}};
keymap_t uc_h_entry[] = {{'q', "H"}};
keymap_t uc_i_entry[] = {{'q', "I"}, {'w', "\u00CC"}, {'e', "\u00CD"}, {'r', "\u00CE"}, {'t', "\u00CF"}};
keymap_t uc_j_entry[] = {{'q', "J"}};
keymap_t uc_k_entry[] = {{'q', "K"}};
keymap_t uc_l_entry[] = {{'q', "L"}};
keymap_t uc_m_entry[] = {{'q', "M"}};
keymap_t uc_n_entry[] = {{'q', "N"}, {'w', "\u00D1"}};
keymap_t uc_o_entry[] = {{'q', "O"}, {'w', "\u00D2"}, {'e', "\u00D3"}, {'r', "\u00D4"}, {'t', "\u00D5"}, {'y', "\u00D6"}, {'u', "\u0152"}, {'i', "\u00D8"}};
keymap_t uc_p_entry[] = {{'q', "P"}, {'w', "\u00B6"}};
keymap_t uc_q_entry[] = {{'q', "Q"}};
keymap_t uc_r_entry[] = {{'q', "R"}};
keymap_t uc_s_entry[] = {{'q', "S"}, {'w', "\u00A7"}, {'e', "\u00DF"}};
keymap_t uc_t_entry[] = {{'q', "T"}, {'w', "\u00DE"}};
keymap_t uc_u_entry[] = {{'q', "U"}, {'w', "\u00D9"}, {'e', "\u00DA"}, {'r', "\u00DB"}, {'t', "\u00DC"}};
keymap_t uc_v_entry[] = {{'q', "V"}};
keymap_t uc_w_entry[] = {{'q', "W"}};
keymap_t uc_x_entry[] = {{'q', "X"}};
keymap_t uc_y_entry[] = {{'q', "Y"}, {'w', "\u00DD"}, {'e', "\u00DF"}};
keymap_t uc_z_entry[] = {{'q', "Z"}};

const keymap_t* uppercase_accent_entries[26] = {
	uc_a_entry, uc_b_entry, uc_c_entry, uc_d_entry, uc_e_entry, uc_f_entry, uc_g_entry,
	uc_h_entry, uc_i_entry, uc_j_entry, uc_k_entry, uc_l_entry, uc_m_entry, uc_n_entry,
	uc_o_entry, uc_p_entry, uc_q_entry, uc_r_entry, uc_s_entry, uc_t_entry, uc_u_entry,
	uc_v_entry, uc_w_entry, uc_x_entry, uc_y_entry, uc_z_entry
};
