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

Music playback code and timer interrupt

The music playback and PC Speaker/AdLib sound playback are driven by a timer
interrupt, in order for music and sound to run concurrently with the game code.
The same timer interrupt also serves as the game's heartbeat, which is used to
implement timing functions.

A fair bit of the code here is very similar or even identical to code found in
Cosmo, which in turn is similar to Id Software code from Catacomb 3D.

*******************************************************************************/


// [NOTE] This being a variable and not a define is probably a relic of the
// Cosmo codebase, which uses different timer frequencies depending on if music
// is enabled or not.
static word sysTimerFrequency = TIMER_FREQUENCY;


/** Set specified AdLib hardware register to the given value
 *
 * This function is almost identical to the version found in the DIGISND
 * library, and similar to the one found in basicsnd.c. In total, the
 * game includes three different versions of this routine.
 */
static void WriteAdLibReg(byte reg, byte val)
{
  // Lock
  asm pushf
  asm cli

  // Write address register
  asm mov   dx, 0x388
  asm mov   al, [reg]
  asm out   dx, al

  // Wait for at least 3.3 usecs by executing 6 IN instructions
  // (as recommended in the AdLib documentation)
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx

  // Write data register
  asm mov   dx, 0x389
  asm mov   al, [val]
  asm out   dx, al

  asm popf

  // Wait for at least 23 usecs by executing 35 IN instructions
  // (as recommended in the AdLib documentation)
  asm mov   dx, 0x388
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx

  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx

  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx

  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx

  // [NOTE] This is unnecessary, as the popf above
  // already re-enabled interrupts.
  enable();
}


/** Configure PIT channel 0 with the given counter value
 *
 * Configures the Programmable Interval Timer channel 0 with the given counter
 * value. The timer counts down at a fixed rate of 1,193,182 Hz, and fires
 * interrupt 8 every time the counter reaches 0. It then resets the counter to
 * the configured value and the process repeats.
 *
 * To achieve a desired interrupt rate measured in interrupts per second, the
 * counter value needs to be set to 1193182 / desiredFrequency, which
 * is (kind of) done in SetupTimerFrequency() below.
 */
static void pascal SetPIT0Value(int value)
{
  /*
  Bit Pattern | Interpretation
  ------------|---------------
  00xxxxxx    | Select timer channel 0
  xx11xxxx    | Access Mode: "Low byte, followed by high byte"
  xxxx011x    | Mode 3: Square wave generator
  xxxxxxx0    | 16-bit binary counting mode
  */
  DN2_outportb(0x43, 0x36);

  /* PIT counter 0 divisor (low byte, high byte) */
  DN2_outportb(0x40, value);
  DN2_outportb(0x40, value >> 8);
}


/** Feed pending music commands to AdLib hardware */
static void MusicService(void)
{
  int data;

  if (!musicIsPlaying)
  {
    return;
  }

  // If enough time has passed for the next event to be due, keep reading
  // events and feeding them to the AdLib hardware until we encounter an event
  // with a non-zero delay value.
  while (musicDataLeft != 0 && musicNextEventTime <= musicTicksElapsed)
  {
    // Each event consists of two bytes of AdLib data (register + value),
    // and a word indicating the delay until the next event.
    // Here, we're reading both of the first two data bytes as a single word.
    data = *musicData++;

    // Extract the delay value, and update next event time
    musicNextEventTime = musicTicksElapsed + *musicData++;

    // Submit the data
    WriteAdLibReg(data, data >> 8);

    musicDataLeft -= 4;
  }

  musicTicksElapsed++;

  // [HACK] For some reason, the length of the Apogee logo movie is determined
  // by the music playback instead of a number of ticks. This code here sets a
  // flag whenever there are less than 40 bytes of music data left to play.
  // AwaitNextFrame() in video2.c polls this variable and interrupts the video
  // playback as if a key was pressed.
  // For the intro movie, this condition never occurs, because the movie ends
  // before the music is finished. But the Apogee logo is set to repeat 255
  // times, which means it'll definitely still be playing when the Apogee
  // fanfare ends.
  // This seems like a pretty odd way to do things, I guess the idea was to
  // sync the end of video playback precisely to the end of the fanfare.  But
  // since music playback is driven by the timer interrupt, which also updates
  // the tick counter, the exact same result can easily be achieved in a more
  // robust way by using the tick counter directly to control the duration of
  // the video playback.
  if (musicDataLeft > 40)
  {
    hackStopApogeeLogo = false;
  }
  else
  {
    hackStopApogeeLogo = true;
  }

  // Once the entire song has been played, loop back to the beginning
  if (musicDataLeft == 0)
  {
    musicData = musicDataStart;
    musicDataLeft = musicDataSize;
    musicTicksElapsed = musicNextEventTime = 0;
  }
}


