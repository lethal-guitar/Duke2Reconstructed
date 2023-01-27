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

Digital audio playback library by Jason Blochowiak

This library consists of three components:

* Hardware detection and configuration
* 8-bit audio playback on a SoundBlaster (or compatible) card using DMA
* Playback of audio files using the Creative Voice format (.VOC)

The latter is built as a layer on top of the regular audio playback in a pretty
neat way. The audio playback layer supports 8-bit audio, either uncompressed
PCM or various SoundBlaster-specific compression schemes.  Only one sound
effect can play at a time.  It's implemented using the single-cycle DMA mode,
which is available on all models of SoundBlaster cards (including the original
1.0 version from 1989). Later models also feature a so called "auto-init DMA"
mode, which allows for fully seamless continuous audio playback.  That mode
would be more complex to use for the type of audio playback implemented in this
library, however, and would exclude the oldest models of SoundBlaster hardware.
So single-cycle mode is a decent choice here.  Unfortunately, it can lead to
some audible glitches like clicks and pops on later models like SoundBlaster
16.

Like the basicsnd library, a lot of the code here is very similar to code in
Wolfenstein 3D. I kept some of the original names intact, but also modified some
to make the code easier to understand. Any references made to functions when
talking about similarities in the code refer to the Wolfenstein 3D source:

https://github.com/id-Software/wolf3d/blob/master/WOLFSRC/ID_SD.C

*******************************************************************************/


#define SNDLIB_SRC_BUILD
#include "digisnd.h"

#include <dos.h>


// Sound Blaster register base addresses. To get actual register port addresses,
// these need to be offset depending on the configured address for the card.
// This is handled by the sbOut()/sbIn() macros.
#define sbReset         0x206 // W
#define sbFMStatus      0x208 // R
#define sbFMAddress     0x208 // W
#define sbFMData        0x209 // W
#define sbReadData      0x20a // R
#define sbWriteCmd      0x20c // W
#define sbWriteData     0x20c // W
#define sbWriteStatus   0x20c // R
#define sbDataAvailable 0x20e // R

// Utility macros
#define sbOut(n, b)     outportb((n) + sbLocation, b)
#define sbIn(n)         inportb((n) + sbLocation)
#define sbAwaitReady()  while (sbIn(sbWriteStatus) & 0x80);


// Sound Blaster commands
#define CMD_SET_TIME_CONSTANT 0x40
#define CMD_PAUSE_DAC         0x80
#define CMD_PAUSE_DMA         0xD0
#define CMD_TURN_SPEAKER_ON   0xD1


// These don't exist in the original code, but I've added them for improved
// readability.
#define DISABLE_INTERRUPTS() asm { pushf; cli }
#define ENABLE_INTERRUPTS()  asm { popf; }

#define MAX_NESTED_VOC_REPEATS 8


/** Replacement for C library's isspace */
#define SNDLIB_isspace(c) (c == ' ' || c == '\t')


typedef void interrupt (far *InterruptHandler)(void);


// Codec types supported by the hardware. The numbering is identical to the way
// the codecs are identified in VOC files.
typedef enum
{
  CODEC_8BIT_PCM,
  CODEC_4BIT_ADPCM,
  CODEC_3BIT_ADPCM,
  CODEC_2BIT_ADPCM
} CodecType;


// Types of sections in a VOC file
typedef enum
{
  VOC_SECTION_TERMINATOR = 0,
  VOC_SECTION_SOUND_TYPED = 1,
  VOC_SECTION_SOUND_UNTYPED = 2,
  VOC_SECTION_SILENCE = 3,
  VOC_SECTION_REPEAT_START = 6,
  VOC_SECTION_REPEAT_END = 7
} VocSectionType;


//
// Public globals
//
bool AdLibPresent;
bool SoundBlasterPresent;


//
// Private globals
//
static char* sndParseEnvError;
static bool sndInitialized;

static SoundFinishedCallback sbSoundFinishedCallback;
static NewVocSectionCallback sbNewVocSectionCallback;

// Not clear what these are for. They might also be variables in the basicsnd
// library, it's not clear from the machine code to which translation unit they
// belong, since they are unused.
static word junk1 = 0x118;
static word junk2 = 0x120;


//
// Sound Blaster address, interrupt and DMA channel configuration variables
//
static word sbAlAddress   = 0x388;

static byte sbOldIntMask  = -1; // all 1s
static byte sbOldIntMask2 = -1; // all 1s


// DMA channel to use. Valid values are 0, 1, and 3.  DMA channel 2 can't be
// used by the Sound Blaster, since it's reserved for the floppy drive.
static byte sbDmaChannel = 1;

// Port addresses for programming the DMA controller, these are different based
// on the channel. The lookup tables below contain their values for the
// different possible channel values.
static byte sbDmaPageRegister = 0x83;
static byte sbDmaAddressPort  = 2;
static byte sbDmaLengthPort   = 3;

static const byte DMA_PAGE_REGISTERS[] = { 0x87, 0x83,    0, 0x82 };
static const byte DMA_ADDRESS_PORTS[]  = {    0,    2,    0,    6 };
static const byte DMA_LENGTH_PORTS[]   = {    1,    3,    0,    7 };


// Command bytes to send to the Sound Blaster's DSP in order to kick off
// playback of a sample. Ordered by codec type (see CodecType enum).
static const byte PLAY_CMDS_WITH_REF[] = { 0x14, 0x75, 0x77, 0x17 };
static const byte PLAY_CMDS_NO_REF[]   = { 0x14, 0x74, 0x76, 0x16 };


// Address offset of the Sound Blaster DSP's I/O port. Has to be combined with a
// base port address to create a valid port address, see sbOut/sbIn above.
static int sbLocation = -1;


// Interrupt (IRQ) number
static int sbInterrupt = 7;

// Location of the Sound Blaster's DMA completion interrupt vector, derived from
// the IRQ number. The lookup table below contains all vector locations for
// the different possible IRQ numbers, with -1 marking invalid values.
// Valid IRQ numbers are 2, 3, 5, 7, and 10.
static int sbIntVec = 0xf; // Default IRQ is 7

static const int INTERRUPT_VECTORS[] = {
  -1, -1, 0xa, 0xb, -1, 0xd, -1, 0xf, -1, -1, 0x72 };


//
// Watermark strings
//

// These strings aren't used by the code, but they end up in the executable.
// This was likely their only purpose - perhaps to be able to identify if
// people were using Jason's library in their programs without permission, or
// maybe also as a means of advertising to other developers who would peek
// into the strings of competitor's game executables.
static char UNKNOWN[] = "!AGDR13";
static char COPYRIGHT[] =
  "Digital playback routines, Copyright 1992,1993 by Jason Blochowiak";


