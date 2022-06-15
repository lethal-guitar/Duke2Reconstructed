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

UI drawing routines, part 2: Message box slide-in animation

*******************************************************************************/


/** Draw an animation of a message box frame appearing
 *
 * The frame will be drawn horizontally centered on screen using the given
 * coordinates. The animation first expands the frame horizontally, then
 * vertically.
 *
 * The function returns once the animation is finished.
 */
void pascal UnfoldMessageBoxFrame(int top, int height, int width)
{
  register int i;
  register int left = SCREEN_WIDTH_TILES/2 - (width >> 1);
  word xcenter = SCREEN_WIDTH_TILES/2 - 1;
  word ycenter = top + (height >> 1);
  word size;

  // Expand horizontally
  size = 1;
  for (i = xcenter; i > left; i--)
  {
    DrawMessageBoxFrame(i, ycenter, 2, size += 2);
    WaitTicks(1);
  }

  // ... then expand vertically.
  size = 0;
  for (i = ycenter; i > top + !(height & 1); i--)
  {
    DrawMessageBoxFrame(left, i, size += 2, width);
    WaitTicks(1);
  }

  // Finally, draw the frame at its final size
  DrawMessageBoxFrame(left, top, height, width);
}
