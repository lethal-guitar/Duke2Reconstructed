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


/*******************************************************************************

Utilites, standard library function replacements, and some core functionality

*******************************************************************************/


/** Replacement for inportb
 *
 * I'm not sure why the devs made their own version of this. The standard
 * version is slightly more efficient, since it's intrinsic - i.e. the compiler
 * directly emits the required instructions instead of a function call.  So this
 * seems like a pessimization.
 */
byte pascal DN2_inportb(int address)
{
  asm mov dx, [address]
  asm in  al, dx

  return _AL;
}


/** Replacement for outportb
 *
 * Same here as with inportb.
 */
void pascal DN2_outportb(int address, byte value)
{
  asm mov dx, [address]
  asm mov al, [value]
  asm out dx, al
}


/** Replacement for outport */
void pascal DN2_outport(int address, word value)
{
  asm mov dx, [address]
  asm mov ax, [value]
  asm out dx, ax
}


/** Pause execution for specified number of ticks
 *
 * Conceptually very similar to how you'd use sleep() in modern code, but this
 * does a busy wait instead of putting the thread to sleep (threads don't
 * really exist in DOS).
 * There are 140 ticks per second, so one tick is about 7.14 ms.
 * This function depends on the timer interrupt, so InstallTimerInterrupt must
 * have been called before using it.
 */
void pascal WaitTicks(word ticks)
{
  sysTicksElapsed = 0;

  // This looks like an infinite loop, but the timer interrupt regularly fires
  // and makes the CPU run the interrupt service routine, which increments
  // sysTicksElapsed (see TimerInterruptHandler() in music.c).
  while (sysTicksElapsed < ticks);
}


/** Sets VGA 16-color palette entry at specified index
 *
 * index must be between 0 and 15, and each color component (r, g, b) must be
 * between 0 and 63.
 */
void pascal SetPaletteEntry_16(word index, byte r, byte g, byte b)
{
  // The 16-color mode used by the game, mode 0xD, is an EGA mode.  VGA cards
  // are backwards compatible with EGA and offer this mode as well. Because of
  // this, the way the palette is set in this mode is a little strange: Colors
  // for the indices from 8 to 15 actually need to be set for indices 16 to 23.
  // This is because of the way EGA itself is backwards compatible with CGA.
  // EGA uses 6 bits to transmit color information to the display, with each bit
  // being transmitted over its own signal connection and thus having a
  // dedicated pin on the monitor connector. CGA uses only 4 bits.  Making use
  // of the additional 2 bits/pins requires a special EGA display, but an EGA
  // card can also drive a CGA display. 2 of the pins are unused in that case,
  // while the remaining 4 are used in a way that matches the pinout for a CGA
  // card. EGA cards will only make use of full 6-bit color when running in vido
  // modes with a vertical resolution of 350, modes with 200 pixels can only use
  // the CGA compatibility mode with 4 bits. Mode 0xD is 320x200, and thus
  // also limited to CGA compatibility.
  //
  // On EGA cards, the palette always maps a 4-bit index to a 6-bit color value,
  // and the 6 bits in the color value directly correspond to the pins on the
  // monitor connector. When running in CGA compatibility mode, 2 of those bits
  // are unused. Due to the correlation between bits and pins and the need to
  // have a CGA-compatible pinout, the bits that are used/unused are arranged as
  // follows:
  //
  //   | 6 | 5 | 4 | 3 | 2 | 1 |
  //   |   | X |   | X | X | X |
  //
  // Specifically, bit 5 maps to the intensity pin on a CGA card, while the pins
  // corresponding to bits 4 and 6 remain unused.
  //
  // And now here's the catch: The EGA palette is still based on 6-bit color,
  // even in CGA compatibility mode. By default, it is set up so that the
  // palette matches the colors that were available on a CGA monitor. This means
  // that the 4 bits of a palette index each have a specific meaning, namely
  // blue, green, red, and intensity (which makes the entire color brighter),
  // with each bit corresponding directly to one of the signal pins. This is
  // only the case when using the default EGA palette, the palette can be
  // changed so that this relationship doesn't apply anymore. But this is how
  // the EGA palette is set up when entering mode 0xD.
  // Due to the intensity pin being at bit 5 of the EGA color value, not bit 4,
  // the color values jump from a value of 7 at index 7 to a value of 16 at
  // index 8. Reading the color value as a binary number and then comparing to
  // the pinout diagram above makes it clear why that's the case:
  // 7 is 111b and 16 is 10000b. The default palette wants color index 8 to
  // have the intensity pin on and the three color pins off.
  //
  // VGA cards use analog signals for color and have no direct correlation
  // between bits and pins anymore. But they do retain the EGA default palette
  // when running in EGA compatibility mode. And for some reason, changing the
  // palette requires replicating this EGA-specific discontinuity, so that's
  // what we need to do here.
  if (index > 7)
  {
    index += 8;
  }

  DN2_outportb(0x3C8, index);
  DN2_outportb(0x3C9, r);
  DN2_outportb(0x3C9, g);
  DN2_outportb(0x3C9, b);
}