//
// State needed for playback
//
static byte sbIntMask;
static byte sbIntMask2;

static InterruptHandler sbSavedIntHandler;

static int sbCodecType;
static byte sbTimeValue;
static volatile bool sbSamplePlaying;
static volatile byte huge *sbNextChunkPtr;
static volatile dword sbNextChunkLen;

static int sbVocRepeatIndex;
static byte huge* sbVocToRepeat[MAX_NESTED_VOC_REPEATS];
static word sbVocRepeatCounts[MAX_NESTED_VOC_REPEATS];
static bool sbVocPlaying;
static byte huge* sbVocData;


/*******************************************************************************

Part 1: SoundBlaster digital audio playback

*******************************************************************************/


/** Replacement for Borland C library's getvect */
static InterruptHandler SNDLIB_getvect(int num)
{
  // Retrieve an interrupt vector using a DOS interrupt. This is kinda like a
  // system call.
  asm mov  ah, 0x35
  asm mov  al, byte ptr [num]
  asm int  0x21
  asm xchg bx, ax
  asm mov  dx, es
}


/** Replacement for Borland C library's setvect */
static void SNDLIB_setvect(int num, InterruptHandler vector)
{
  // Set an interrupt vector using a DOS interrupt. This is kinda like a
  // system call.
  asm mov  ah, 0x25
  asm mov  al, byte ptr [num]
  asm push ds
  asm lds  dx, [vector]
  asm int  0x21
  asm pop  ds
}


/** Enable Sound Blaster DMA completion interrupts */
static void EnableSbInterrupts(void)
{
  // Here, we're modifying the interrupt mask register on the PC's PIC
  // (programmable interrupt controller). The register is a bitmask which has 1
  // bit for each IRQ (interrupt request) line connected to the PIC. If a bit
  // is set, then the corresponding IRQ is ignored.  sbIntMask contains the
  // single bit that would disable the interrupt used by the Sound Blaster.
  // Since we want to enable it here, we invert sbIntMask in order to unset the
  // corresponding bit.
  sbOldIntMask = inportb(0x21);
  outportb(0x21, sbOldIntMask & ~sbIntMask);

  // If the configured interrupt is higher than 7, we need to instead use the
  // secondary PIC. The IBM PC (starting with the PC AT) featured two PICs
  // connected together, each supporting 8 IRQ lines, for a total of 15 lines
  // (one line is already needed to chain the two PICs together).
  //
  // [BUG] I'm not quite sure why the primary PIC is still modified in case the
  // interrupt is higher than 7. I'm positive that enabling the interrupt in
  // the secondary PIC should be sufficient here.  Using an if/else for the two
  // cases would seem more appropriate, and is also how masking the interrupts
  // is done in other example code I've looked at.
  //
  // Is this a problem? sbIntMask (for the primary PIC) ends up being set to 4
  // (100b) in case the sound card is using IRQ 10 (the only valid IRQ > 7 for
  // the SoundBlaster), which happens to be the bit for masking off IRQ 2. That
  // IRQ happens to be used to connect the secondary PIC to the primary one.
  // Since that IRQ should already be enabled, enabling it again shouldn't
  // have any adverse side-effects, but disabling it would effectively disable
  // *all* IRQs in the range 8 to 15. That's more likely to cause issues, but
  // it's unlikely to occur in practice due to the way DisableSbInterrupts() is
  // implemented. See that function for more details.
  if (sbInterrupt >= 8)
  {
    sbOldIntMask2 = inportb(0xA1);
    outportb(0xA1, sbOldIntMask2 & ~sbIntMask2);
  }
}


/** Disable Sound Blaster DMA completion interrupts */
static void DisableSbInterrupts(void)
{
  byte interruptMask;

  // This works similary to EnableSbInterrupts, but here we want to set the
  // bit, not unset it.
  interruptMask = inportb(0x21);

  // This condition will be true if either EnableSbInterrupts() has never been
  // called so far (so sbOldIntMask is still at its initial value of all 1s),
  // or if the desired interrupt was disabled at the time EnableSbInterrupts()
  // was last called.
  if (sbOldIntMask & sbIntMask)
  {
    // Set the bit, disabling the interrupt
    interruptMask |= sbIntMask;
  }
  else
  {
    // Otherwise, the interrupt was already enabled at the time
    // EnableSbInterrupts() was called. In that case, we unset the bit.  This
    // effectively keeps the interrupt enabled, or even enables it in case
    // DisableSbInterrupts() is called without calling EnableSbInterrupts()
    // first.
    //
    // I'm not quite sure what the reasoning was behind this logic, it seems
    // like the idea was to preserve the interrupt's enabled state in case it
    // was already enabled before EnableSbInterrupts() was called, but I'm not
    // quite sure why. If the intention was to restore the system's state from
    // before setting up the SoundBlaster interrupt handler, then I would
    // expect sbOldIntMask to be captured at the time when the interrupt
    // handler is installed, and kept unchanged until restoring the original
    // interrupt handler. But that variable is set in EnableSbInterrupts every
    // time that function is called.
    //
    // One side-effect of this is that it mitigates a bug in InitSoundBlaster()
    // (although one could argue that the bug is really in this function along
    // with EnableSbInterrupts(), not in InitSoundBlaster()).
    // When the SoundBlaster's IRQ is set to 10, its interrupt will be handled
    // by the secondary PIC. InitSoundBlaster() incorrectly sets sbIntMask to 4
    // in case the IRQ is 10. Applying that mask to the primary PIC would cause
    // IRQ 2 to be disabled, which happens to be the IRQ used by the secondary
    // PIC to notify the primary one. In other words, disabling IRQ 2 would
    // disable *all* IRQs in range 8 to 15!  The only reason that doesn't
    // happen is because of the aforementioned logic here, which will keep IRQ
    // 2 enabled if it was already enabled by the time EnableSbInterrupts()
    // gets called. Due to the purpose of said IRQ, it's normally going to be
    // enabled already, so that prevents the code here from (incorrectly)
    // disabling it.
    interruptMask &= ~sbIntMask;
  }

  outportb(0x21, interruptMask);


  // Same thing here as in EnableSbInterrupts - use the secondary PIC if the
  // interrupt number is higher than 7. The logic here is otherwise identical
  // to the code above. Also see the [BUG] note in EnableSbInterrupts() on this.
  if (sbInterrupt >= 8)
  {
    interruptMask = inportb(0xA1);

    if (sbOldIntMask2 & sbIntMask2)
    {
      interruptMask |= sbIntMask2;
    }
    else
    {
      interruptMask &= ~sbIntMask2;
    }

    outportb(0xA1, interruptMask);
  }
}


