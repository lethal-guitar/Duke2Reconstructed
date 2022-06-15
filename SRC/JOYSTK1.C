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

Joystick support code, part 1

TODO: Further document this file and the functions here

*******************************************************************************/


static void pascal PollJoystickPosition(int* xAxis, int* yAxis)
{
  *xAxis = 0;
  *yAxis = 0;

  DN2_outportb(0x0201, DN2_inportb(0x0201));

  do
  {
    word data;
    int isWaitingX;
    int isWaitingY;

    disable();
    data = DN2_inportb(0x0201);
    enable();

    isWaitingX = (data & 1) != 0;
    isWaitingY = (data & 2) != 0;

    *xAxis += isWaitingX;
    *yAxis += isWaitingY;

    if (isWaitingX + isWaitingY == 0)
    {
      break;
    }
  }
  while (*xAxis < 500 && *yAxis < 500);
}


void pascal PollJoystick(void)
{
  word buttonState;
  int xAxis;
  int yAxis;

  inputMoveLeft = false;
  inputMoveRight = false;
  inputMoveUp = false;
  inputMoveDown = false;

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

  buttonState = DN2_inportb(0x0201);

  if (jsButtonsSwapped)
  {
    inputJump = !(buttonState & 0x10);
    inputFire = !(buttonState & 0x20);
  }
  else
  {
    inputJump = !(buttonState & 0x20);
    inputFire = !(buttonState & 0x10);
  }

  jsButton3 = !(buttonState & 0x30); // [NOTE] Unused
  jsButton4 = !(buttonState & 0x40);
}
