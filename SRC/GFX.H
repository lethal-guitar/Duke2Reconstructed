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


/** Resets the EGA's bit mask to its default state.
  *
  * Allows writes to all eight pixel positions in each written byte.
  */
#define EGA_SET_DEFAULT_BITMASK() asm { \
  mov dx, 0x3ce;  \
  mov ax, 0xff08; \
  out dx, ax;     \
}

/** Resets the EGA's read and write modes to their default state.
  *
  * This allows for direct (i.e. non-latched) writes from the CPU.
  */
#define EGA_SET_DEFAULT_MODE() asm { \
  mov dx, 0x3ce; \
  mov ax, 0x5;   \
  out dx, ax;    \
}

/** Configures EGA for latch copy writes
  *
  * Resets the EGA's map mask to its default state (allows writes to all four
  * memory planes), sets default read mode, and enables latched writes from the
  * CPU.
  */
#define EGA_SETUP_LATCH_COPY() asm { \
  mov dx, 0x3c4; \
  mov ax, 0xf02; \
  out dx, ax;    \
  mov dx, 0x3ce; \
  mov ax, 0x105; \
  out dx, ax;    \
}

// See gfx.asm for documentation on what these functions do.
void SetVideoMode(word mode);
void SetBorderColor(word color);
void SetDrawPage(word page);
void SetDisplayPage(word page);
void AwaitVblank(void);

void BlitSolidTile(word srcOffset, word destOffset);
void BlitMaskedTile(byte far* data, word x, word y);
void BlitMaskedTile_FlexibleY(byte far* data, word x, word yInPx);
void BlitFontTile(byte far* data, word x, word y, word plane);
void BlitMaskedTileTranslucent(byte far* data, word x, word y);
void BlitMaskedMapTile(byte far* data, word destOffset);
void BlitMaskedTileWhiteFlash(byte far* data, word x, word y);

void ApplyWaterEffect(word x, word y);
void ApplyWaterEffectWave0(word x, word y);
void ApplyWaterEffectWave1(word x, word y);
void ApplyWaterEffectWave2(word x, word y);

void SetPixel(word x, word y, word color);