/** Send a command with a word-sized parameter to the Sound Blaster */
static void OutputCommand(byte command, word value)
{
  // Write command ID
  sbAwaitReady();
  sbOut(sbWriteCmd, command);

  // Write parameter, two bytes go to the same I/O port
  sbAwaitReady();
  sbOut(sbWriteData, (byte)value);
  sbAwaitReady();
  sbOut(sbWriteData, (byte)(value >> 8));
}


/** Stop a currently playing sample
 *
 * This function is also called when a block of silence is finished.
 *
 * Fairly similar to SDL_SBStopSample() from Wolf3D. Differences are the
 * additional wait loop before sbAwaitReady, the addition of a 'sound finished'
 * callback, and the code to disable DMA completion interrupts is different.
 * In Wolf3D, the modification of the PIC's mask register is performed inline
 * instead of calling a dedicated function, and there's no support for IRQ 10.
 */
static void StopSbSound_Private(void)
{
  int i;

  DISABLE_INTERRUPTS();

  if (sbSamplePlaying)
  {
    sbSamplePlaying = false;

    // This is an extra busy loop to wait for the hardware to be ready, I'm not
    // sure why this is needed since there's already a sbAwaitReady() below.
    for (i = 0; i < 50; i++) { if (sbIn(sbWriteStatus) & 0x80) { break; } }

    sbAwaitReady();

    // Send a "Pause DMA" command, this will stop any DMA operation that's
    // currently in flight.
    sbOut(sbWriteCmd, CMD_PAUSE_DMA);

    DisableSbInterrupts();

    // Notify client code that we're done playing a sound.
    if (sbSoundFinishedCallback)
    {
      sbSoundFinishedCallback();
    }
  }

  ENABLE_INTERRUPTS();
}


/** Trigger playback of (part of) a sample via DMA
 *
 * The sample must be < 64kB in size.
 * sbCodecType must be set before invoking this function.
 *
 * Returns the number of bytes that could be submitted in one go, which might be
 * lower than the total size of the sample. In that case, the remainder needs to
 * be submitted after the DMA transfer started by this function completes.
 *
 * This is almost identical to SDL_SBPlaySeg() from Wolf3D, the only difference
 * is that sending the playback command to the SB's DSP is different - Wolf3D
 * only supports uncompressed 8-bit samples, and thus hardcodes the command ID,
 * and it doesn't use a function like OutputCommand but inlines the needed
 * code.
 */
static dword SubmitSampleChunk(
  volatile byte huge *data, dword length, bool hasRefByte)
{
  unsigned dataPage;
  dword dataOffset;
  dword lengthToPlay;

  // The basic idea here is that we program the PC's DMA controller with the
  // address and length of the data we want to play, and then send a command to
  // the Sound Blaster to kick off DMA-based playback. The DMA controller will
  // then transfer the data from main memory to the sound card without any
  // involvement by the CPU. The sound card will start generating audio as soon
  // as it receives data. Once the transfer is complete, we receive an
  // interrupt - that's handled by SBService. Within the interrupt, we can then
  // submit the next piece of data if there's still some samples left to play,
  // or stop if we're done. This process is also known as "single-cycle DMA" in
  // the Sound Blaster documentation.
  //
  // The DMA controller used in the IBM PC only supports transfers of up to
  // 64kB in size. And what's more, the controller chip itself only supports
  // 16-bit addresses, requiring part of the address to be loaded into a
  // separate "Page Register" (a dedicated hardware component specific to the
  // IBM PC). The value in the page register will then be combined with the
  // address coming from the DMA controller to generate the actual address.
  // Finally, addresses within the block to be transferred must not cross a
  // 64kB boundary, since the page register is not automatically incremented as
  // the data is transferred.
  //
  // Because of this, we first need to decompose our pointer into a 16-bit
  // address and a 4-bit offset for the page register.
  lengthToPlay = length;

  // Derive the "data page" from our pointer, this is what we will program into
  // the DMA page register below. The data pointer is a huge pointer, so it
  // consists of a segment and an offset, which can be combined into a 20-bit
  // address. Here, we take the upper 4 bits of the segment value, which is
  // what we need for our page number.
  dataPage = FP_SEG(data) >> 12;

  // Now compute the 16-bit DMA address by combining segment and offset in a
  // similar way as the hardware does when computing a physical address, but
  // omit the upper 4 bits from the segment (which are already in dataPage).
  // The resulting value will be between 0x00000 and 0x1FFEF (inclusive).
  dataOffset = ((FP_SEG(data) & 0xFFF) << 4) + FP_OFF(data);

  // If the resulting offset is larger than 16 bits, we need to adjust it to
  // fit, by bumping up the page number and subtracting 0x10000 from the
  // offset.  The resulting physical address is still the same, we just move
  // the high bit in the address from the offset value into the page value,
  // basically.
  //
  // [NOTE] It feels like all of this could've been accomplished in an easier
  // way by computing the full physical address first, and then setting
  // dataOffset to `address & 0xFFFF` and dataPage to `address >> 16`.
  if (dataOffset >= 0x10000)
  {
    dataPage++;
    dataOffset -= 0x10000;
  }

  // Finally, we also need to make sure that we don't cross a 64kB boundary
  // during the data transfer. If we do, we clamp the length. The remainder of
  // the data will then be sent via an additional DMA transfer (see SBService).
  //
  // [NOTE] In theory, a very large sound effect might cross multiple 64kB
  // boundaries, which is not handled by this code. But this never happens in
  // practice, since all sound effects are smaller than 64kB. The biggest one
  // is 21kB, but most of them are just 3kB or less.
  if (dataOffset + lengthToPlay > 0x10000)
  {
    lengthToPlay = 0x10000 - dataOffset;
  }

  // The DMA controller expects the length to be one less than what we actually
  // want to transfer, so adjust the length accordingly.
  lengthToPlay--;

  //
  // Now we have everything ready to actually program the DMA controller.
  //

  DISABLE_INTERRUPTS();

  // First, mask off (disable) DMA on the channel we're using. This is to
  // prevent the DMA controller from responding to DMA request signals while it
  // still has incomplete data.
  outportb(0x0a, sbDmaChannel | 4);

  // Clear the DMA controller's MSB/LSB Flip-Flop. 16-bit values are given to
  // the DMA controller via two consecutive writes to the same I/O port.  We
  // don't know if the port is currently expecting a high or a low byte, so we
  // first need to put it into a known state - this accomplishes that.
  outportb(0x0c, 0);

  // Set the DMA controller's mode. 0x49 is a bit mask setting various bits in
  // the DMA controller's mode register, as follows:
  //
  // Bits      | Interpretation
  // ----------|---------------
  // 01xxxxxx  | Select Single Mode - transfer one byte at a time
  // xx0xxxxx  | Increment address after each transfer
  // xxx0xxxx  | Disable auto-initialize - don't restart the DMA transfer
  //           | after its completion
  // xxxx10xx  | Select Read Transfer mode - data is read from memory and made
  //           | available to a hardware device
  // xxxxxx01  | Apply these settings for channel 1
  //
  // [BUG] This should actually be `0x48 | sbDmaChannel` instead of 0x49, which
  // hardcodes channel 1 regardless of the value of sbDmaChannel. This
  // oversight prevents the game from using any DMA channel aside from 1, even
  // though all the rest of the code is correctly set up to handle other DMA
  // channels.  The same bug also exists in the Wolfenstein 3D codebase. The
  // technical trouble-shooting documentation that came with the game offers
  // the following:
  //
  // """
  // Please note that Duke Nukem II must have a DMA of 1 in order to function
  // properly. If you do have it set for 1, and you determine you have a
  // conflict, you will need to change the DMA channel of some other piece of
  // hardware in your system that is also using DMA 1. Please consult your
  // appropriate manual for information on how to do this.
  // """
  //
  // However, a simple 1-line change to the code would have made the game
  // perfectly capable of handling other DMA channels as well.  I find it
  // somewhat baffling that this was never fixed - I'd love to know how this
  // happened, and why the author of this code, despite clearly being very
  // competent, wasn't able to spot this problem and fix it. Was this aspect of
  // Sound Blaster programming poorly documented? Did the author blindly copy
  // some example code without understanding what it does?
  //
  // Anyway - what happens when a different channel is configured? Depending on
  // what state the DMA controller is in when the game is launched, sound might
  // still work just fine, or the entire system might lock up completely and
  // require a hard reboot. This is on real hardware - in my experiments using
  // DosBox, sound seemed to work fine regardless of which DMA channel was
  // used.
  outportb(0x0b, 0x49);

  // Now give the memory address and length to the DMA controller
  outportb(sbDmaAddressPort, (byte)dataOffset);         // LSB of address
  outportb(sbDmaAddressPort, (byte)(dataOffset >> 8));  // MSB of address
  outportb(sbDmaPageRegister, (byte)dataPage);          // page
  outportb(sbDmaLengthPort, (byte)lengthToPlay);        // LSB of length
  outportb(sbDmaLengthPort, (byte)(lengthToPlay >> 8)); // MSB of length

  // Re-enable DMA on the channel we're using, now that we've completed the
  // configuration.
  outportb(0x0a, sbDmaChannel);

  // Kick off playback by sending an appropriate command to the Sound Blaster.
  // The type of command depends on the codec used, and also on if we have a
  // reference byte or not.
  // From now on, the DMA controller and Sound Blaster will communicate, and
  // the block of data is sent to the Sound Blaster byte for byte until
  // complete without requiring any involvement by the CPU.
  OutputCommand(
    hasRefByte ?
      PLAY_CMDS_WITH_REF[sbCodecType] : PLAY_CMDS_NO_REF[sbCodecType],
    lengthToPlay);

  ENABLE_INTERRUPTS();

  // Since we decremented the length above for sending it to the DMA
  // controller, we need to add 1 again here before returning how many bytes we
  // were able to submit via DMA.
  return lengthToPlay + 1;
}


