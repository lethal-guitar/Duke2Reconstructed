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

High-score list related functions

Some parts of the high-score list are also in main.c and ui2.c.

*******************************************************************************/


/** Read given episode's high score list from file on disk */
void pascal ReadHighScoreList(byte episode)
{
  int i;
  int fd;

  switch (episode)
  {
    case 1:
      fd = OpenFileRW("NUKEM2.-V1");
      break;

    case 2:
      fd = OpenFileRW("NUKEM2.-V2");
      break;

    case 3:
      fd = OpenFileRW("NUKEM2.-V3");
      break;

    case 4:
      fd = OpenFileRW("NUKEM2.-V4");
      break;
  }

  if (fd == -1)
  {
    // If no high score list file was found, initialize the list.
    // Unlike in Cosmo, there are no predefined entries here,
    // everything is just set to 0.
    for (i = 0; i < NUM_HIGH_SCORE_ENTRIES; i++)
    {
      gmHighScoreList[i] = 0;
      gmHighScoreNames[i][0] = 0;
    }
  }
  else
  {
    for (i = 0; i < NUM_HIGH_SCORE_ENTRIES; i++)
    {
      _read(fd, gmHighScoreNames + i, HIGH_SCORE_NAME_MAX_LEN);
      _read(fd, gmHighScoreList + i, sizeof(dword));
    }

    CloseFile(fd);
  }
}


/** Persist given episode's high score list to file on disk */
void pascal WriteHighScoreList(byte episode)
{
  register int i;
  int fd;

  switch (episode)
  {
    case 1:
      fd = OpenFileW("NUKEM2.-V1");
      break;

    case 2:
      fd = OpenFileW("NUKEM2.-V2");
      break;

    case 3:
      fd = OpenFileW("NUKEM2.-V3");
      break;

    case 4:
      fd = OpenFileW("NUKEM2.-V4");
      break;
  }

  for (i = 0; i < NUM_HIGH_SCORE_ENTRIES; i++)
  {
    _write(fd, gmHighScoreNames + i, HIGH_SCORE_NAME_MAX_LEN);
    _write(fd, gmHighScoreList + i, sizeof(dword));
  }

  CloseFile(fd);
}


/** Draw the given episode's high score list (names & scores)
  *
  * This draws the names and score values, but not the background. The latter
  * is handled by ShowHighScoreList() in main.c.
  */
void pascal DrawHighScoreList(byte episode)
{
  char scoreStr[12];

  word i;
  int yOffset = 0;

  ReadHighScoreList(episode);

  for (i = 0; i < NUM_HIGH_SCORE_ENTRIES; i++)
  {
    if (i == 1)
    {
      yOffset = 1;
    }

    ultoa(gmHighScoreList[i], scoreStr, 10);

    DrawText(10, i + yOffset + 6, scoreStr);
    DrawText(20, i + yOffset + 6, gmHighScoreNames[i]);
  }

  WriteHighScoreList(episode);
}


/** Adds player's current score into high score list if high enough
  *
  * If the player's score qualifies, it's entered into the high score list,
  * moving existing entries with lower scores down if needed.  If the score
  * isn't high enough, nothing happens.
  */
void pascal TryAddHighScore(byte episode)
{
  int i;
  int j;

  ReadHighScoreList(episode);

  // See if the player's score is higher or equal to one of the high scores
  for (i = 0; i < NUM_HIGH_SCORE_ENTRIES; i++)
  {
    // Is the player's current score eligible?
    if (plScore > gmHighScoreList[i])
    {
      // Shift existing entries down by one
      for (j = 9; j > i; j--)
      {
        gmHighScoreList[j] = gmHighScoreList[j - 1];
        CopyStringUppercased(gmHighScoreNames[j - 1], gmHighScoreNames[j]);
      }

      // And enter the new high score
      gmHighScoreNames[i][0] = 0;
      gmHighScoreList[i] = plScore;

      DrawNewHighScoreEntryBackground();
      RunHighScoreNameEntry(
        12, 14, gmHighScoreNames[i], HIGH_SCORE_NAME_MAX_LEN);

      WriteHighScoreList(episode);
      return;
    }
  }
}
