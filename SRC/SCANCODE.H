/* Copyright (C) 2022, Nikolai Wuttke. All rights reserved.
 *
 * This project is based on disassembly of NUKEM2.EXE from the game
 * Duke Nukem II, Copyright (C) 1993 Apogee Software, Ltd.
 *
 * Some parts of the code are based on or have been adapted from the Cosmore
 * project, Copyright (c) 2020-2022 Scott Smitelli.
 * See LICENSE_Cosmore file at the root of the repository, or refer to
 * https://github.com/smitelli/cosmore/blob/master/LICENSE.
 *
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SCANCODE_H_INC
#define SCANCODE_H_INC

#define SCANCODE_NULL           0x00

/* Standard PC/AT scancodes. */
#define SCANCODE_ESC            0x01
#define SCANCODE_1              0x02  /* 1  ! */
#define SCANCODE_2              0x03  /* 2  @ */
#define SCANCODE_3              0x04  /* 3  # */
#define SCANCODE_4              0x05  /* 4  $ */
#define SCANCODE_5              0x06  /* 5  % */
#define SCANCODE_6              0x07  /* 6  ^ */
#define SCANCODE_7              0x08  /* 7  & */
#define SCANCODE_8              0x09  /* 8  * */
#define SCANCODE_9              0x0a  /* 9  ( */
#define SCANCODE_0              0x0b  /* 0  ) */
#define SCANCODE_MINUS          0x0c  /* -  _ */
#define SCANCODE_EQUAL          0x0d  /* =  + */
#define SCANCODE_BACKSPACE      0x0e
#define SCANCODE_TAB            0x0f
#define SCANCODE_Q              0x10  /* q  Q */
#define SCANCODE_W              0x11  /* w  W */
#define SCANCODE_E              0x12  /* e  E */
#define SCANCODE_R              0x13  /* r  R */
#define SCANCODE_T              0x14  /* t  T */
#define SCANCODE_Y              0x15  /* y  Y */
#define SCANCODE_U              0x16  /* u  U */
#define SCANCODE_I              0x17  /* i  I */
#define SCANCODE_O              0x18  /* o  O */
#define SCANCODE_P              0x19  /* p  P */
#define SCANCODE_LEFT_BRACE     0x1a  /* [  { */
#define SCANCODE_RIGHT_BRACE    0x1b  /* ]  } */
#define SCANCODE_ENTER          0x1c
#define SCANCODE_CTRL           0x1d
#define SCANCODE_A              0x1e  /* a  A */
#define SCANCODE_S              0x1f  /* s  S */
#define SCANCODE_D              0x20  /* d  D */
#define SCANCODE_F              0x21  /* f  F */
#define SCANCODE_G              0x22  /* g  G */
#define SCANCODE_H              0x23  /* h  H */
#define SCANCODE_J              0x24  /* j  J */
#define SCANCODE_K              0x25  /* k  K */
#define SCANCODE_L              0x26  /* l  L */
#define SCANCODE_SEMICOLON      0x27  /* ;  : */
#define SCANCODE_APOSTROPHE     0x28  /* '  " */
#define SCANCODE_GRAVE          0x29  /* `  ~ */
#define SCANCODE_LEFT_SHIFT     0x2a
#define SCANCODE_BACKSLASH      0x2b  /* \  | */
#define SCANCODE_Z              0x2c  /* z  Z */
#define SCANCODE_X              0x2d  /* x  X */
#define SCANCODE_C              0x2e  /* c  C */
#define SCANCODE_V              0x2f  /* v  V */
#define SCANCODE_B              0x30  /* b  B */
#define SCANCODE_N              0x31  /* n  N */
#define SCANCODE_M              0x32  /* m  M */
#define SCANCODE_COMMA          0x33  /* ,  < */
#define SCANCODE_DOT            0x34  /* .  > */
#define SCANCODE_SLASH          0x35  /* /  ? */
#define SCANCODE_RIGHT_SHIFT    0x36
#define SCANCODE_KP_ASTERISK    0x37  /* *  PrtSc */
#define SCANCODE_ALT            0x38
#define SCANCODE_SPACE          0x39
#define SCANCODE_CAPS_LOCK      0x3a
#define SCANCODE_F1             0x3b
#define SCANCODE_F2             0x3c
#define SCANCODE_F3             0x3d
#define SCANCODE_F4             0x3e
#define SCANCODE_F5             0x3f
#define SCANCODE_F6             0x40
#define SCANCODE_F7             0x41
#define SCANCODE_F8             0x42
#define SCANCODE_F9             0x43
#define SCANCODE_F10            0x44
#define SCANCODE_NUM_LOCK       0x45
#define SCANCODE_SCROLL_LOCK    0x46
#define SCANCODE_HOME           0x47
#define SCANCODE_UP             0x48
#define SCANCODE_PGUP           0x49
#define SCANCODE_KP_MINUS       0x4a
#define SCANCODE_LEFT           0x4b
#define SCANCODE_KP_5           0x4c
#define SCANCODE_RIGHT          0x4d
#define SCANCODE_KP_PLUS        0x4e
#define SCANCODE_END            0x4f
#define SCANCODE_DOWN           0x50
#define SCANCODE_PGDOWN         0x51
#define SCANCODE_INSERT         0x52
#define SCANCODE_DELETE         0x53
#define SCANCODE_SYS_REQ        0x54

/* Additional PS/2 scancodes. */
#define SCANCODE_F11            0x57
#define SCANCODE_F12            0x58
#define SCANCODE_EXTENDED       0xe0

#endif