/** Timer interrupt service routine
 *
 * This function is invoked 280 times per second via the timer interrupt.
 * It drives music and (non-digitized) sound playback, updates the tick counter
 * used for timing, and also draws the progress bar during loading screens.
 */
static void interrupt TimerInterruptHandler(void)
{
  int i;

  sysIsSecondTick = !sysIsSecondTick;
  sysFastTicksElapsed++;

  // Run the music service at full speed, i.e. 280 Hz
  if (AdLibPresent && sndMusicEnabled)
  {
    MusicService();
  }

  // Everything else only needs to run at 140 Hz, so we only do it every other
  // timer interrupt.
  if (sysIsSecondTick)
  {
    // Increment the game's global tick counter - this is the game's
    // heartbeat, which all timing-related functionality is built on.
    sysTicksElapsed++;

    // Drive non-digitized sound effect playback (see basicsnd.c).
    // Digitized sound effects use DMA and thus don't need to be driven via the
    // timer interrupt (see digisnd.c).
    PcSpeakerService();
    AdLibSoundService();

    //
    // Loading screen progress bar
    //
    uiProgressBarTicksElapsed++;

    // The client code making use of the progress bar is LoadLevel() in main.c.
    // Handling the progress bar here in the interrupt handler is necessary in
    // order to make it run smoothly while the loading is happening.
    //
    // uiProgressBarState represents the filled in length of the bar in 1/4ths
    // of a pixel.  Setting it to 1 makes the progress bar active. The progress
    // bar advances by one quarter of a pixel every uiProgressBarStepDelay
    // ticks, until it reaches its maximum length.
    if (
      uiProgressBarState != 0 &&
      uiProgressBarTicksElapsed >= uiProgressBarStepDelay &&
      uiProgressBarState < 284)
    {
      // These color indices are meant for use with the loading screen
      // background palette, where they result in different shades of red.
      const byte PROGRESS_BAR_COLORS[] = { 12, 11, 10, 11 };

      // Reset tick counter
      uiProgressBarTicksElapsed = 0;

      // Advance by 1/4 of a pixel
      uiProgressBarState++;

      // Draw vertical strip of pixels, this makes the progress bar visually
      // advance by one pixel every 4th tick. The rest of the time it just
      // overdraws the right-most strip of pixels that's already visible.
      //
      // [NOTE] The EGA map mask needs to be set to write to all planes
      // simultaneously in order for SetPixel to work correctly, which is not
      // done here. It still works fine because the background that the pixels
      // are drawn onto is black.
      for (i = 0; i < 4; i++)
      {
        SetPixel(108 + (uiProgressBarState >> 2), 105 + i, PROGRESS_BAR_COLORS[i]);
      }

      // [NOTE] This is unnecessary. SetPixel doesn't change the write mode,
      // and it already sets the bitmask to default before returning.
      EGA_SET_DEFAULT_MODE();
      EGA_SET_DEFAULT_BITMASK();
    }
  }

  // Invoke the original timer interrupt service routine at roughly the
  // original rate. The PIT0 timer is already in use by the system (BIOS/DOS)
  // on the IBM PC, serving various important functions like updating the
  // computer's clock, stopping floppy drive motors etc. Therefore, it's
  // important to keep invoking the original handler, which was saved when
  // installing our own custom handler. The original handler expects to be
  // invoked at a much slower rate though, namely 18.2 Hz which is the default
  // frequency of the PIT0 timer. So we can't simply invoke the original
  // handler every time, we have to do it in time intervals that roughly match
  // the original timer frequency. Here, the way this is done is to invoke the
  // original handler every 16th invocation (by using modulo). 280 Hz / 16 is
  // 17.5 Hz, which is roughly in the right ballpark. Unfortunately, that's not
  // quite good enough:
  //
  // [BUG] Since it's only roughly right, the computer's clock will run too
  // slowly while the game is running. It won't be that noticeable after a
  // short period of time, but when playing the game for a couple of hours, the
  // clock will be off by a few minutes.  The issue is that doing a simple
  // modulo doesn't account for the error introduced by using a timer frequency
  // that's not an even multiple of the original one. It's possible to track
  // the error and take it into account, which will result in a correct ratio.
  // But that wasn't done here. Interestingly, Cosmo actually does it correctly
  // in its timer interrupt handler:
  //
  // https://github.com/smitelli/cosmore/blob/2891cb6717e5f6bbe85afdb87cd7d0c9ef376147/src/game2.c#L445
  //
  // It's not clear why the code was changed for Duke 2. My best guess is that
  // the code used in Cosmo came from Id Software or some other source, and was
  // replaced with the version here because the authors didn't fully understand
  // the original code and assumed that it could be replaced with a simpler
  // version.
  if (!(sysFastTicksElapsed % 16))
  {
    sysSavedTimerIntHandler();
  }
  else
  {
    // If we didn't invoke the original handler, we need to acknowledge the
    // interrupt ourselves.
    DN2_outportb(0x20, 0x20);
  }
}