/** Respond to Sound Blaster DMA transfer completion interrupts
 *
 * Almost identical to SDL_SBService() from Wolf3D, the only difference is
 * added support for IRQ 10 and a different function is called when playback
 * has finished.
 */
static void interrupt SBService(void)
{
  dword bytesSubmitted;

  // Acknowledge interrupt to Sound Blaster
  sbIn(sbDataAvailable);

  if ((byte far*)sbNextChunkPtr) // Is there more data left to send?
  {
    // Submit next portion of the sample via DMA
    bytesSubmitted = SubmitSampleChunk(sbNextChunkPtr, sbNextChunkLen, false);

    // If we've submitted all of the data, unset the next chunk pointer so
    // that we can finish playback on the next interrupt.
    if (sbNextChunkLen <= bytesSubmitted)
    {
      sbNextChunkPtr = NULL;
    }
    else
    {
      // Otherwise, adjust the pointer and length so that the next chunk of
      // sound data is submitted after the current DMA transfer finishes.
      sbNextChunkPtr += bytesSubmitted;
      sbNextChunkLen -= bytesSubmitted;
    }
  }
  else
  {
    // We've completed playback of the entire sound
    StopSbSound_Private();
  }

  // Acknowledge interrupt to the PC's interrupt controller
  outportb(0x20, 0x20);

  // When the sound card is using IRQ 10, the interrupt signal from the
  // hardware arrives at the secondary PIC, which in turn notifies the primary
  // PIC (which then notifies the CPU). We therefore need to send an
  // acknowledgement command to both PICs in case the sound card's IRQ is
  // greater than 7.
  if (sbInterrupt >= 8)
  {
    outportb(0xA0, 0x20);
  }
}


/** Set the DSP's Time Constant - this determines the DAC's sampling rate */
static void SetTimeConstant(byte timeValue)
{
  DISABLE_INTERRUPTS();

  sbTimeValue = timeValue;

  sbAwaitReady();
  sbOut(sbWriteCmd, CMD_SET_TIME_CONSTANT);
  sbAwaitReady();
  sbOut(sbWriteData, timeValue);

  ENABLE_INTERRUPTS();
}


/** Convert sample rate (samples per second) to a DSP Time Constant value */
static byte ComputeTimeValue(long sampleRate)
{
  byte result = 256 - (1000000L / sampleRate);
  return result;
}


/** Start playback of a digital sound sample using the given parameters
 *
 * Quite similar to SDL_SBPlaySample() from Wolf3D. Support for variable
 * sampling rates and different codecs is new in this version.
 */
static void PlaySample_Private(
  byte huge *data,
  byte timeValue,
  word codecType,
  bool hasRefByte,
  dword length)
{
  dword bytesSubmitted;

  // Stop any already playing sound
  StopSbSound_Private();

  // Set the sample rate
  SetTimeConstant(timeValue);

  DISABLE_INTERRUPTS();

  // Store codec type for later use, in case the data has to be submitted via
  // multiple DMA transfers (see SBService).
  sbCodecType = codecType;

  // Kick off the DMA transfer
  bytesSubmitted = SubmitSampleChunk(data, length, hasRefByte);
  if (length <= bytesSubmitted)
  {
    // If we were able to submit all the data in one transfer, playback will be
    // finished as soon as the DMA completion interrupt comes in, so set the
    // next chunk pointer accordingly. See SBService().
    sbNextChunkPtr = NULL;
  }
  else
  {
    // Otherwise, set up the pointer and length so that the next chunk of sound
    // data is submitted after the current DMA transfer finishes.
    // See SBService().
    sbNextChunkPtr = data + bytesSubmitted;
    sbNextChunkLen = length - bytesSubmitted;
  }

  sbSamplePlaying = true;

  EnableSbInterrupts();

  ENABLE_INTERRUPTS();
}


