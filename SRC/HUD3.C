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

TODO: Further document this file and the functions here

*******************************************************************************/


void DrawBossHealthBar_Impl(int health)
{
  register word i;

  for (i = 0; i < 30; i++)
  {
    DrawStatusIcon_1x1(XY_TO_OFFSET(0, 11), 6 + i, 0);
  }

  if (health > 0)
  {
    for (i = 0; health / 8 > i; i++)
    {
      DrawStatusIcon_1x1(XY_TO_OFFSET(8, 11), 6 + i, 0);
    }

    DrawStatusIcon_1x1(XY_TO_OFFSET(health % 8, 11), 6 + i, 0);
  }
}


void HUD_DrawBossHealthBar(word health)
{
  const word LABEL_TILES[] = {
    XY_TO_OFFSET(21, 6), // B
    XY_TO_OFFSET(34, 6), // O
    XY_TO_OFFSET(38, 6), // S
    XY_TO_OFFSET(38, 6), // S
  };

  int i;

  gmBossActivated = true;

  SetDrawPage(gfxCurrentDisplayPage);
  for (i = 0; i < 4; i++)
  {
    DrawStatusIcon_1x1(LABEL_TILES[i], i + 1, 0);
  }
  DrawBossHealthBar_Impl(health);

  SetDrawPage(!gfxCurrentDisplayPage);
  for (i = 0; i < 4; i++)
  {
    DrawStatusIcon_1x1(LABEL_TILES[i], i + 1, 0);
  }
  DrawBossHealthBar_Impl(health);
}


void pascal ShowInGameMessage(char* message)
{
  if (hudShowingHintMachineMsg || gmCurrentLevel > 6) { return; }

  hudCurrentMessage = message;
  hudMessageCharsPrinted = 1;
  hudMessageDelay = 0;

  SetDrawPage(gfxCurrentDisplayPage);
  FillScreenRegion(SFC_BLACK, 0, 0, 39, 0);

  SetDrawPage(!gfxCurrentDisplayPage);
  FillScreenRegion(SFC_BLACK, 0, 0, 39, 0);
}


void pascal ShowTutorial(TutorialId index, char* message)
{
  if (!gmTutorialsShown[index])
  {
    gmTutorialsShown[index] = true;
    ShowInGameMessage(message);
  }
}

