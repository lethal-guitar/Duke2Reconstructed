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

The "bonus screen" shown between levels

*******************************************************************************/


/** Show animation and update player score */
static void ApplyBonus(byte bonusNum)
{
  static const char* TEXT_SLIDE_IN[] = {
    "S  ",
    "ONUS  ",
    " BONUS  ",
    "ET BONUS  ",
    "CRET BONUS  ",
    "SECRET BONUS  ",
    "            ,,",
    "          ,,,,",
    "        ,,,,,,",
    "      ,,,,,,,,",
    "    ,,,,,,,,,,",
    "  ,,,,,,,,,,,,",
    ",,,,,,,,,,,,,,",
    "  ,,,,,,,,,,,,",
    "   N,,,,,,,,,,",
    "   NO ,,,,,,,,",
    "   NO BO,,,,,,",
    "   NO BONU,,,,",
    "   NO BONUS!,,",
    "   NO BONUS!  ",
    " NO BONUS!  BE",
    "O BONUS! BETTE",
    "BONUS! BETTER ",
    "NUS! BETTER LU",
    "S! BETTER LUCK",
    " BETTER LUCK! ",
    "ETTER LUCK!  N",
    "TER LUCK!  NEX",
    "R LUCK!  NEXT ",
    "LUCK!  NEXT TI",
    "CK!  NEXT TIME",
    "!  NEXT TIME! ",
    "  NEXT TIME!  "};

  int i;

  WaitTicks(100);

  // A bonusNum of 0 indicates that we should play the "No Bonus" animation
  if (!bonusNum)
  {
    // "No Bonus"
    for (i = 6; i < 20; i++)
    {
      DrawBigText(6, 18, TEXT_SLIDE_IN[i], 16);
      WaitTicks(5);
    }

    PlaySound(SND_BIG_EXPLOSION);
    WaitTicks(130);

    // "Better luck"
    for (i = 20; i < 26; i++)
    {
      DrawBigText(6, 18, TEXT_SLIDE_IN[i], 16);
      WaitTicks(10);
    }

    PlaySound(SND_BIG_EXPLOSION);
    WaitTicks(130);

    // "Next time"
    for (i = 26; i < 33; i++)
    {
      DrawBigText(6, 18, TEXT_SLIDE_IN[i], 16);
      WaitTicks(10);
    }

    WaitTicks(15);
    PlaySound(SND_BIG_EXPLOSION);
  }
  else // we got a bonus
  {
    // Play the text slide-in animation ("Secret Bonus")
    for (i = 0; i < 6; i++)
    {
      DrawBigText(6, 18, TEXT_SLIDE_IN[i], 16);
      WaitTicks(5);
    }

    // Now add the bonus number next to the text, since it's not part of the
    // slide-in animation
    DrawBigNumberGrey(34, 18, (word)bonusNum);

    PlaySound(SND_BIG_EXPLOSION);
    WaitTicks(190);

    // Now add the bonus points to the player's score, animating the process
    DrawBigText(6, 18, "  100000 PTS  ", 16);
    WaitTicks(100);

    // Add score in steps of 1000, 70 steps per second
    for (i = 0; i < 100; i++)
    {
      plScore += 1000;
      WaitTicks(2);

      DrawBigNumberGrey(34, 9, plScore);
      DrawBigNumberGrey(22, 18, 99000L - (long)i * 1000L);

      PlaySound(SND_DUKE_JUMPING);
    }

    DrawBigText(6, 18, "       0 PTS  ", 16);

    PlaySound(SND_BIG_EXPLOSION);
    WaitTicks(50);
  }
}


/** Show the bonus screen
 *
 * This function only returns once the bonus screen is finished. It implements
 * both the visuals, as well as the logic, i.e. determining which bonuses apply
 * and modifying the player's score accordingly.
 */
void ShowBonusScreen(void)
{
  ibool gotBonus = false;
  byte far* musicBuffer;

  FadeOutScreen();

  // Draw the background
  DrawFullscreenImage("Bonusscn.mni");

  // Play music, if applicable
  if (gmCurrentLevel < 7) // not a boss level
  {
    // Music playback is skipped when showing the bonus screen after an
    // episode's last level, since different music is already playing at that
    // time (which is started by ShowEpisodeEndScreen() in main.c).
    musicBuffer = MM_PushChunk(
      GetAssetFileSize("OPNGATEA.IMF"),
      CT_TEMPORARY);
    PlayMusic("OPNGATEA.IMF", musicBuffer);
  }

  DrawBigText(6, 9, "SCORE", 16);
  DrawBigNumberGrey(34, 9, plScore);

  FadeInScreen();

  WaitTicks(60);

  //
  // Go through all 7 bonuses and grant those that apply
  //

  // Bonus 1
  if (gmCamerasDestroyed == gmCamerasInLevel && gmCamerasDestroyed)
  {
    gotBonus = true;
    ApplyBonus(1);
  }

  // Bonus 2
  if (!gmPlayerTookDamage)
  {
    gotBonus = true;
    ApplyBonus(2);
  }

  // Bonus 3
  if (gmWeaponsCollected == gmWeaponsInLevel && gmWeaponsCollected)
  {
    gotBonus = true;
    ApplyBonus(3);
  }

  // Bonus 4
  if (gmMerchCollected == gmMerchInLevel && gmMerchCollected)
  {
    gotBonus = true;
    ApplyBonus(4);
  }

  // Bonus 5
  if (gmTurretsDestroyed == gmTurretsInLevel && gmTurretsDestroyed)
  {
    gotBonus = true;
    ApplyBonus(5);
  }

  // [BUG] ?? Unlike the other bonuses, 6 and 7 are granted if the level never
  // contained any bomb boxes or bonus globes (orbs) to begin with. Not sure if
  // this is intentional or an oversight.

  // Bonus 6
  if (gmBombBoxesLeft == 0)
  {
    gotBonus = true;
    ApplyBonus(6);
  }

  // Bonus 7
  if (gmOrbsLeft == 0)
  {
    gotBonus = true;
    ApplyBonus(7);
  }

  // If no bonus was given, show a "No bonus" message
  if (gotBonus == false)
  {
    // '0' means 'no bonus'
    ApplyBonus(0);
  }

  WaitTicks(425); // roughly 3 seconds

  // Stop music playback, if applicable
  if (gmCurrentLevel < 7) // not a boss level
  {
    // [BUG] This has a race condition, since the music playback might still
    // run between these two function calls. It's not a problem in practice
    // since the memory is still there until something else overwrites it, but
    // it's still incorrect. The two calls should be swapped.
    MM_PopChunk(CT_TEMPORARY);
    StopMusic();
  }
}