/** Start playback of a digital sound sample in 8-bit PCM format
 *
 * Public function. Not used in the game.
 */
void SB_PlaySample(byte huge* data, long sampleRate, dword length)
{
  PlaySample_Private(
    data, ComputeTimeValue(sampleRate), CODEC_8BIT_PCM, true, length);
}


/** "Play" silence - pause playback for specified duration */
static void PlaySilence_Private(byte timeValue, dword length)
{
  DISABLE_INTERRUPTS();

  SetTimeConstant(timeValue);

  // This command tells the DSP to be silent for the specified duration, and
  // then generate an interrupt. This allows us to handle silence the same way
  // as sample playback is handled. Once the specified time period has elapsed,
  // SBService will be called due to the interrupt, it will see that
  // sbNextChunkPtr is NULL, and thus call StopSbSound_Private.
  OutputCommand(CMD_PAUSE_DAC, length);

  sbSamplePlaying = true;

  EnableSbInterrupts();

  ENABLE_INTERRUPTS();
}


/** Output silence for given duration and sample rate
 *
 * Public function. Not used in the game.
 */
void SB_PlaySilence(long sampleRate, dword length)
{
  PlaySilence_Private(ComputeTimeValue(sampleRate), length);
}


/** Return true if a sample (or silence) is currently playing
 *
 * Also returns true if sample playback was initiated by a call to SB_PlayVoc.
 *
 * Public function.
 */
bool SB_IsSamplePlaying(void)
{
  return sbSamplePlaying;
}


/** Set callback function to be invoked after sound playback finishes
 *
 * Public function. Not used in the game, but used internally in this library.
 */
void SB_SetSoundFinishedCallback(SoundFinishedCallback callback)
{
  DISABLE_INTERRUPTS();

  sbSoundFinishedCallback = callback;

  ENABLE_INTERRUPTS();
}


/** Stop any currently playing sound
 *
 * This works for sounds started with any of the playback functions - i.e.
 * SB_PlaySample, SB_PlaySilence, and SB_PlayVoc.
 *
 * Public function.
 */
void SB_StopSound(void)
{
  SB_SetSoundFinishedCallback(NULL);
  StopSbSound_Private();

  DISABLE_INTERRUPTS();

  sbVocData = NULL;
  sbVocPlaying = false;
  sbVocRepeatIndex = 0;

  ENABLE_INTERRUPTS();
}


/*******************************************************************************

Part 2: Creative Voice (VOC) file support

This is a layer on top of the sample playback functionality. Thanks to the
callback system, and the fact that VOC files are tailor-made for the
SoundBlaster hardware, it could be implemented in a fairly simple & elegant
manner here.

*******************************************************************************/


/** Parse next VOC file section, and submit audio
 *
 * This function is invoked via sbSoundFinishedCallback whenever a piece of
 * audio finishes playing that was part of the current VOC file. It parses the
 * next section in the file's data and submits more audio as needed.
 *
 * See SB_PlayVoc for more details on the VOC file format.
 */
static void PlayNextVocSection(void)
{
  bool keepGoing = false;
  long sectionLength;
  int sectionType;

  do
  {
    keepGoing = false;

    // The first byte in each section indicates what type of section it is
    sectionType = *sbVocData++;

    if (sectionType == VOC_SECTION_TERMINATOR)
    {
      // We've reached the end of the file, stop here
      SB_StopSound();

      if (sbNewVocSectionCallback)
      {
        sbNewVocSectionCallback(sectionType, 0, NULL);
        return;
      }

      break;
    }

    // In VOC files, the section length is encoded as a 24-bit value (3 bytes).
    // Here we take advantage of little-endian encoding, by reading a full
    // 32-bit value from the data, and then throwing away the most significant
    // 8 bits (which are not actually part of the length value).
    sectionLength = 0xFFFFFFl & *((dword far*)sbVocData);
    sbVocData += 3;

    if (sbNewVocSectionCallback)
    {
      sbNewVocSectionCallback(sectionType, sectionLength, sbVocData);
    }

    // Now handle the section's content
    switch ((VocSectionType)sectionType)
    {
      case VOC_SECTION_SOUND_TYPED:
        // A "typed sound" section has two bytes of header information, first
        // the time value and then the codec type. So we extract those from the
        // data and pass them to our sample playback function. The sound data
        // itself starts after those two header bytes, hence the +2 on the
        // pointer and the -2 on the length.
        // When using one of the ADPCM codecs, the first byte in a typed
        // section acts as the reference byte, so we say that we have one.
        PlaySample_Private(
          sbVocData + 2,      // data
          *sbVocData,         // timeValue
          *(sbVocData + 1),   // codecType
          true,               // hasRefByte
          sectionLength - 2); // length
        break;

      case VOC_SECTION_SOUND_UNTYPED:
        // An "untyped sound" section has no further header information, so we
        // just reuse the most recent codec and time value. The actual data
        // starts right after the section type & size, i.e. sbVocData already
        // points at the right place.
        // Since an untyped sound section is meant as a continuation of a
        // preceding typed sound section, we don't have a reference byte for
        // the ADPCM codecs - this was part of the last typed section.
        PlaySample_Private(
          sbVocData,
          sbTimeValue,
          sbCodecType,
          false,          // hasRefByte
          sectionLength);
        break;

      case VOC_SECTION_SILENCE:
        // A "silence" section has a 3 byte header consisting of a 16-bit
        // duration, and an 8-bit time value. Extract these values from the
        // header and pass them to our playback function.
        PlaySilence_Private(*(sbVocData + 2), *((word far*)sbVocData));
        break;

      case VOC_SECTION_REPEAT_START:
        // A "repeat start" section indicates that all subsequent sections,
        // until encountering a "repeat end" section, should be played back
        // multiple times.
        // How many times is indicated by a 16-bit value at the start of the
        // section.
        // We allow a maximum of MAX_NESTED_VOC_REPEATS nested repeats.
        if (sbVocRepeatIndex < MAX_NESTED_VOC_REPEATS)
        {
          // The current data plus section length is the start of the next
          // section, which is where we need to jump back to in order to
          // repeat this part.
          sbVocToRepeat[sbVocRepeatIndex] = sbVocData + sectionLength;

          // Also extract and store the repetition count.
          sbVocRepeatCounts[sbVocRepeatIndex] = *((word huge*)sbVocData);
        }

        // Even if we've reached the maximum number of nested repeats, we
        // still increment the index to keep track of the current repeat
        // stacking level.
        sbVocRepeatIndex++;

        // Also parse the next section - that will usually be the start of the
        // sound data that's meant to be repeated.
        keepGoing = true;
        break;

      case VOC_SECTION_REPEAT_END:
        if (sbVocRepeatIndex == 0)
        {
          // If we enconter a repeat end without a preceding repeat start,
          // abort playback of the whole file, assuming that it's a corrupt
          // file.
          break;
        }

        // Always decrement the index, even if we've reached the maximum
        // number of nested repeats, to keep track of the current repeat
        // stacking level.
        sbVocRepeatIndex--;

        if (sbVocRepeatIndex < MAX_NESTED_VOC_REPEATS)
        {
          // If we're below the limit of max. nested repeats, look up and
          // decrement the repeat counter.
          if (sbVocRepeatCounts[sbVocRepeatIndex]--)
          {
            // Jump back to the start of the part that's to be repeated
            sbVocData = sbVocToRepeat[sbVocRepeatIndex];

            // Normally, we would skip to the start of the next section at the
            // end of the loop, but since we've just set sbVocData to point to
            // where we need to jump to for repeating, we don't want that to
            // happen this time.
            // [NOTE] According to the documentation, the size field for a
            // "repeat end" should always be 0, so this might not be necessary.
            sectionLength = 0;

            // We need to increment this again, since we would otherwise get
            // out of balance the next time we encounter the current "repeat
            // end".
            sbVocRepeatIndex++;
          }
        }

        // Parse the next section - either the section after the "repeat end"
        // if we've already performed all repetitions, or the start of the
        // part that we need to repeat.
        keepGoing = true;
        break;

      default:
        // Unrecognized section types are skipped over
        keepGoing = true;
        break;
    }

    // Skip forward to the start of the next section
    //
    // [UNSAFE] There's no checking that we still have data left to process, so
    // a truncated VOC file or one without an "end" marker (apparently, it is
    // optional) would cause a buffer overrun and we might start interpreting
    // random unrelated memory as sound data.
    sbVocData += sectionLength;
  }
  while (keepGoing);
}


