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

Demo playback and recording

The game includes a pre-recorded demo which is shown during the attract loop.
Demo playback works exactly like regular gameplay, except that pre-recorded
inputs are used to control the player instead of keyboard or joystick input.
The pre-recorded inputs are stored in a data file, the list of levels to play
is hard-coded.

There is no mechanism in the shipping game to trigger it, but almost all of the
code for demo recording is still present. We don't know how recording was
triggered in development builds of the game, it could've been a special key
combination, a command line argument, or maybe an environment variable.

*******************************************************************************/


#define DEMO_DATA_CHUNK_SIZE 128


/** Prepare demo playback
 *
 * Must be called before ReadDemoInput() can be used.
 * demoIsPlaying must be true, otherwise this is a no-op.
 */
void InitDemoPlayback(void)
{
  if (!demoIsPlaying) { return; }

  demoFramesProcessed = 0;
  OpenAssetFile("NUKEM2.MNI", &demoFileFd);
}


/** Read next demo input packet and set input variables accordingly
 *
 * During demo playback, this function replaces keyboard/joystick input.
 * Returns true if the next level should be loaded, false otherwise. The end
 * of the demo data is indicated by changing gmGameState.
 */
bool ReadDemoInput(void)
{
  // Fetch the next chunk of data from disk if needed
  if (demoFramesProcessed == 0 || demoFramesProcessed == DEMO_DATA_CHUNK_SIZE)
  {
    // [UNSAFE] No checking if the file has enough data left
    _read(demoFileFd, demoData, DEMO_DATA_CHUNK_SIZE);
    demoFramesProcessed = 0;
  }

  // All bits set indicates end of demo data
  if (demoData[demoFramesProcessed] == 0xFF)
  {
    gmGameState = GS_QUIT;
    return false;
  }

  {
    // Decode data packet
    register byte frameData = demoData[demoFramesProcessed];

    inputMoveUp = (frameData & 0x1);
    inputMoveDown = (frameData & 0x2) >> 1;
    inputMoveLeft = (frameData & 0x4) >> 2;
    inputMoveRight = (frameData & 0x8) >> 3;
    inputJump = (frameData & 0x10) >> 4;
    inputFire = (frameData & 0x20) >> 5;

    demoFramesProcessed++;

    // High bit set indicates end of current level
    if (frameData & 0x80)
    {
      return true;
    }
  }

  return false;
}


/** Wrap up demo playback. No-op if demoIsPlaying is false */
void FinishDemoPlayback(void)
{
  if (!demoIsPlaying) { return; }

  demoIsPlaying = false;
  CloseFile(demoFileFd);
}


/** Prepare demo recording.
 *
 * Must be called before RecordDemoInput() can be used.
 * No-op if demoIsRecording is false.
 *
 * demoIsRecording is never true in the shipping version of the game, so this
 * function is effectively dead code despite still being invoked by the rest of
 * the code.
 */
void InitDemoRecording(void)
{
  if (!demoIsRecording) { return; }

  demoFramesProcessed = 0;
  demoFileFd = OpenFileW("NUKEM2.MNI");
}


/** Flush recorded demo input to the demo data file
 *
 * No-op if demoIsRecording is false.
 */
static void pascal WriteDemoDataChunk(word size)
{
  if (!demoIsRecording) { return; }

  _write(demoFileFd, demoData, size);
}


/** Wrap up demo recording. No-op if demoIsRecording is false.
 *
 * demoIsRecording is never true in the shipping version of the game, so this
 * function is effectively dead code despite still being invoked by the rest of
 * the code.
 */
void FinishDemoRecording(void)
{
  if (!demoIsRecording) { return; }

  // Add the "end of demo" marker byte to the end of the buffer
  demoData[demoFramesProcessed] = 0xFF;

  // Write one additional byte to include the end marker
  WriteDemoDataChunk(demoFramesProcessed + 1);

  CloseFile(demoFileFd);
  demoIsRecording = false;
}


/** Record one demo input data packet. No-op if demoIsRecording is false.
 *
 * Takes the state of the input variables as set by keyboard or joystick input,
 * and adds it to the current demo recording. Returns true if the next level
 * should be loaded, false otherwise.
 *
 * demoIsRecording is never true in the shipping version of the game, so this
 * function is effectively dead code despite still being invoked by the rest of
 * the code.
 */
bool RecordDemoInput(void)
{
  if (!demoIsRecording) { return false; }

  // Flush buffer to file whenever a full chunk has been recorded
  if (demoFramesProcessed == DEMO_DATA_CHUNK_SIZE)
  {
    WriteDemoDataChunk(DEMO_DATA_CHUNK_SIZE);
    demoFramesProcessed = 0;
  }

  // Encode current input state
  demoData[demoFramesProcessed] =
    inputMoveUp |
    (inputMoveDown << 1) |
    (inputMoveLeft << 2) |
    (inputMoveRight << 3) |
    (inputJump << 4) |
    (inputFire << 5);
  demoFramesProcessed++;

  // Record a "change level" command if space key pressed
  if (kbKeyState[SCANCODE_SPACE])
  {
    *(demoData - 1 + demoFramesProcessed) =
      *(demoData - 1 + demoFramesProcessed) | 0x80;
    return true;
  }

  return false;
}
