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

UI drawing routines, part 3: Text drawing

This file provides code for drawing large colored text using the sprite-based
font used in the menus, and the large white font used in the bonus screen.

All the fonts used in the game are bitmap-based fonts, which means that every
character that can be drawn has a corresponding bitmap graphic.

*******************************************************************************/


/** Draw a single character using the sprite-based font
 *
 * Index is an index into the font, not an ASCII value. The translation from
 * the latter to the former happens in DrawBigTextChar().
 *
 * Unlike regular sprites, the font is stored as a series of monochrome bitmaps
 * (only one color bit-plane). This allows the game to draw text in any of the
 * 16 palette colors. Since colors are 4-bit values, the character is drawn once
 * for each bit that's set in the color index, targeting the corresponding
 * bit-plane in video memory. The end result is pixels of the desired color.
 */
static void pascal DrawColorizedChar(word index, word x, word y, word color)
{
  // In Duke 2, Y always refers to the bottom when drawing sprites, hence the
  // adjustment here. I'm not sure why the X position is offset by 2 though.
  y--;
  x += 2;

  // [NOTE] A much more concise version without the need for a macro would be:
  //
  // DrawFontSprite(color & 1 ? index : 40, x, y, plane);
  //  ... etc. ...
  //
  // But that results in slightly different code from the compiler, so we keep
  // this version.
#define DRAW_PLANE_IF_SET(bitmask, plane)        \
  if (color & bitmask)                    \
  {                                       \
    DrawFontSprite(index, x, y, plane);   \
  }                                       \
  else                                    \
  {                                       \
    DrawFontSprite(40, x, y, plane);      \
  }

  DRAW_PLANE_IF_SET(1, 0);
  DRAW_PLANE_IF_SET(2, 1);
  DRAW_PLANE_IF_SET(4, 2);
  DRAW_PLANE_IF_SET(8, 3);

#undef DRAW_PLANE_IF_SET
}


/** Draw a single character using one of two large fonts
 *
 * The primary purpose of this function is mapping ASCII character values to
 * the corresponding sprite frames or status icon tile coordinates needed to
 * draw that character.
 *
 * Also see DrawBigText() below.
 */
static void pascal DrawBigTextChar(word x, word y, char c, byte color)
{
 /*
  * Layout of the large font actor/sprite:
  *
  * |   0 | A |
  * | ...     |
  * |  25 | Z |
  * |  26 | 0 | // <-- start of numbers
  * |  27 | 1 |
  * |  ...    |
  * |  35 | 9 |
  * |  36 | ? |
  * |  37 | , |
  * |  38 | . |
  * |  39 | ! |
  * |  40 |   | // empty
  * |  41 | a | // <-- start of lowercase letters
  * |  42 | b |
  * |  ...    |
  * |  66 | z |
  */

  if (color == 16) // use status-icon based font
  {
    y--;

    if (c == '?')
    {
      // Actually a '%' in the graphics data
      DrawStatusIcon_2x2(XY_TO_OFFSET(32, 2), x, y);
    }
    else if (c == ',')
    {
      // Actually a '=' in the graphics data
      DrawStatusIcon_2x2(XY_TO_OFFSET(34, 2), x, y);
    }
    else if (c == '.')
    {
      DrawStatusIcon_2x2(XY_TO_OFFSET(36, 2), x, y);
    }
    else if (c == '!')
    {
      DrawStatusIcon_2x2(XY_TO_OFFSET(38, 2), x, y);
    }
    else if (c == ' ')
    {
      DrawStatusIcon_2x2(XY_TO_OFFSET(0, 5), x, y);
    }
    else if (c <= '9')
    {
      DrawStatusIcon_2x2(((c - '0') << 4) + XY_TO_OFFSET(0, 0), x, y);
    }
    else if (c <= 'J')
    {
      DrawStatusIcon_2x2(((c - 'A') << 4) + XY_TO_OFFSET(20, 0), x, y);
    }
    else if (c <= 'Z')
    {
      DrawStatusIcon_2x2(((c - 'K') << 4) + XY_TO_OFFSET(0, 2), x, y);
    }
  }
  else // use sprite-based font, use specified color
  {
    if (c != ' ')
    {
      if (c >= 'A' && c <= 'Z')
      {
        DrawColorizedChar(c - 'A', x, y, color);
      }
      else if (c >= 'a' && c <= 'z')
      {
        DrawColorizedChar(c - 'a' + 41, x, y, color);
      }
      else if (c >= '0' && c <= '9')
      {
        DrawColorizedChar(c - '0' + 26, x, y, color);
      }
      else
      {
        byte index;

        switch (c)
        {
          case '?': index = 36; break;
          case ',': index = 37; break;
          case '.': index = 38; break;
          case '!': index = 39; break;
        }

        DrawColorizedChar(index, x, y, color);
      }
    }
  }
}


/** Draw string at given position using one of two large fonts
 *
 * If color is between 0 and 15, the text will be drawn in that color using the
 * sprite-based font. Color can also be 16 however. In that case, the text will
 * be drawn using a status-icon based font, which is always white. The latter is
 * used for the bonus-screen (see bonusscr.c).
 */
void pascal DrawBigText(word x, word y, const char far* str, byte color)
{
  int i;
  word pos = x;

  for (i = 0; str[i] != '\0'; i++)
  {
    DrawBigTextChar(pos + i, y, str[i], color);

    // When using the alternate big font, characters are 2 tiles wide, so we
    // need to advance the position by one here to make it advance by 2 in total
    // after each character.
    //
    // [NOTE] It would seem a bit clearer to multiply i by 2 for this
    // case instead, i.e.:
    //
    // DrawBigTextChar(pos + (color == 16 ? i * 2 : i), ...
    if (color == 16)
    {
      pos++;
    }
  }
}