/** Play a VOC file that's already in memory
 *
 * Public function.
 */
void SB_PlayVoc(byte huge* data, bool includesHeader)
{
  // A Creative Voice (VOC) file is a binary file format which consists of a
  // small header followed by a number of variable-sized sections. Each section
  // begins with a type number and size, followed by data specific to that type
  // of section. The different types of sections are handled in
  // PlayNextVocSection().
  // Each section typically contains audio data using different supported
  // codecs and sample rates. All the codecs are decoded in hardware by the
  // SoundBlaster's DSP, so the data just needs to be sent over to the card
  // unchanged.  To further reduce file size, sections can also describe
  // periods of silence, and sections can be repeated multiple times.
  //
  // See https://moddingwiki.shikadi.net/wiki/VOC_Format

  if (includesHeader)
  {
    // The header starts with a 20-byte signature, followed by a 16-bit offset
    // to the data after the header. Here, we extract that offset value and
    // use it to adjust our pointer in order to skip past the header.
    data += *(word huge*)(data + 20);
  }

  // Interrupt any already playing sound
  SB_StopSound();

  // We reuse the callback system to dispatch the next VOC file section after
  // the first one has finished playing, if there's more than one section in
  // the file.
  SB_SetSoundFinishedCallback(PlayNextVocSection);

  DISABLE_INTERRUPTS();

  sbVocData = data;
  sbVocPlaying = true;

  // Kick off playback by parsing the first section in the file. Subsequent
  // sections are handled by the completion callback we've set above.
  PlayNextVocSection();

  ENABLE_INTERRUPTS();
}


/** Return true if a VOC file is currently playing
 *
 * Public function. Not used by the game, it only uses SB_IsSamplePlaying.
 */
bool SB_IsVocPlaying(void)
{
  return sbVocPlaying;
}


/** Set a new callback to be invoked when reaching a new section in a VOC file
 *
 * Public function. Not used by the game.
 */
void SB_SetNewVocSectionCallback(NewVocSectionCallback callback)
{
  DISABLE_INTERRUPTS();

  sbNewVocSectionCallback = callback;

  ENABLE_INTERRUPTS();
}


/*******************************************************************************

Part 3: Hardware detection, initialization and shutdown

*******************************************************************************/


bool DetectAndInitAdLib(void);


/** Run the SoundBlaster initialization routine using the specified port index
 *
 * If we can successfully run the SoundBlaster initialization routine at the
 * specified port index, than we conclude that a SoundBlaster or compatible
 * device is present at that location.
 *
 * Returns true if successful, false otherwise.
 *
 * Very similar to SDL_CheckSB() from Wolf3D. The only major difference is that
 * the latter doesn't try to detect the SoundBlaster's OPL2 first.
 */
static bool TryInitSB(int portIndex)
{
  int i;
  int originalAddress = sbAlAddress;

  sbLocation  = portIndex << 4;
  sbAlAddress = sbLocation + sbFMAddress;

  if (!DetectAndInitAdLib())
  {
    // If there isn't even an OPL2 at the specified location, than we're
    // unlikely to find a full-blown SoundBlaster there.
    sbAlAddress = originalAddress;
    sbLocation  = -1;

    return false;
  }

  // Now we run the SoundBlaster initialization procedure as described in
  // Creative's documentation. This consists of writing a 1 to a dedicated
  // reset port, waiting some time, then resetting the port back to 0, waiting
  // some more time, and finally trying to read a data byte from the sound
  // card. If that data byte has the expected value, we've successfully
  // initialized the SoundBlaster.
  sbOut(sbReset, 1);

  // Wait at least 4 usec. For timing, we read the OPL2's address register,
  // similarly to what we do in WriteSBAdLibReg().
  asm mov  dx,[sbAlAddress]
  asm in   al, dx
  asm in   al, dx
  asm in   al, dx
  asm in   al, dx
  asm in   al, dx
  asm in   al, dx
  asm in   al, dx
  asm in   al, dx
  asm in   al, dx

  sbOut(sbReset, 0);

  // Wait at least 100 usec
  asm mov  dx, [sbAlAddress]
  asm mov  cx, 100
waitLoop:
  asm in   al, dx
  asm loop waitLoop

  // Now attempt to read the data byte, and retry this 100 times in case the
  // card needs a bit more time.
  for (i = 0; i < 100; i++)
  {
    // Check if the sound card indicates that it has some data for us to read
    if (sbIn(sbDataAvailable) & 0x80)
    {
      // If yes, then fetch the data and check that the value is as expected.
      if (sbIn(sbReadData) == 0xAA)
      {
        // Success!
        return true;
      }
      else
      {
        // If we get something else, the device doesn't seem to be a
        // SoundBlaster, so we don't need to keep trying.
        break;
      }
    }
  }

  // We failed, either because the card didn't respond within 100 tries, or it
  // responded with an unexpected value. Restore default address values and
  // report failure.
  sbLocation = -1;
  sbAlAddress = originalAddress;

  return false;
}


