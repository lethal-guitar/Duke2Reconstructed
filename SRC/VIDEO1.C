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

Video playback system - timing control and sound effects

Duke Nukem II uses the FLIC file format for its video files. This format only
stores images, no audio track. The game therefore needs dedicated code to
trigger sound effect playback.

FLIC files contain information about the intended playback speed, but they can
only specify a single speed per file. A major part of the intro video uses
variable speed, including several multi-second freeze-frames. To achieve this,
the game completely ignores the speed information from the video files, and
instead dynamically changes the playback speed every couple of frames.

The OnNewVideoFrame function implements this. Every video file that's played
back is also given an id within the code via the VideoType enum. After each
frame of video that's displayed, OnNewVideoFrame is invoked with the id and the
frame number.  Depending on these values, the function then triggers sound
effects and adjusts the playback speed.

*******************************************************************************/


void OnNewVideoFrame(word videoType, int frame)
{
  switch (videoType)
  {
    case VT_APOGEE_LOGO:
      flicNextDelay = 8; // 125 ms
      break;

    case VT_NEO_LA:
      flicNextDelay = 4; // 250 ms
      break;

    case VT_UNUSED_1:
    case VT_RANGE_1:
      switch (frame)
      {
        case 0:
          flicNextDelay = 20; // 50 ms
          PlaySound(SND_INTRO_GUNSHOT1);
          break;
      }
      break;

    case VT_UNUSED_2:
    case VT_RANGE_2:
      switch (frame)
      {
        case 0:
        case 3:
        case 6:
          flicNextDelay = 12; // ~83.3 ms
          PlaySound(SND_INTRO_GUNSHOT2);
          break;
      }
      break;

    case VT_RANGE_3:
      switch (frame)
      {
        case 0:
          PlaySound(SND_INTRO_SHELLS_CLATTER);
          flicNextDelay = 6; // ~166.6 ms
          break;

        case 7:
          PlaySound(SND_INTRO_REEL_IN_TARGET);
          // fallthrough (unclear if intended)

        case 17:
          flicNextDelay = 6; // ~166.6 ms
          break;

        case 23:
          flicNextDelay = -2; // 2 s
          break;

        case 24:
          flicNextDelay = 6; // ~166.6 ms
          break;

        case 31:
          flicNextDelay = -2; // 2 s
          PlaySound(SND_INTRO_TARGET_STOPS);
          break;

        case 32:
          flicNextDelay = -1; // 1 s
          break;

        case 33:
          flicNextDelay = 5; // 200 ms
          PlaySound(SND_INTRO_DUKE_SPEAKS_1);
          break;

        case 37:
          PlaySound(SND_INTRO_DUKE_SPEAKS_2);
          break;

        case 39:
          flicNextDelay = -1; // 1 s
          break;

        case 40:
          flicNextDelay = 17; // ~ 58.8 ms
          break;

        case 49:
          flicNextDelay = -1; // 1 s
          PlaySound(SND_BIG_EXPLOSION);
          break;

        case 50:
          flicNextDelay = 17; // ~ 58.8 ms
          break;

        case 55:
          flicNextDelay = -4; // 4 s
          PlaySound(SND_BIG_EXPLOSION);
          break;

      }
  }

  if (flicNextDelay < 0)
  {
    // A negative value denotes a delay in seconds ...
    flicFrameDelay = DN2_abs(flicNextDelay * TIMER_FREQUENCY);
  }
  else
  {
    // ... a positive one is fractions of a second (1/x seconds)
    flicFrameDelay = TIMER_FREQUENCY / flicNextDelay;
  }
}
