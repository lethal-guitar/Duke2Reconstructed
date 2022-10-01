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

Joystick support code, part 2: Joystick calibration

*******************************************************************************/


/** Runs the joystick calibration dialog
 *
 * This function draws text on screen during the process, but it doesn't draw
 * the message box frame. This is handled by a script. See ShowOptionsMenu() in
 * main.c.
 *
 * Returns true if interrupted by a keypress, false if it ran to completion.
 */
bool pascal RunJoystickCalibration(void)
{
  word i;
  int xDelta;
  int yDelta;
  int xMin;
  int yMin;
  int xMax;
  int yMax;
  byte data;

  DrawText(5, 6, "Move the joystick towards the");
  DrawText(5, 7, "UPPER LEFT and press a button.");

  // This loop keeps polling the joystick position until either button 1 or 2
  // is pressed. The result is stored in xMin and yMin.
  i = 15;
  do
  {
    // No clue what this is for, the variable is never read.
    if (++i == 23)
    {
      i = 15;
    }

    PollJoystickPosition(&xMin, &yMin);

    // Abort on any keyboard input
    if (ANY_KEY_PRESSED())
    {
      return true;
    }

    // Read joystick button state
    data = DN2_inportb(0x201);
  }
  while (data & 0x20 && data & 0x10);

  // Now wait until neither of the two buttons is pressed
  do
  {
    data = DN2_inportb(0x201);
  }
  while ((data & 0x30) != 0x30);

  // Wait ~285 ms
  WaitTicks(80);

  DrawText(5, 9, "Move the joystick towards the");
  DrawText(5, 10, "LOWER RIGHT and press a button.");

  // This is the same loop as above, but this time, the results are stored in
  // xMax and yMax.
  i = 15;
  do
  {
    // No clue what this is for, the variable is never read.
    if (++i == 23)
    {
      i = 15;
    }

    PollJoystickPosition(&xMax, &yMax);

    // Abort on any keyboard input
    if (ANY_KEY_PRESSED())
    {
      return true;
    }

    // Read joystick button state
    data = DN2_inportb(0x201);
  }
  while (data & 0x20 && data & 0x10);

  // Wait ~285 ms
  WaitTicks(80);

  DrawText(5, 12, "Select fire button.  The other");
  DrawText(5, 13, "button is used for jumping.");

  // Wait for a button press
  do
  {
    // Abort on any keyboard input
    if (ANY_KEY_PRESSED())
    {
      return true;
    }

    // Read joystick button state
    data = DN2_inportb(0x201);
  }
  while (data & 0x20 && data & 0x10);

  // If the 1st button was pressed, then the buttons are considered "swapped"
  // with respect to the default, which is that the 1st button is for jumping
  // and the 2nd one for shooting.
  if (data & 0x20)
  {
    jsButtonsSwapped = false;
  }
  else
  {
    jsButtonsSwapped = true;
  }

  // Compute threshold values from the minimum and maximum axis values we
  // measured above
  xDelta = (xMax - xMin) / 6;
  yDelta = (yMax - yMin) / 6;

  jsThresholdLeft  = xMin + xDelta;
  jsThresholdRight = xMax - xDelta;
  jsThresholdUp    = yMin + yDelta;
  jsThresholdDown  = yMax - yDelta;

  // The joystick is now calibrated, and can be used for input - set the
  // corresponding flag to inform other parts of the code.
  jsCalibrated = true;

  return false;
}