/** Try to detect a SoundBlaster or compatible device at specified location
 *
 * The exact behavior depends on the given portIndex. If it is -1, then various
 * well known locations are tested in sequence. If it is 0, the default of
 * address 0x220 is tested.  Otherwise, the given index is tested.
 *
 * Returns true if successful, false otherwise.
 *
 * Practically identical to SDL_DetectSoundBlaster() from Wolf3D.
 */
static bool DetectSoundBlaster(int portIndex)
{
  int i;

  // The default address is 0x220, i.e. a portIndex of 2. If a 0 was given as
  // portIndex, we assume that the default location should be checked.
  if (portIndex == 0)
  {
    portIndex = 2;
  }

  // No port specified, try all possible locations
  if (portIndex == -1)
  {
    // First, try default address of 0x220
    if (TryInitSB(2))
    {
      return true;
    }

    // Try 0x240
    if (TryInitSB(4))
    {
      return true;
    }

    // Now try a variety of possible addresses: 0x210, 0x230, 0x250, 0x260.
    for (i = 1; i <= 6; i++)
    {
      // Skip 0x220 and 0x240, since we've already tried those
      if ((i == 2) || (i == 4))
      {
        continue;
      }

      if (TryInitSB(i))
      {
        return true;
      }
    }

    // None of the tested addresses worked, report failure
    return false;
  }
  else
  {
    return TryInitSB(portIndex);
  }
}


/** Set DMA-channel related variables
 *
 * Almost identical to SDL_SBSetDMA() from Wolf3D, aside from some error handling
 * and disabling of interrupts.
 */
static void SetDmaChannel(byte channel)
{
  DISABLE_INTERRUPTS();

  sbDmaChannel      = channel;
  sbDmaPageRegister = DMA_PAGE_REGISTERS[channel];
  sbDmaAddressPort  = DMA_ADDRESS_PORTS[channel];
  sbDmaLengthPort   = DMA_LENGTH_PORTS[channel];

  ENABLE_INTERRUPTS();
}


/** Initialize the Sound Blaster. Settings must already be configured. */
static void InitSoundBlaster(void)
{
  byte unused1, unused2;

  // Set the interrupt vector corresponding to the chosen IRQ
  sbIntVec = INTERRUPT_VECTORS[sbInterrupt];

  // Set the mask for disabling (masking off) the chosen IRQ. Needed by
  // EnableSbInterrupts()/DisableSbInterrupts().
  //
  // sbIntMask is for the primary PIC, which handles IRQs 0..7, and
  // sbIntMask2 is for the secondary PIC (IRQs 8..15).
  //
  // Valid IRQs for the SoundBlaster are 2, 3, 5, 7, and 10.
  //
  // [BUG] If the chosen IRQ is 10, then sbIntMask for the primary PIC
  // shouldn't be relevant, but it is in fact still used within
  // EnableSbInterrupts() and DisableSbInterrupts(). That would be ok if
  // sbIntMask was set to 0 here, but with sbInterrupt being 10, it ends up
  // being set to 4 instead (10 & 7 = 2, 1 << 2 = 4). This is the bit for IRQ
  // 2 on the primary PIC, which is the IRQ line used by the secondary PIC to
  // notify the primary one. Because of the way sbIntMask is set here, this
  // means that DisableSbInterrupts() could potentially disable the entire
  // secondary PIC, i.e. IRQs 8 through 15. The only reason this doesn't happen
  // is because of the slightly odd way DisableSbInterrupts() is implemented.
  // See that function for more details.
  //
  // One could also argue that this function here is correct, and that
  // EnableSbInterrupts/DisableSbInterrupts are wrong to make use of sbIntMask
  // if the interrupt is 10. That's a perfectly reasonable way to think about
  // it, but if we assume that those two functions are correct, then we must
  // set sbIntMask to 0 here in case the channel is 10.
  sbIntMask = 1 << (sbInterrupt & 0x7);

  // The only possible IRQ to be handled by the secondary PIC is 10. That IRQ
  // line is connected to the 3rd input line on the secondary PIC, so our mask
  // to disable that IRQ is 100b = 4.
  sbIntMask2 = 4;

  // Install our own interrupt handler
  sbSavedIntHandler = SNDLIB_getvect(sbIntVec);
  SNDLIB_setvect(sbIntVec, SBService);

  // [NOTE] It would seem slightly more correct to disable interrupts before
  // installing our handler. But the way it's done here doesn't seem to cause
  // any issues either.
  DISABLE_INTERRUPTS();

  // Turn on the DSP speaker - basically unmuting the sound card's digital
  // audio output
  sbAwaitReady();
  sbOut(sbWriteCmd, CMD_TURN_SPEAKER_ON);

  ENABLE_INTERRUPTS();
}


/** Stop sound playback and restore interrupt handler */
static void ShutdownSoundBlaster(void)
{
  SB_StopSound();

  SNDLIB_setvect(sbIntVec, sbSavedIntHandler);
}


/** Send a command to the AdLib hardware, using sbAlAddress
  *
  * Very similar to alOut() from Wolf3D. The only difference is that this
  * function can use different port addresses, whereas the one from Wolf3D
  * hardcodes port 0x388.
  *
  * Also compare to WriteAdLibReg in basicsnd.c and music.c - the game features
  * three variants of this routine in total.
  */
static void WriteSBAdLibReg(byte reg, byte val)
{
  DISABLE_INTERRUPTS();

  // Write address register
  asm mov   dx, [sbAlAddress]
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
  asm inc   dx
  asm mov   al, [val]
  asm out   dx, al

  ENABLE_INTERRUPTS();

  // Wait for at least 23 usecs by executing 35 IN instructions
  // (as recommended in the AdLib documentation)
  asm dec   dx
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
}


/** Detect an OPL2 (or compatible) device at I/O port sbAlAddress
 *
 * Also initializes the hardware to a known state if found.
 *
 * Almost identical to SDL_DetectAdLib() from Wolf3D.
 * Also pretty much identical to DetectAdLib() from Cosmo.
 */
