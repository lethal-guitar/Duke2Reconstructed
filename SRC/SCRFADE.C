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

Screen fade-out/fade-in functions

When in 16-color mode, these functions are the only place where the game changes
the VGA palette. In other words, palette changes are always done as part of a
fade-out & fade-in transition. This makes sense, since it avoids artifacts that
could occur when changing the palette while still showing something on screen.
But it's interesting that there isn't even a dedicated function to set the
palette. Instead, to do a palette change, client code needs to fade out, load
the desired palette into the gfxCurrentPalette array, and then fade in. Also
worth noting is that a fade-out always resets gfxCurrentPalette to the default
in-game palette.

As a result of the way the fading is implemented, the game's 16-color palettes
are in a slightly unusual format in that their values range from 0 to 68
inclusive, instead of the 0 to 63 range accepted by VGA cards.  To convert from
this range to the normal range, do: value*15 / 16

*******************************************************************************/


/** Fade in using the specified palette
 *
 * Smoothly transitions into the specified target palette over a period of time.
 * The VGA palette should be all black (0) before using this function.
 */
static void FadeInToPalette(byte* palette) { int srcIndex; int i; int j;

  // Zero out temporary palette accumulation buffer
  for (i = 0; i < 16*3; i++)
  {
    gfxPaletteForFade[i] = 0;
  }

  // Fade in over 15 steps
  for (i = 0; i < 15; i++)
  {
    srcIndex = 0;

    // [NOTE] All other fade functions have a call to AwaitVblank() here.  I'm
    // not sure why it was left out in this one.

    // Submit intermediate palette for the current step to the hardware
    for (j = 0; j < 16; j++)
    {
      // Add target palette value to the accumulation buffer.
      // During the last step, the accumulation buffer holds (val * 15) for each
      // entry in the palette.
      gfxPaletteForFade[srcIndex]     += palette[srcIndex];
      gfxPaletteForFade[srcIndex + 1] += palette[srcIndex + 1];
      gfxPaletteForFade[srcIndex + 2] += palette[srcIndex + 2];

      // Submit the current state of the accumulation buffer, but divide every
      // value by 16. The final palette loaded sent to the hardware during the
      // last step will thus be (value * 15 / 16) for each entry in the palette
      // argument.
      SetPaletteEntry_16(
        j,
        gfxPaletteForFade[srcIndex]     >> 4,
        gfxPaletteForFade[srcIndex + 1] >> 4,
        gfxPaletteForFade[srcIndex + 2] >> 4);

      srcIndex += 3;
    }

    WaitTicks(2);
  }
}


/** Apply one step of the fade-in for the Duke3D teaser palette
 *
 * Unlike the other functions in this file, this function doesn't do the entire
 * fade-in, just a single step. This allows the client code to animate a moving
 * sprite at the same time. See ShowDuke3dTeaserScreen() in main.c.
 */
void Duke3dTeaserFadeIn(byte step)
{
  int srcIndex;
  int i;
  int j;

  // Zero out temporary palette accumulation buffer on the first step
  if (!step)
  {
    for (j = 0; j < 3 * 16; j++)
    {
      gfxPaletteForFade[j] = 0;
    }
  }

  srcIndex = 0;

  AwaitVblank();

  // Update accumulation buffer and submit results to hardware, this works the
  // same as in FadeInToPalette().
  for (i = 0; i < 16; i++)
  {
    gfxPaletteForFade[srcIndex]     += DUKE3D_TEASER_PALETTE[srcIndex];
    gfxPaletteForFade[srcIndex + 1] += DUKE3D_TEASER_PALETTE[srcIndex + 1];
    gfxPaletteForFade[srcIndex + 2] += DUKE3D_TEASER_PALETTE[srcIndex + 2];

    SetPaletteEntry_16(
      i,
      gfxPaletteForFade[srcIndex]     >> 4,
      gfxPaletteForFade[srcIndex + 1] >> 4,
      gfxPaletteForFade[srcIndex + 2] >> 4);

    srcIndex += 3;
  }

  WaitTicks(2);
}


/** Fade out from the specified palette
 *
 * Smoothly transitions into an all-black palette over a period of time.
 * The current VGA palette should be identical to the one given to this
 * function.
 */
