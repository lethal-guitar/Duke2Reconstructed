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

Joystick support code, part 1: Joystick polling

*******************************************************************************/


/** Determine x/y position of the joystick */
static void pascal PollJoystickPosition(int* xAxis, int* yAxis)
{
  *xAxis = 0;
  *yAxis = 0;

  // On the hardware side, the joystick's stick is connected to two
  // potentiometers, which will produce different output currents depending on
  // the stick's position.  These signals are fed into capacitors, which will
  // take a variable amount of time to fully charge depending on the current.
  // For each capacitor, the computer can read a single bit from the Gameport's
  // I/O port, which will be 0 if the capacitor is fully charged and 1
  // otherwise.  It's also possible to discharge the capacitors by writing a
  // value to the Gameport I/O port.
  //
  // To figure out the location of the joystick, the overall process is as
  // follows:
  //
  // 1. Discharge capacitors
  // 2. Poll capacitor state until the bit becomes zero
  // 3. Note how long it took for the capacitor to become fully charged
  //
  // The time measurement determines the axis' position.
  //
  // This function implements this exact scheme. It doesn't measure precise
  // time however, just the number of loop iterations. This number will vary
  // widely between different systems, necessitating a calibration step to
  // establish a frame of reference for how to interpret the numbers.
  //
  // For a much more in-depth explanation, have a look at:
  //
  //   https://cosmodoc.org/topics/joystick-functions

  // Discharge capacitors
  DN2_outportb(0x0201, DN2_inportb(0x0201));

  // Now measure how many loop iterations it takes for each axis' capacitor to
  // be fully charged.
  do
  {
    word data;
    int isWaitingX;
    int isWaitingY;

    // [NOTE] It would make more sense to disable interrupts for the entire
    // loop, since an interrupt can still fire after reading the port and throw
    // off the timing.
    disable();
    data = DN2_inportb(0x0201);
    enable();

    // Sample axis state bits, as long as a bit is 1, we are waiting for the
    // capacitor to become charged
    isWaitingX = (data & 1) != 0;
    isWaitingY = (data & 2) != 0;

    // Count number of iterations we spent waiting, for each axis
    *xAxis += isWaitingX;
    *yAxis += isWaitingY;

    // Stop the loop when both capacitors have become charged
    if (isWaitingX + isWaitingY == 0)
    {
      break;
    }
  }
  // As a safety precaution, also abort the loop if one of the capacitors
  // doesn't become charged after 500 loop iterations
  while (*xAxis < 500 && *yAxis < 500);
}


/** Set state of inputXXX variables based on joystick state
 *
 * Joystick must be calibrated before using this function.
 */
void pascal PollJoystick(void)
{
  word buttonState;
  int xAxis;
  int yAxis;

  inputMoveLeft = false;
  inputMoveRight = false;
  inputMoveUp = false;
  inputMoveDown = false;

  // Poll joystick x/y axes and set input variables based on stick position,
  // using the threshold values computed during calibration
  PollJoystickPosition(&xAxis, &yAxis);

  if (xAxis > jsThresholdRight - 1)
  {
    inputMoveRight = true;
  }
  else if (xAxis < jsThresholdLeft)
  {
    inputMoveLeft = true;
  }

  if (yAxis > jsThresholdDown - 1)
  {
    inputMoveDown = true;
  }
  else if (yAxis < jsThresholdUp)
  {
    inputMoveUp = true;
  }

  // Read joystick button state, each button is encoded as a bit in the
  // Gameport status byte.
  buttonState = DN2_inportb(0x0201);

  if (jsButtonsSwapped)
  {
    inputJump = !(buttonState & 0x10); // test button 1
    inputFire = !(buttonState & 0x20); // test button 2
  }
  else
  {
    inputJump = !(buttonState & 0x20); // test button 2
    inputFire = !(buttonState & 0x10); // test button 1
  }

  // Also store the state of the 3rd and 4th buttons. Traditionally, these
  // would belong to a 2nd joystick connected to the computer, but they might
  // also be part of one device along with the first two buttons, for example
  // when using a Gravis Gamepad.
  //
  // I assume that the intention here is to support the latter, since the only
  // usage for jsButton4 is to act as pause button (see RunInGameLoop() in
  // main.c). It would be strange to use one joystick for playing the game and
  // a 2nd one to pause the game, but making use of the extra buttons on a
  // Gravis Gamepad makes sense.
  jsButton3 = !(buttonState & 0x30); // [NOTE] Unused
  jsButton4 = !(buttonState & 0x40);
}