static bool DetectAndInitAdLib(void)
{
  byte status1, status2;
  int i;

  // The detection works by using the OPL2's two on-chip timers, T1 and T2.
  // For a detailed description of how this works, I refer you to Scott
  // Smitelli's page: https://cosmodoc.org/topics/adlib-functions/#DetectAdLib
  //
  // The gist of it is that we reset the timers, capture their flag bits, then
  // start the timers, wait for some time, and capture the flag bits again.
  // At that point, we expect the first capture to indicate that the timers
  // haven't fired, and the second capture should say they have.

  // Reset and disable both timers
  WriteSBAdLibReg(4, 0x60);
  WriteSBAdLibReg(4, 0x80);

  // Capture status flags
  status1 = inportb(sbAlAddress);

  // Start timer 1, let it fire once every 80 usecs
  WriteSBAdLibReg(2, 0xFF);
  WriteSBAdLibReg(4, 0x21);

  // Wait for at least 100 usecs
  asm mov  dx, [sbAlAddress]
  asm mov  cx, 100
waitLoop:
  asm in   al, dx
  asm loop waitLoop

  // Capture status flags again
  status2 = inportb(sbAlAddress);

  // Stop the timers
  WriteSBAdLibReg(4, 0x60);
  WriteSBAdLibReg(4, 0x80);

  // Now check that the status before starting a timer is 0 (none of the timers
  // have fired), and that the status after starting timer 1 indicates that T1
  // has fired, but T2 hasn't.
  if (((status1 & 0xe0) == 0x00) && ((status2 & 0xe0) == 0xc0))
  {
    // We've successfully detected an AdLib/OPL2 at the specified address.
    // Set the hardware to a known initial state.

    // Zero out all registers
    //
    // [NOTE] This also attempts to write to a few non-existent register
    // addresses, since there are gaps in the hardware's register address
    // space.  But that doesn't seem to cause any issues.
    for (i = 1; i <= 0xF5; i++)
    {
      WriteSBAdLibReg(i, 0);
    }

    // Enable waveform selection
    WriteSBAdLibReg(1, 0x20);

    // Disable CSM (Composite Sine Wave Speech Modelig) and set Note Select
    // (Keyboard Split) to 0.
    //
    // [NOTE] Redundant, since the loop above already reset register 8.
    WriteSBAdLibReg(8, 0);

    return true;
  }
  else
  {
    // Not an OPL2/AdLib
    return false;
  }
}


/** Replacement for C library's strtol */
static long SNDLIB_strtol(char* str, char** endOfNumber, int radix)
{
  long result = 0;
  char digit;
  int num;

  while (*str)
  {
    digit = *str;

    if (digit >= '0' && digit <= '9')
    {
      num = digit - '0';
    }
    else if (digit >= 'a' && digit <= 'f')
    {
      num = digit - 'a' + 10;
    }
    else if (digit >= 'A' && digit <= 'F')
    {
      num = digit - 'A' + 10;
    }
    else
    {
      break;
    }

    result *= radix;
    result += num;

    str++;
  }

  if (endOfNumber)
  {
    *endOfNumber = str;
  }

  return result;
}


/** Parse BLASTER configuration string and set global variables accordingly
 *
 * This doesn't exist as an individual function in Wolf3D, but the body is
 * extremely similar to parts of SD_Startup(). The main difference is that
 * standard library functions have been replaced with custom counterparts.
 */
static int ParseBlasterConfig(char* env)
{
  long temp;
  char c;
  register int portIndex = -1;

  while (*env)
  {
    // Skip leading spaces
    while (SNDLIB_isspace(*env))
    {
      env++;
    }

    c = *env;

    // Convert `c` to uppercase. The Wolfenstein 3D version of this code is
    // using the standard toupper() function for this.
    if (c >= 'a' && c <= 'z')
    {
      c = c - 'a' + 'A';
    }

    switch (c)
    {
      case 'A':
        temp = SNDLIB_strtol(env + 1, &env, 16);

        if (temp >= 0x210 && temp <= 0x260 && !(temp & 0x00F))
        {
          portIndex = (temp - 0x200) >> 4;
        }
        else
        {
          sndParseEnvError = "Unsupported address value";
          return -1;
        }
        break;

      case 'I':
        temp = SNDLIB_strtol(env + 1, &env, 10);

        if (temp >= 0 && temp <= 10 && INTERRUPT_VECTORS[temp] != -1)
        {
          sbInterrupt = temp;
          sbIntVec = INTERRUPT_VECTORS[sbInterrupt];
        }
        else
        {
          sndParseEnvError = "Unsupported interrupt value";
          return -1;
        }
        break;

      case 'D':
        temp = SNDLIB_strtol(env + 1, &env, 10);

        if (temp == 0 || temp == 1 || temp == 3)
        {
          SetDmaChannel(temp);
        }
        else
        {
          sndParseEnvError = "Unsupported DMA channel";
          return -1;
        }
        break;

      default:
        // Skip unrecognized settings
        while (*env && !SNDLIB_isspace(*env))
        {
          env++;
        }
        break;
    }
  }

  return portIndex;
}


/** Initialize Sound Blaster, using specified BLASTER string if not NULL
 *
 * The result of getenv("BLASTER") should be given to this function.
 *
 * [NOTE] It seems a bit odd that the function doesn't call getenv itself, but
 * I assume this is because the library avoids using any standard library
 * functions.
 *
 * Sets SoundBlasterPresent and AdLibPresent variables.
 * Returns NULL on success, a pointer to an error message string otherwise.
 *
 * Public function.
 */
char* SB_Init(char* blasterEnvVar)
{
  int portIndex;

  // Skip nested initialization attempts
  if (sndInitialized)
  {
    return NULL;
  }

  sbSoundFinishedCallback = NULL;
  sbNewVocSectionCallback = NULL;

  // Check if there's a (potentially standalone) AdLib in the system
  if (DetectAndInitAdLib())
  {
    AdLibPresent = true;
  }

  portIndex = -1;

  if (blasterEnvVar)
  {
    portIndex = ParseBlasterConfig(blasterEnvVar);

    if (sndParseEnvError != NULL)
    {
      return sndParseEnvError;
    }
  }

  SoundBlasterPresent = DetectSoundBlaster(portIndex);

  if (SoundBlasterPresent)
  {
    // If we have a Sound Blaster, we also have an AdLib.
    AdLibPresent = true;

    InitSoundBlaster();
  }

  sndInitialized = true;

  return NULL;
}


/** Shut down SoundBlaster and reset hardware detection variables
 *
 * Public function.
 */
void SB_Shutdown(void)
{
  if (sndInitialized)
  {
    if (SoundBlasterPresent)
    {
      ShutdownSoundBlaster();
    }

    AdLibPresent = SoundBlasterPresent = false;
    sndInitialized = false;
  }
}
