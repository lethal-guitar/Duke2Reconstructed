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

HUD-related code, part 3

The game itself is redrawn every frame (see game2.c), but the HUD is only drawn
fully after loading a level (and when returning to gameplay from an in-game
menu). During gameplay, only parts of the HUD that have changed are redrawn.

Since the game uses two VGA video pages to implement double-buffering, the HUD
needs to be drawn to both pages at once in order to make it appear persistent
while the game is switching between pages.

*******************************************************************************/


/** Draw or redraw the boss health bar in the top-row */
void DrawBossHealthBar_Impl(int health)
{
  register word i;

  // Erase previous health bar.
  //
  // [BUG] The screen is 40 tiles wide in total, and the boss health bar starts
  // at position 6. So in case the health bar spans the entire available space,
  // we'd need to erase 34 tiles. But only 30 are erased here.
  for (i = 0; i < 30; i++)
  {
    DrawStatusIcon_1x1(XY_TO_OFFSET(0, 11), 6 + i, 0);
  }

  if (health > 0)
  {
    // The health bar shrinks by one pixel for each point of damage the boss
    // takes, but the health bar is drawn using 8x8-pixel tiles. The status
    // icon tileset contains one tile with a full 8-pixel wide health bar block,
    // and 7 tiles with different widths (7 pixels, 6 pixels etc. down to 1
    // pixel).

    // Draw as many full 8-pixel wide pieces of the health bar as we can
    for (i = 0; health / 8 > i; i++)
    {
      DrawStatusIcon_1x1(XY_TO_OFFSET(8, 11), 6 + i, 0);
    }

    // Then draw the right piece to fill in the remaining width
    DrawStatusIcon_1x1(XY_TO_OFFSET(health % 8, 11), 6 + i, 0);
  }
}


/** Draw or redraw the boss health bar in the top-row along with a label */
void HUD_DrawBossHealthBar(word health)
{
  // [PERF] Missing `static` causes a copy operation here
  const word LABEL_TILES[] = {
    XY_TO_OFFSET(21, 6), // B
    XY_TO_OFFSET(34, 6), // O
    XY_TO_OFFSET(38, 6), // S
    XY_TO_OFFSET(38, 6), // S
  };

  int i;

  gmBossActivated = true;

  SetDrawPage(gfxCurrentDisplayPage);

  // Draw the "BOSS" label
  for (i = 0; i < 4; i++)
  {
    DrawStatusIcon_1x1(LABEL_TILES[i], i + 1, 0);
  }

  DrawBossHealthBar_Impl(health);

  SetDrawPage(!gfxCurrentDisplayPage);

  // Draw the "BOSS" label
  for (i = 0; i < 4; i++)
  {
    DrawStatusIcon_1x1(LABEL_TILES[i], i + 1, 0);
  }

  DrawBossHealthBar_Impl(health);
}


/** Start showing a message in the top-row
 *
 * The message itself will be drawn letter by letter in UpdateAndDrawActors()
 * (game3.c).
 */
void pascal ShowInGameMessage(char* message)
{
  // Do not show messages if this is a boss level or if a hint machine is
  // currently shown
  if (hudShowingHintMachineMsg || gmCurrentLevel > 6) { return; }

  hudCurrentMessage = message;
  hudMessageCharsPrinted = 1;
  hudMessageDelay = 0;

  // Erase the top-row, in case another message is currently visible there
  SetDrawPage(gfxCurrentDisplayPage);
  FillScreenRegion(SFC_BLACK, 0, 0, SCREEN_WIDTH_TILES - 1, 0);

  SetDrawPage(!gfxCurrentDisplayPage);
  FillScreenRegion(SFC_BLACK, 0, 0, SCREEN_WIDTH_TILES - 1, 0);
}


/** Show a tutorial message if it hasn't been shown yet */
void pascal ShowTutorial(TutorialId index, char* message)
{
  if (!gmTutorialsShown[index])
  {
    gmTutorialsShown[index] = true;
    ShowInGameMessage(message);
  }
}