static void FadeOutFromPalette(byte* palette)
{
  int i;
  int j;
  int srcIndex;

  // Initialize accumulation buffer to (value * 16). On the first loop iteration
  // below, the palette will be subtracted once from the buffer values before
  // submitting to the hardware, so the first submission of the buffer will be
  // identical to the currently loaded palette (see FadeInToPalette()).
  for (i = 0; i < 3*16; i += 3)
  {
    gfxPaletteForFade[i]     = palette[i]     << 4;
    gfxPaletteForFade[i + 1] = palette[i + 1] << 4;
    gfxPaletteForFade[i + 2] = palette[i + 2] << 4;
  }

  // The first step loads an identical version of the palette that's already
  // loaded into the hardware, so we need 16 steps instead of 15 to actually end
  // up at 0 at the end.
  //
  // [NOTE] This is a bit non-obvious, it would be clearer to initialize the
  // accumulation buffer with (value * 15) and then just do 15 steps as usual.
  for (i = 0; i < 16; i++)
  {
    srcIndex = 0;

    AwaitVblank();

    // Update accumulation buffer and submit results to hardware, this works the
    // same as in FadeInToPalette() except that we subtract the palette on each
    // iteration.
    for (j = 0; j < 16; j++)
    {
      gfxPaletteForFade[srcIndex]     -= palette[srcIndex];
      gfxPaletteForFade[srcIndex + 1] -= palette[srcIndex + 1];
      gfxPaletteForFade[srcIndex + 2] -= palette[srcIndex + 2];

      SetPaletteEntry_16(
        j,
        gfxPaletteForFade[srcIndex]     >> 4,
        gfxPaletteForFade[srcIndex + 1] >> 4,
        gfxPaletteForFade[srcIndex + 2] >> 4);

      srcIndex += 3;
    }

    WaitTicks(2);
  }

  // Clear the screen after a fade-out - or at least parts of it, depending on
  // if we're currently playing back the demo or not.
  if (gmCurrentEpisode < 4)
  {
    // This clears only the outer parts of the screen - those which have black
    // bars during gameplay. I have no clue why.

    SetDrawPage(gfxCurrentDisplayPage);

    // Top row
    FillScreenRegion(SFC_BLACK, 0, 0, SCREEN_WIDTH_TILES - 1, 0);

    // Left-most column
    FillScreenRegion(SFC_BLACK, 0, 1, 0, SCREEN_HEIGHT_TILES - 1);

    // Right-most column
    FillScreenRegion(
      SFC_BLACK,
      SCREEN_WIDTH_TILES - 1, 1,
      SCREEN_WIDTH_TILES - 1, SCREEN_HEIGHT_TILES - 1);

    SetDrawPage(!gfxCurrentDisplayPage);

    // Same as above
    FillScreenRegion(SFC_BLACK, 0, 0, SCREEN_WIDTH_TILES - 1, 0);
    FillScreenRegion(SFC_BLACK, 0, 1, 0, SCREEN_HEIGHT_TILES - 1);
    FillScreenRegion(
      SFC_BLACK,
      SCREEN_WIDTH_TILES - 1, 1,
      SCREEN_WIDTH_TILES - 1, SCREEN_HEIGHT_TILES - 1);
  }
  else // demo episode
  {
    // Clear the whole screen.
    SetDrawPage(1);
    CLEAR_SCREEN();

    SetDrawPage(0);
    CLEAR_SCREEN();
  }

  // Clear any in-progress HUD message
  hudMessageCharsPrinted = 0;
  hudMessageDelay = 0;
  hudShowingHintMachineMsg = false;

  // Reset back to the in-game palette.
  //
  // [NOTE] It's a bit odd to do this here, since the game does many fade
  // transitions into screens which do not use this palette. So it will be
  // overwritten again right after.  The only time that this built-in palette is
  // needed is when transitioning from the loading screen or an in-game menu
  // into gameplay, and it would be easy enough to explicitly load it only in
  // those places.
  for (j = 0; j < 16 * 3; j++)
  {
    gfxCurrentPalette[j] = INGAME_PALETTE[j];
  }
}


/** Switch palette to gfxCurrentPalette and fade in */
void FadeInScreen(void)
{
  FadeInToPalette(gfxCurrentPalette);
}


/** Fade screen to black, set in-game palette */
void FadeOutScreen(void)
{
  FadeOutFromPalette(gfxCurrentPalette);
}
