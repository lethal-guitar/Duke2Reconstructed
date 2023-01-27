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

UI drawing routines, part 1: Basic functions, message box frame drawing

One of the game's data files is STATUS.MNI, a tileset containing all kinds of
UI elements. This includes multiple fonts, icons used in the HUD and menus,
and other things. LoadStatusIcons() loads the tileset and copies it into
video memory. This makes it very fast to display status icons on screen using
latch copies. (Refer to gfx.asm for more info on the latch copy technique).

The majority of the game's UI is built up from these status icon tiles.
This file contains a set of building block functions for drawing status icons
of different sizes, and some other functions which are built on top of that
mechanism.

*******************************************************************************/


/** Draw a single status icon tile at the given position
 *
 * WARNING: It's important to ensure that the resulting drawing operation is in
 * bounds w.r.t. the screen. If not, unrelated video memory will be overwritten,
 * which can cause graphical corruption.
 *
 * To specify the source offset in a human-readable manner, use the
 * XY_TO_OFFSET() macro.
 */
void pascal DrawStatusIcon_1x1(word srcOffset, word x, word y)
{
  EGA_SETUP_LATCH_COPY();

  BlitSolidTile(srcOffset + 0x2000, x + y*320);
}


/** Draw a status icon consisting of two vertically adjacent tiles
 *
 * WARNING: It's important to ensure that the resulting drawing operation is in
 * bounds w.r.t. the screen. If not, unrelated video memory will be overwritten,
 * which can cause graphical corruption.
 *
 * To specify the source offset in a human-readable manner, use the
 * XY_TO_OFFSET() macro.
 */
void pascal DrawStatusIcon_1x2(word srcOffset, word x, word y)
{
  DrawStatusIcon_1x1(srcOffset, x, y);
  DrawStatusIcon_1x1(srcOffset + 320, x, y + 1);
}


/** Draw a status icon consisting of two horizontally adjacent tiles
 *
 * WARNING: It's important to ensure that the resulting drawing operation is in
 * bounds w.r.t. the screen. If not, unrelated video memory will be overwritten,
 * which can cause graphical corruption.
 *
 * To specify the source offset in a human-readable manner, use the
 * XY_TO_OFFSET() macro.
 */
void pascal DrawStatusIcon_2x1(word srcOffset, word x, word y)
{
  DrawStatusIcon_1x1(srcOffset, x, y);
  DrawStatusIcon_1x1(srcOffset + 8, x + 1, y);
}


/** Draw a status icon consisting of four adjacent tiles (2 by 2)
 *
 * WARNING: It's important to ensure that the resulting drawing operation is in
 * bounds w.r.t. the screen. If not, unrelated video memory will be overwritten,
 * which can cause graphical corruption.
 *
 * To specify the source offset in a human-readable manner, use the
 * XY_TO_OFFSET() macro.
 */
void pascal DrawStatusIcon_2x2(word srcOffset, word x, word y)
{
  DrawStatusIcon_1x2(srcOffset, x, y);
  DrawStatusIcon_1x2(srcOffset + 8, x + 1, y);
}


/** Fill specified part of the screen with a specified color
 *
 * The coordinates are in tiles, i.e. must be multiplied by 8 to get pixel
 * coordinates.
 *
 * WARNING: It's important to ensure that the given coordinates are in bounds
 * w.r.t. the screen. If not, unrelated video memory will be overwritten, which
 * can cause graphical corruption.
 *
 * The screen filling is implemented via the status icon tileset, and there are
 * only a few valid colors to use - namely those defined by the ScreenFillColor
 * enum.
 */
void pascal FillScreenRegion(
  word fillTileIndex,
  word left,
  word bottom,
  word right,
  word top)
{
  int x;
  int y;

  fillTileIndex <<= 3; // *= 8

  for (y = bottom; y <= top; y++)
  {
    for (x = left; x <= right; x++)
    {
      DrawStatusIcon_1x1(fillTileIndex + XY_TO_OFFSET(13, 4), x, y);
    }
  }
}


/** Draw a message box frame at the given coordinates
 *
 * WARNING: It's important to ensure that the given coordinates are in bounds
 * w.r.t. the screen. If not, unrelated video memory will be overwritten, which
 * can cause graphical corruption.
 */
int pascal DrawMessageBoxFrame(word left, word top, int height, int width)
{
  register int x;
  register int y;

  left -= uiMessageBoxShift;

  // Fill in the background
  for (y = 1; (height - 1) > y; y++)
  {
    for (x = 1; (width - 1) > x; x++)
    {
      DrawStatusIcon_1x1(XY_TO_OFFSET(8, 4), x + left, y + top);
    }
  }

  // Draw the sides
  //
  // [NOTE] This ends up drawing into the corners as well, which are then
  // drawn over again further down. Starting the loop at 1 and ending at
  // height-1 would avoid this.
  for (y = 0; y < height; y++)
  {
    DrawStatusIcon_1x1(XY_TO_OFFSET(7, 4), left, y + top);
    DrawStatusIcon_1x1(XY_TO_OFFSET(3, 4), left + width - 1, y + top);
  }

  // Draw the bottom/top edges
  //
  // [NOTE] Same thing here as with the sides.
  for (x = 0; x < width; x++)
  {
    DrawStatusIcon_1x1(XY_TO_OFFSET(1, 4), x + left, top);
    DrawStatusIcon_1x1(XY_TO_OFFSET(5, 4), left + x, top + height - 1);
  }

  // Draw the corners
  DrawStatusIcon_1x1(XY_TO_OFFSET(0, 4), left, top);
  DrawStatusIcon_1x1(XY_TO_OFFSET(2, 4), left + width - 1, top);
  DrawStatusIcon_1x1(XY_TO_OFFSET(6, 4), left, top + height - 1);
  DrawStatusIcon_1x1(XY_TO_OFFSET(4, 4), left + width - 1, top + height - 1);

  // [NOTE] The return value isn't used anywhere, this could've been deleted.
  // This is most likely a holdover from the Cosmo codebase, which has a
  // basically identical version of this same function, but does make use of
  // the return value.
  return left + 1;
}
