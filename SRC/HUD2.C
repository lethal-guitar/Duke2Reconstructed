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

HUD-related code, part 2

The game itself is redrawn every frame (see game2.c), but the HUD is only drawn
fully after loading a level (and when returning to gameplay from an in-game
menu). During gameplay, only parts of the HUD that have changed are redrawn.

Since the game uses two VGA video pages to implement double-buffering, the HUD
needs to be drawn to both pages at once in order to make it appear persistent
while the game is switching between pages.

*******************************************************************************/


/** Draw or redraw the level number in the HUD */
void pascal HUD_DrawLevelNumber(word level)
{
  register int offset;

  level++;

  offset = level >= 10 ? 1 : 0;

  SetDrawPage(0);
  DrawBigNumberBlue(offset + 35, 21, level);

  SetDrawPage(1);
  DrawBigNumberBlue(offset + 35, 21, level);
}


/** Draw or redraw the HUD background */
void pascal HUD_DrawBackground(void)
{
  int i;

  for (i = 0; i < 2; i++)
  {
    SetDrawPage(i);

    DrawSprite(ACT_HUD_FRAME_BACKGROUND, 0, 34, 20);
    DrawSprite(ACT_HUD_FRAME_BACKGROUND, 1,  2, 24);
    DrawSprite(ACT_HUD_FRAME_BACKGROUND, 2, 30, 24);
  }
}


/** Draw or redraw the player's inventory in the HUD */
void pascal HUD_DrawInventory(void)
{
  register int i = 0;

  // Map inventory index to position in the grid
  //
  // [PERF] Missing `static` causes copy operations here
  const byte X_POS[] = { 0, 2, 0, 2, 0, 2 };
  const byte Y_POS[] = { 0, 0, 2, 2, 4, 4 };

  while (plInventory[i])
  {
    SetDrawPage(gfxCurrentDisplayPage);
    DrawStatusIcon_2x2(XY_TO_OFFSET(31, 4), X_POS[i] + 34, Y_POS[i] + 3);
    DrawSprite(plInventory[i] | 0x8000, 0, X_POS[i] + 35, Y_POS[i] + 4);

    SetDrawPage(!gfxCurrentDisplayPage);
    DrawStatusIcon_2x2(XY_TO_OFFSET(31, 4), X_POS[i] + 34, Y_POS[i] + 3);
    DrawSprite(plInventory[i] | 0x8000, 0, X_POS[i] + 35, Y_POS[i] + 4);

    i++;
  }

  // Erase a most recently removed item by overdrawing just the empty slot
  SetDrawPage(gfxCurrentDisplayPage);
  DrawStatusIcon_2x2(XY_TO_OFFSET(31, 4), X_POS[i] + 34, Y_POS[i] + 3);

  SetDrawPage(!gfxCurrentDisplayPage);
  DrawStatusIcon_2x2(XY_TO_OFFSET(31, 4), X_POS[i] + 34, Y_POS[i] + 3);
}