/** Sets VGA 256-color palette entry at specified index
 *
 * index must be between 0 and 255, and each color component (r, g, b) must be
 * between 0 and 63.
 */
void pascal SetPaletteEntry_256(word index, word r, word g, word b)
{
  DN2_outportb(0x3C8, index);
  DN2_outportb(0x3C9, r);
  DN2_outportb(0x3C9, g);
  DN2_outportb(0x3C9, b);
}


/** Replacement for C library's toupper */
static char pascal DN2_toupper(char c)
{
  return c >= 'a' && c <= 'z' ? (c - 'a' + 'A') : c;
}


/** Copy a string and make it uppercase
 *
 * This is basically strcpy() followed by strupr(), but done in one pass.
 */
void pascal CopyStringUppercased(const char far* src, char far* dest)
{
  const char far* pCurrentChar = src;

  while (*pCurrentChar != '\0')
  {
    *dest++ = DN2_toupper(*pCurrentChar++);
  }

  *dest = '\0';
}


/** Test if the beginning of string is identical to prefix */
bool pascal StringStartsWith(const char far* prefix, const char far* string)
{
  const char far* pCurrentChar = prefix;

  while (*pCurrentChar == *string)
  {
    ++pCurrentChar;
    ++string;

    if (!*string || !*pCurrentChar)
    {
      return true;
    }
  }

  return false;
}


/** Replacement for C library's strlen */
word pascal DN2_strlen(char far* string)
{
  register int i = 0;

  while (string[i]) {
    i++;
  }

  return i;
}


/** Generate a "random" number
 *
 * This random number "generator" starts repeating after just 256 invocations,
 * which is quite limited. But it's very fast and also deterministic, which is
 * important for demo playback.
 */
byte RandomNumber(void)
{
  static const byte RANDOM_NR_TABLE[] = {
    0,   8,   109, 220, 222, 241, 149, 107, 75,  248, 254, 140, 16,  66,  74,
    21,  211, 47,  80,  242, 154, 27,  205, 128, 161, 89,  77,  36,  95,  110,
    85,  48,  212, 140, 211, 249, 22,  79,  200, 50,  28,  188, 52,  140, 202,
    120, 68,  145, 62,  70,  184, 190, 91,  197, 152, 224, 149, 104, 25,  178,
    252, 182, 202, 182, 141, 197, 4,   81,  181, 242, 145, 42,  39,  227, 156,
    198, 225, 193, 219, 93,  122, 175, 249, 0,   175, 143, 70,  239, 46,  246,
    163, 53,  163, 109, 168, 135, 2,   235, 25,  92,  20,  145, 138, 77,  69,
    166, 78,  176, 173, 212, 166, 113, 94,  161, 41,  50,  239, 49,  111, 164,
    70,  60,  2,   37,  171, 75,  136, 156, 11,  56,  42,  146, 138, 229, 73,
    146, 77,  61,  98,  196, 135, 106, 63,  197, 195, 86,  96,  203, 113, 101,
    170, 247, 181, 113, 80,  250, 108, 7,   255, 237, 129, 226, 79,  107, 112,
    166, 103, 241, 24,  223, 239, 120, 198, 58,  60,  82,  128, 3,   184, 66,
    143, 224, 145, 224, 81,  206, 163, 45,  63,  90,  168, 114, 59,  33,  159,
    95,  28,  139, 123, 98,  125, 196, 15,  70,  194, 253, 54,  14,  109, 226,
    71,  17,  161, 93,  186, 87,  244, 138, 20,  52,  123, 251, 26,  36,  17,
    46,  52,  231, 232, 76,  31,  221, 84,  37,  216, 165, 212, 106, 197, 242,
    98,  43,  39,  175, 254, 145, 190, 84,  118, 222, 187, 136, 120, 163, 236,
    249
  };

  gmRngIndex++;
  return RANDOM_NR_TABLE[gmRngIndex];
}
