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

#include "basicsnd.h"

#include <stdlib.h>


/*******************************************************************************

AdLib and PC Speaker sound effect playback library, using Id Software's
AUDIOT format.

This code has a lot of similarities with sound playback code in Wolfenstein 3D.
I therefore used mostly the same variable and constant names, and took over some
of the comments from the latter.  Any references made to functions when talking
about similarities in the code refer to the Wolfenstein 3D source:

https://github.com/id-Software/wolf3d/blob/master/WOLFSRC/ID_SD.C

It's worth noting that the Wolfenstein code is an evolution of code that Id had
already been using in Commander Keen, Catacomb 3D etc., so in some cases, the
code is also quite similar to code found in those games.

There are also some differences. We don't know the exact history of this code,
while it could be Id code that was shared with Apogee, it's also possible that
it's in-house Apogee code developed to be compatible with Id's file formats.
The code produces identical results on all three compilers used for the project,
i.e. Borland C++ 3.0, Borland C++ 3.1, and Turbo C++ 3.0, so this unfortunately
doesn't give any clues as to the exact nature of this code. It could've been
a library or it could've been built alongside the game's source code.

*******************************************************************************/


// Operator-specific AdLib registers
#define alChar    0x20 // "Character" - Tremolo, vibrato, envelope generator,
                       // key scaling (rate), frequency multiplier
#define alScale   0x40 // Total level, key scaling (level)
#define alAttack  0x60 // Attack, decay
#define alSustain 0x80 // Sustain, release
#define alWave    0xE0 // Waveform select

// Channel-specific AdLib registers
#define alFreqL   0xA0 // Frequency number - low byte
#define alFreqH   0xB0 // Frequency number - high byte, key on, block
#define alFeedCon 0xC0 // Feedback depth, connection type


//
// Extra utility macros
//

// I don't think these macros existed in the original source code, the Wolf3D
// code for example directly uses inline assembly for these without macros.
// But it helps readability and makes understanding the code easier, so
// I decided to add them.


// The sound playback code runs concurrently to the main code, via the timer
// interrupt. Code that runs on the "main thread" therefore needs to be
// protected against data races when accessing shared state. This is
// accomplished by temporarily disabling interrupts via the CLI Assembly
// instruction. The PUSHF and POPF instructions save and restore CPU flags,
// which re-enables interrupts when unlocking.
#define LOCK()   asm { pushf; cli }
#define UNLOCK() asm { popf; }


// Turn off PC Speaker
//
// This reads I/O port 0x61, unsets the two least significant bits, and
// then writes back the value. Port 0x61 is actually the control register
// for the PC's keyboard controller, but as it turns out, it also plays a
// role in controlling the PC speaker. The PC Speaker is driven by the
// output of the PIT (programmable interval timer) channel 2, but there's
// a gate in-between which can be used to interrupt the signal. Bit 1
// of this control register turns this gate on or off, essentially muting
// the speaker but leaving the timer running. Bit 0, on the other hand,
// disables the timer itself.
// Here, both are disabled.
#define DISABLE_SPEAKER() asm {   \
  in  al,   0x61;                 \
  and al,   0xFC; /* 11111100b */ \
  out 0x61, al;                   \
}

// This works exactly like DISABLE_SPEAKER, except that it's setting the bits.
#define ENABLE_SPEAKER() asm {    \
  in  al,   0x61;                 \
  or  al,   0x03; /* 00000011b */ \
  out 0x61, al;                   \
}


//
// Globals
//

// PC Speaker state
static int       pcSoundPriority;
static ibool     pcUseLookupTable;
static word      pcSoundLookup[256];
static word      pcLastSample;
static word far* pcSoundData;
static long      pcLengthLeft;

// AdLib state
static int       alSoundPriority;
static byte far* alSoundData;
static word      alBlock;
static long      alLengthLeft;


/** Stop any currently playing PC Speaker sound effect
  *
  * Resembles SDL_ShutPC() from Wolf3D, mixed with aspects of
  * SDL_SoundFinished() and SDL_PCStopSound().
  */
void StopPcSpeakerSound(void)
{
  LOCK();

  DISABLE_SPEAKER();

  pcSoundData = NULL;
  pcLengthLeft = 0;
  pcSoundPriority = 0;
  pcLastSample = 0;

  UNLOCK();
}


/** Initialize PC Speaker sound playback */
void InitPcSpeaker(ibool useLookupTable, word factor)
{
  int i;
  word value;

  StopPcSpeakerSound();

  pcUseLookupTable = useLookupTable;

  // The lookup table is essentially a list of i*factor, i.e. a multiplication
  // table. Multiplication was still relatively slow on CPUs of the time, so
  // this gave a speed advantage.
  if (useLookupTable)
  {
    value = 0;

    for (i = 0; i < 256; i++)
    {
      pcSoundLookup[i] = value;
      value += factor;
    }
  }
}