/** Configure the PIT timer to run at the game's desired frequency */
static void SetupTimerFrequency()
{
  // The constant used here is incorrect - it should be 1193182 instead.
  // This incorrect constant is used fairly widely in various games from
  // this era, for unknown reasons.
  // See https://cosmodoc.org/topics/adlib-functions/#SetInterruptRate for
  // a more in-depth discussion.
  SetPIT0Value(1192030L / sysTimerFrequency);
}


/** Stops all AdLib channels used for music */
void ResetAdLibMusicChannels(void)
{
  int i;

  // Silence any rhythm mode instruments (none of the music files shipping with
  // the game make use of rhythm mode)
  WriteAdLibReg(0xBD, 0);

  // Set the "key on" bit to 0 for AdLib channels 2 to 9. This stops any
  // currently playing notes on those channels, but doesn't necessarily stop
  // audio output, depending on the release envelope setting for the instrument.
  // If audio output keeps going, it will be at a fairly low frequency, since
  // setting the register to 0 doesn't just alter the "key on" bit, it also
  // changes the octave (aka "block") to the lowest one and unsets the two most
  // significant bits of the channel's current frequency value.
  //
  // Channel 1 is reserved for sound effects, and thus left unchanged.
  //
  // [BUG] The loop should only go from 0 to 7, i.e. the condition should be
  // `i < 8`. There are only 9 AdLib channels in total, with 0xB8 addressing
  // the 9th channel. The loop thus ends up writing two non-existant registers,
  // but this doesn't seem to cause any issues.
  for (i = 0; i < 10; i++)
  {
    WriteAdLibReg(0xB1 + (byte)i, 0);
  }
}


/** Stop music playback (private version) */
static void StopMusic_Internal(void)
{
  ResetAdLibMusicChannels();
  musicIsPlaying = false;
}


/** Setup timer frequency and interrupt handler, replacing default handler */
void InstallTimerInterrupt(void)
{
  sysSavedTimerIntHandler = getvect(8);
  setvect(8, TimerInterruptHandler);

  musicTicksElapsed = 0;

  SetupTimerFrequency();
}


/** Restore default timer interrupt handler and timer frequency */
void RestoreTimerInterrupt(void)
{
  StopMusic_Internal();

  // 0 is the default PIT 0 counter value, which results in an interrupt rate
  // of roughly 18.2 Hz.
  SetPIT0Value(0);

  setvect(8, sysSavedTimerIntHandler);
}


#include "joystk2.c"


/** Start playing back music stored in `data`.
 *
 * The length of the music data must be set via the global variable
 * sndCurrentMusicFileSize _before_ calling this function.
 *
 * [NOTE] This could've been made safer by passing the size as another
 * argument.
 */
void pascal StartMusicPlayback(int far* data)
{
  StopMusic_Internal();

  musicData = musicDataStart = data;
  musicDataSize = musicDataLeft = sndCurrentMusicFileSize;
  musicNextEventTime = 0;
  musicTicksElapsed = 0;
  musicIsPlaying = true;
}


/** Load music from file and start playing it
 *
 * `buffer` must point to a block of memory large enough to hold all of the
 * music data. The game uses different memory blocks for music depending on the
 * situation, which necessitates this design.
 *
 * [NOTE] This makes the function a bit awkward to use, since it does the file
 * loading by itself but the caller is responsible for allocating the buffer.
 * It would seem easier to let the caller handle both allocation and loading,
 * and then pass the already filled buffer and size to this function.
 */
void pascal PlayMusic(char far* filename, void far* buffer)
{
  if (!AdLibPresent)
  {
    return;
  }

  LoadAssetFile(filename, buffer);

  // [NOTE] This is usually redundant, since the caller already needs to figure
  // out the size of the file in order to allocate a large enough buffer.
  // Passing the size as an argument to this function would seem better.
  sndCurrentMusicFileSize = GetAssetFileSize(filename);

  // [HACK] This is a bit hacky and complicates some code elsewhere (see e.g.
  // ShowEpisodeEndScreen() in main.c). It would be much better to have this
  // condition in the place where the logic is important, which is in
  // LoadLevel()/StartLevel().
  if (gmCurrentLevel < 7) // not a boss level
  {
    StartMusicPlayback(buffer);
  }
}


/** Stop music playback */
void StopMusic(void)
{
  if (!AdLibPresent)
  {
    return;
  }

  StopMusic_Internal();
}
