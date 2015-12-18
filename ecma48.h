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

#ifndef ECMA48_H_
#define ECMA48_H_

#define PRIDA "\033[?1;2c"    /* VT100 with advanced video option */
#define SECDA "\033[>1;95;0c" /* VT220 */

#define DSROK "\033[0n"
#define ANSWERBACK "Term48"
#define OUTBUF_LEN 30

void ecma48_init();
void ecma48_uninit();
void ecma48_setenv();
int  ecma48_parse_control_codes(int sym, int mod, UChar* buf);
void ecma48_filter_text(UChar* tbuf, ssize_t chars);

/* Control Code function declarations */
void ecma48_NUL();
void ecma48_SOH();
void ecma48_STX();
void ecma48_ETX();
void ecma48_EOT();
void ecma48_ENQ();
void ecma48_ACK();
void ecma48_BEL();
void ecma48_BS();
void ecma48_HT();
void ecma48_LF();
void ecma48_VT();
void ecma48_FF();
void ecma48_CR();
void ecma48_SO();
void ecma48_SI();
void ecma48_DLE();
void ecma48_DC1();
void ecma48_DC2();
void ecma48_DC3();
void ecma48_DC4();
void ecma48_NAK();
void ecma48_SYN();
void ecma48_ETB();
void ecma48_CAN();
void ecma48_EM();
void ecma48_SUB();
void setstate_ESC();
void ecma48_IS1();
void ecma48_IS2();
void ecma48_IS3();
void ecma48_IS4();
void ecma48_BPH();
void ecma48_NBH();
void ecma48_NEL();
void ecma48_SSA();
void ecma48_ESA();
void ecma48_HTS();
void ecma48_HTJ();
void ecma48_VTS();
void ecma48_PLD();
void ecma48_PLU();
void ecma48_RI();
void ecma48_SS2();
void ecma48_SS3();
void ecma48_DCS();
void ecma48_PU1();
void ecma48_PU2();
void ecma48_STS();
void ecma48_CCH();
void ecma48_MW();
void ecma48_SPA();
void ecma48_EPA();
void ecma48_SOS();
void ecma48_SCI();
void setstate_CSI();
void ecma48_ST();
void ecma48_OSC();
void ecma48_PM();
void ecma48_APC();
void ecma48_DMI();
void ecma48_INT();
void ecma48_EMI();
void ecma48_RIS();
void ecma48_CMD();
void ecma48_LS2();
void ecma48_LS3();
void ecma48_LS3R();
void ecma48_LS2R();
void ecma48_LS1R();
void ecma48_ICH();
void ecma48_CUU();
void ecma48_CUD();
void ecma48_CUF();
void ecma48_CUB();
void ecma48_CNL();
void ecma48_CPL();
void ecma48_CHA();
void ecma48_CUP();
void ecma48_CHT();
void ecma48_ED();
void ecma48_EL();
void ecma48_IL();
void ecma48_DL();
void ecma48_EF();
void ecma48_EA();
void ecma48_DCH();
void ecma48_SEE();
void ecma48_CPR();
void ecma48_SU();
void ecma48_SD();
void ecma48_NP();
void ecma48_PP();
void ecma48_CTC();
void ecma48_ECH();
void ecma48_CVT();
void ecma48_CBT();
void ecma48_SRS();
void ecma48_PTX();
void ecma48_SDS();
void ecma48_SIMD();
void ecma48_HPA();
void ecma48_HPR();
void ecma48_REP();
void ecma48_DA();
void ecma48_VPA();
void ecma48_VPR();
void ecma48_HVP();
void ecma48_TBC();
void ecma48_SM();
void ecma48_MC();
void ecma48_HPB();
void ecma48_VPB();
void ecma48_RM();
void ecma48_SGR();
void ecma48_DSR();
void ecma48_DAQ();
void ansi_DECANM();

#endif /* ECMA48_H_ */