/** PC Speaker playback service routine
  *
  * This must be called at 140 Hz to keep PC Speaker playback going. The game
  * invokes this from its timer interrupt handler (found in music.c).
  *
  * Resembles SDL_PCService from Wolf3D, but has been extended to support both
  * the older and newer sound effect data formats used by Id Software. The newer
  * format uses a sequence of bytes and requires a multiplication (implemented
  * via lookup table) to derive playable sample values, while the older format
  * directly stores word-sized sample values.
  */
void PcSpeakerService(void)
{
  byte byteSample;
  word sample;
  byte far* byteWiseData;

  if (pcSoundData)
  {
    // Determine the new sample value, either using the lookup table or fetching
    // it directly, and advance the data pointer
    if (pcUseLookupTable)
    {
      byteWiseData = (byte far*)pcSoundData;
      byteSample = *byteWiseData++;

      // byteWiseData was initialized to pcSoundData and then incremented, so
      // this assignment advances pcSoundData (a word pointer) by one byte.
      pcSoundData = (word far*)byteWiseData;

      sample = pcSoundLookup[byteSample];
    }
    else
    {
      sample = *pcSoundData++;
    }

    // Now play back the sample if it's different from the previous one
    if (sample != pcLastSample)
    {
      pcLastSample = sample;

      if (sample)
      {
        // Load the sample value into the BX register. The low and high byte
        // can be accessed via the BL and BH registers, respectively.
        asm mov bx,   [sample]

        // Load sample into the PIT (programmable interval timer), channel 2,
        // which controls the PC speaker. The sample is the counter value to be
        // set in the timer in order to achieve a desired frequency from the
        // speaker. I/O port 0x43 is the PIT's control register, 0x42 is the
        // data register for channel 2.

        // Bits      | Interpretation
        // ----------|---------------
        // 10xxxxxx  | Select timer channel 2
        // xx11xxxx  | Access Mode: "Low byte, followed by high byte"
        // xxxx011x  | Mode 3: Square wave generator
        // xxxxxxx0  | 16-bit binary counting mode
        asm mov al,   0xb6
        asm out 0x43, al

        // High byte
        asm mov al,   bl
        asm out 0x42, al

        // Low byte
        asm mov al,   bh
        asm out 0x42, al

        ENABLE_SPEAKER();
      }
      else // Sample is 0, turn speaker off
      {
        DISABLE_SPEAKER();
      }
    }

    // Check if we've reached the end of the sound, and stop if so
    if (!(--pcLengthLeft))
    {
      StopPcSpeakerSound();
    }
  }
}


/** Start playback of the given PC Speaker sound
  *
  * This function only sets up state and then returns immediately. The actual
  * playback happens asynchronously via PcSpeakerService().
  */
void PlayPcSpeakerSound(PCSound far* sound, long length)
{
  if (sound->priority >= pcSoundPriority)
  {
    StopPcSpeakerSound();

    LOCK();

    // [NOTE] It's strange that the struct size is subtracted from the size
    // parameter. There's no good reason for this, and it forces the client code
    // to add the size before calling this function (see PlayBasicSound() in
    // sound.c).  It would seem much simpler for the client code to simply pass
    // in a pointer to the start of the data from the AUDIOT package, and
    // extract the size value here instead of having the client code do it. Not
    // sure what lead to this strange arrangement.
    pcLengthLeft = length - sizeof(PCSound);

    if (!pcUseLookupTable)
    {
      // When using the old format without a lookup table, the sound data is
      // word-sized. Divide by 2 to convert the size in bytes to a number of
      // words.
      pcLengthLeft /= 2;
    }

    pcSoundPriority = sound->priority;

    // The sound data starts after the PCSound struct header
    pcSoundData = (word far*)(sound + 1);

    UNLOCK();
  }
}


/** Test if a PC Speaker sound effect is currently playing
  *
  * Due to the concurrent nature of the sound playback, this function cannot
  * always reflect the actual state accurately - by the time the client code
  * makes use of the return value, the actual playback state might have already
  * changed.
  */
ibool IsPcSpeakerPlaying(void)
{
  dword lengthLeft;

  LOCK();

  lengthLeft = pcLengthLeft;

  UNLOCK();

  return lengthLeft != 0;
}


/** Send a command to the AdLib hardware
  *
  * Similar to alOut() from Wolf3D, but uses a loop instead of individual
  * instructions for the 2nd busy wait, making this the most space-efficient
  * version of this code in the executable. This function is one of three
  * versions of this, compare music.c and digisnd.c.
  */
static void WriteAdLibReg(byte reg, byte val)
{
  // Lock
  asm pushf
  asm cli

  // Write to the address register
  asm mov   dx, 0x388
  asm mov   al, [reg]
  asm out   dx, al

  // Wait for at least 3.3 usecs by executing a couple of IN instructions
  // (as recommended in the AdLib documentation)
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx
  asm in    al, dx

  // Write to the data register
  asm inc   dx
  asm mov   al, [val]
  asm out   dx, al

  // Unlock
  asm popf

  // Back to the address register
  asm dec   dx

  // Wait for at least 23 usecs by executing 35 IN instructions
  // (as recommended in the AdLib documentation)
  asm mov   cx, 35
waitLoop:
  asm in    al, dx
  asm loop  waitLoop
}


/** Stop any currently playing AdLib sound effect
  *
  * A combination of SDL_ALStopSound() and SDL_SoundFinished() from Wolf3D.
  */
void StopAdLibSound(void)
{
  LOCK();

  // Stop a currently playing note, by setting the "key on" bit for the 1st
  // channel to 0.  "block" and the high bits for "frequency" are also set to
  // 0.  This doesn't necessarily immediately mute the sound effect, depending
  // on the envelope settings (release).
  WriteAdLibReg(alFreqH + 0, 0);

  alSoundData = NULL;
  alLengthLeft = 0;
  alSoundPriority = 0;

  UNLOCK();
}


/** AdLib sound playback service routine
  *
  * This must be called at 140 Hz to keep AdLib sound playback going. The game
  * invokes this from its timer interrupt handler (found in music.c).
  *
  * Fairly similar to SDL_ALSoundService() from Wolf3D.
  */
void AdLibSoundService(void)
{
  byte sample;

  if (alSoundData)
  {
    sample = *alSoundData++;

    if (sample)
    {
      // Play a note - this sets the "frequency", "block", and "key on" values
      // for the AdLib's 1st channel.
      WriteAdLibReg(alFreqL + 0, sample);
      WriteAdLibReg(alFreqH + 0, alBlock);
    }
    else
    {
      // Stop a currently playing note, by setting the "key on" bit to 0.
      // "block" and the high bits for "frequency" are also set to 0.  This
      // doesn't necessarily immediately mute the sound effect, depending on
      // the envelope settings (release).
      WriteAdLibReg(alFreqH + 0, 0);
    }

    if (!(--alLengthLeft))
    {
      StopAdLibSound();
    }
  }
}

/** Start playback of the given AdLib sound
  *
  * This function only sets up state and then returns immediately. The actual
  * playback happens asynchronously via AdLibSoundService().
  *
  * Resembles SDL_ALPlaySound() from Wolf3D.
  */
void PlayAdLibSound(AdLibSound far* sound, long size)
{
  byte far* soundData;

  if (sound->priority >= alSoundPriority)
  {
    StopAdLibSound();

    LOCK();

    // [NOTE] It's strange that the struct size is subtracted from the size
    // parameter. There's no good reason for this, and it forces the client code
    // to add the size before calling this function (see PlayBasicSound() in
    // sound.c).  It would seem much simpler for the client code to simply pass
    // in a pointer to the start of the data from the AUDIOT package, and
    // extract the size value here instead of having the client code do it. Not
    // sure what lead to this strange arrangement.
    alLengthLeft = size - sizeof(AdLibSound);

    alSoundPriority = sound->priority;

    // The actual sound data starts after the AdLibSound struct header
    soundData = (byte far*)(sound + 1);
    alSoundData = soundData;

    // Set the "block" - this determines the octave of the generated sound.
    // The value is OR-ed with 0x20 in order to set the "key on" bit when the
    // alBlock value is written to the AdLib's "frequency high" register, in
    // order to start sound playback. See AdLibSoundService().
    alBlock = ((sound->block & 7) << 2) | 0x20;

    //
    // Set up initial state of the AdLib hardware for playing back the sound
    // effect.
    // Resembles the body of SDL_AlSetFXInst() from Wolf3D.
    //

    // AdLib sound effects only use the 1st channel, which leaves all other
    // channels free for music playback. None of the music files included with
    // the game use that channel, since that would interfere with sound effect
    // playback.

    // Set up modulator operator for channel 0 as specified in the sound data
    WriteAdLibReg(0 + alChar, sound->inst.mChar);
    WriteAdLibReg(0 + alScale, sound->inst.mScale);
    WriteAdLibReg(0 + alAttack, sound->inst.mAttack);
    WriteAdLibReg(0 + alSustain, sound->inst.mSus);
    WriteAdLibReg(0 + alWave, sound->inst.mWave);

    // Set up carrier operator for channel 0 as specified in the sound data
    WriteAdLibReg(3 + alChar, sound->inst.cChar);
    WriteAdLibReg(3 + alScale, sound->inst.cScale);
    WriteAdLibReg(3 + alAttack, sound->inst.cAttack);
    WriteAdLibReg(3 + alSustain, sound->inst.cSus);
    WriteAdLibReg(3 + alWave, sound->inst.cWave);

    // Set connection type to frequency modulation, and disable feedback
    WriteAdLibReg(alFeedCon, 0);

    UNLOCK();
  }
}


/** Test if an AdLib sound effect is currently playing
  *
  * Due to the concurrent nature of the sound playback, this function cannot
  * always reflect the actual state accurately - by the time the client code
  * makes use of the return value, the actual playback state might have already
  * changed.
  */
ibool IsAdLibPlaying(void)
{
  dword lengthLeft;

  LOCK();

  lengthLeft = alLengthLeft;

  UNLOCK();

  return lengthLeft != 0;
}
