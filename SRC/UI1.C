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

UI code, part 1

Many of the functions here are primarily implementing functionality offered
by the scripting system.

*******************************************************************************/


/** Draw animated menu cursor, or erase it
 *
 * If a menu cursor is currently active and true is passed to the function, it
 * draws a single frame of the menu cursor, advances the animation state (so
 * a different frame will be drawn next time), and waits for 7 ticks (50 ms).
 * If false is passed, the cursor's last known location is erased by
 * overdrawning with black.
 */
static void UpdateMenuCursor(bool show)
{
  if (uiMenuCursorPos == 0)
  {
    return;
  }

  if (show)
  {
    // The animation has 8 frames, so conceptually, the step variable
    // ranges from 0 to 7 and is incremented by 1 each time.
    // By incrementing it in steps of 16 instead, the step variable is
    // directly usable as source offset without an additional multiplication.
    //
    // [NOTE] Multiplying by 16 is just a left shift, which is very fast.
    // So this seems like a premature optimization, making the code slightly
    // more complex without any significant/important speed gains.
    uiMenuCursorAnimStep += 16;
    if (uiMenuCursorAnimStep == 8 * 16)
    {
      uiMenuCursorAnimStep = 0;
    }

    if (uiMenuCursorPos!= 0)
    {
      DrawStatusIcon_2x2(
        uiMenuCursorAnimStep + XY_TO_OFFSET(0, 9),
        8,
        uiMenuCursorPos - 1);
    }

    // [NOTE] This is a 50ms delay, which makes the cursor animate
    // at 20 frames per second. Unfortunately, it also slows down the
    // menu's input polling to that rate, which feels a little sluggish.
    // A better approach would be to update the menu at full framerate,
    // and use the tick counter to pace the animation.
    // I.e. something like:
    //
    // if (sysTicksElapsed - menuCursorTicks >= 7) {
    //   // update the animation step, redraw
    //   menuCursorTicks = sysTicksElapsed;
    //
    // Interestingly, a similar approach is actually used in
    // DrawTextEntryCursor, but not here.
    WaitTicks(7);
  }
  else
  {
    // Clear a previously shown cursor
    DrawStatusIcon_2x2(XY_TO_OFFSET(0, 5), 8, uiMenuCursorPos - 1);
  }
}


#include "joystk1.c"


/** Pauses execution until keyboard or joystick input is received
  *
  * Primarily used by the scripting system.
  * Returns the scancode for the key that was pressed (joystick
  * inputs are mapped to scancodes as well).
  * If "timeout to demo" is active, the function also returns
  * once the timeout has elapsed - it returns 0xDF in that case.
  *
  * If a menu cursor is currently visible, it is animated while
  * waiting.
  */
byte pascal AwaitInput(void)
{
  UpdateMenuCursor(true);

  while ((kbLastScancode & 0x80))
  {
    if (uiDemoTimeoutTime)
    {
      uiDemoTimeoutTime++;
      if (uiDemoTimeoutTime == 600)
      {
        return 0xDF;
      }
    }

    if (ANY_KEY_PRESSED())
    {
      UpdateMenuCursor(false);
      return LAST_SCANCODE();
    }

    if (jsCalibrated)
    {
      // [NOTE] This delay causes the menu to feel sluggish if the joystick
      // calibration has been run in the past. It doesn't matter if a joystick
      // is actually plugged in or not.  4 ticks is roughly 28.6ms. Each call
      // to UpdateMenuCursor already delays for 50 ms. This means that the
      // menu's framerate drops from 20 to 12 FPS if the joystick calibration
      // has been run at some point.
      //
      // I assume this was done because navigating the menus with a joystick
      // felt too fast otherwise. But a much better approach would've been to
      // use a tick counter and only poll the joystick whenever enough time has
      // elapsed, instead of slowing down the keyboard navigation.
      WaitTicks(4);

      PollJoystick();

      if (inputMoveDown || inputMoveRight)
      {
        UpdateMenuCursor(false);
        return SCANCODE_DOWN;
      }
      else if (inputMoveUp || inputMoveLeft)
      {
        UpdateMenuCursor(false);
        return SCANCODE_UP;
      }
      else if (inputFire)
      {
        UpdateMenuCursor(false);
        return SCANCODE_ENTER;
      }
    }

    UpdateMenuCursor(true);
  }

  UpdateMenuCursor(false);
  return LAST_SCANCODE();
}


/** Pauses execution until timeout elapsed or keyboard input received
  *
  * Like AwaitInput(), returns the scancode for the key that was pressed,
  * or 0xFE if it returns due to the timeout.
  * Unlike AwaitInput(), it doesn't respond to joystick inputs.
  *
  * If the talking news reporter is currently shown, this function
  * animates it while waiting.
  */
byte AwaitInputOrTimeout(word ticksToWait)
{
  if (kbKeyState[SCANCODE_UP])
  {
    return SCANCODE_UP;
  }
  else if (kbKeyState[SCANCODE_DOWN])
  {
    return SCANCODE_DOWN;
  }

  // Wait until a currently held key is released, if any
  while (ANY_KEY_PRESSED());

  sysTicksElapsed = 0;
  while (kbLastScancode & 0x80)
  {
    if (uiReporterTalkAnimTicksLeft && sysFastTicksElapsed % 25 == 0)
    {
      uiReporterTalkAnimTicksLeft--;
      DrawNewsReporterTalkAnim();
    }

    if (sysTicksElapsed >= ticksToWait)
    {
      return 0xFE;
    }
  }

  return LAST_SCANCODE();
}


/** Draws a blinking cursor at the specified location */
static void pascal DrawTextEntryCursor(word x, word y)
{
  if (sysTicksElapsed > 5)
  {
    uiTextEntryCursorAnimStep += 8;
    sysTicksElapsed = 0;
  }

  if (uiTextEntryCursorAnimStep == 4 * 8)
  {
    uiTextEntryCursorAnimStep = 0;
  }

  DrawStatusIcon_1x1(uiTextEntryCursorAnimStep + XY_TO_OFFSET(9, 4), x, y);
}


/** Draw text entry cursor and wait for text input, then return it */
byte pascal GetTextInput(word x, word y)
{
  while (uiTextEntryLastScancode == LAST_SCANCODE())
  {
    if (kbLastScancode & 0x80)
    {
      break;
    }

    DrawTextEntryCursor(x, y);
  }

  while (kbLastScancode & 0x80)
  {
    DrawTextEntryCursor(x, y);
  }

  uiTextEntryLastScancode = LAST_SCANCODE();

  // Erase the cursor
  DrawStatusIcon_1x1(XY_TO_OFFSET(8, 4), x, y);

  return LAST_SCANCODE();
}


#include "scrfade.c"


/** Wait until progress bar is full, then fade out
 *
 * The progress bar (shown during the loading screen) is advanced by the timer
 * interrupt handler in music.c. This function sets the progress bar speed to
 * the maximum and then waits until the progress bar is completely filled up.
 */
void AwaitProgressBarEnd(void)
{
  if (uiProgressBarState)
  {
    // Wait until the progress bar reaches the end. The progress bar
    // is driven by the timer interrupt handler.
    while (uiProgressBarState != 284)
    {
      // Set the progress bar to fastest speed
      uiProgressBarStepDelay = 0;
    }
  }

  // [NOTE] Could use FadeOutScreen() here.
  FadeOutFromPalette(gfxCurrentPalette);
}


/** Draw single character at specified location, using small orange font */
static void pascal DrawSmallTextChar(word x, word y, char c)
{
  if (c == '_')
  {
    c = 0x1F;
  }

  if (c <= '=')
  {
    DrawStatusIcon_1x1(T2PX(c - 0x16) + XY_TO_OFFSET(0, 21), x, y);
  }
  else if (c <= 'Z')
  {
    DrawStatusIcon_1x1(T2PX(c - '>') + XY_TO_OFFSET(0, 22), x, y);
  }
  else if (c <= 'k')
  {
    DrawStatusIcon_1x1(T2PX(c - 'a') + XY_TO_OFFSET(29, 22), x, y);
  }
  else
  {
    DrawStatusIcon_1x1(T2PX(c - 'l') + XY_TO_OFFSET(17, 23), x, y);
  }
}


/** Draw text or sprite
 *
 * This function is strangely overloaded. When given a regular string,
 * it draws it using a small orange font. But special marker codes can trigger
 * alternative behavior:
 *
 * 0xEF       - draw sprite
 * 0xF0..0xFF - use large colorized font
 *
 * For drawing a sprite, the marker byte must be followed by 5 numbers. The
 * first three numbers are interpreted as actor ID, the last two as animation
 * frame.
 *
 * For drawing with the large font, the low nibble of the marker byte indicates
 * the color index to use. I.e. 0xF2 is color index 2, 0xFB is index 11, etc.
 * The different ways of drawing can be mixed within a single call to the
 * function, although small text can't be used anymore once large text was
 * requested. I.e. if there's a 0xFn marker anywhere in the string, all
 * text following that marker byte will be large and colorized. The color
 * can be changed with another marker byte, but there's no way to revert back
 * to the small font except by invoking the function again.
 *
 * [HACK] This whole scheme feels hacky. The special marker bytes are only
 * used within script files, which can invoke this function via the XYTEXT
 * command. I'm not sure why the scripting system doesn't have dedicated
 * commands for drawing sprites and large text. That would make this function
 * much simpler.
 */
void pascal DrawText(word x, word y, char far* text)
{
  register int i;
  register int currentX = x;
  word color = 0;
  word actorId;
  word frame;
  char numStr[4];

  for (i = 0; text[i]; i++)
  {
    if (text[i] == 0xEF)
    {
      // [UNSAFE] No checking here that we have enough characters left to
      // read in the string.
      numStr[0] = text[i + 1];
      numStr[1] = text[i + 2];
      numStr[2] = text[i + 3];
      numStr[3] = '\0';
      actorId = atoi(numStr);

      numStr[0] = text[i + 4];
      numStr[1] = text[i + 5];
      numStr[2] = '\0';
      frame = atoi(numStr);

      DrawSprite(actorId, frame, currentX + i + 2, y + 1);

      i += 5;
    }
    else if (text[i] >= 0xF0)
    {
      // A value of 0 in the variable `color` means "no color".
      // In order to be able to use color index 0, we add 1 here to make
      // the variable non-zero even if the color index is 0.
      // The drawing code below subtracts 1 again before using the color
      // index.
      color = text[i] - 0xF0 + 1;
    }
    else
    {
      if (color)
      {
        DrawBigTextChar(currentX + i, y, text[i], color - 1);
      }
      else
      {
        DrawSmallTextChar(currentX + i, y, text[i]);
      }
    }
  }
}


/** Draws the key name for the given scancode at the given position */
static void pascal DrawKeyBinding(word x, word y, byte bind)
{
  register char* keyName = KEY_NAMES[bind];

  DrawText(x, y, keyName);
}


/** Draw key names for all key bindings
 *
 * Implements the KEYS script command.
 */
void pascal DrawKeyBindings(void)
{
  DrawKeyBinding(26,  7, kbBindingFire);
  DrawKeyBinding(26,  9, kbBindingJump);
  DrawKeyBinding(26, 11, kbBindingUp);
  DrawKeyBinding(26, 13, kbBindingDown);
  DrawKeyBinding(26, 15, kbBindingLeft);
  DrawKeyBinding(26, 17, kbBindingRight);
}


/** Lets user enter name for a saved game
 *
 * Runs until the user either confirms or cancels their input.
 * The entered text is placed into nameBuffer.
 * Returns true if confirmed, false otherwise.
 */
static bool pascal SaveGameNameEntry_Impl(
  word x, word y, char far* nameBuffer, word maxLen)
{
  int cursorPos = 0;
  byte scancode;
  char caseOffset;

  // Editing an existing name? Move cursor to the end if so
  if (*nameBuffer)
  {
    cursorPos = DN2_strlen(nameBuffer);
  }

  for (;;)
  {
    // Erase location above the current cursor position. Not quite sure
    // what this is for, since deleting a character already erases it.
    DrawStatusIcon_1x1(XY_TO_OFFSET(8, 4), x + cursorPos + 2, y - 1);

    scancode = GetTextInput(x + cursorPos + 2, y);

    if (scancode == SCANCODE_ENTER)
    {
      // Editing done, terminate string and return success
      nameBuffer[cursorPos] = '\0';
      return true;
    }
    else if (scancode == SCANCODE_ESC)
    {
      // User cancelled editing
      return false;
    }
    else if (scancode == SCANCODE_BACKSPACE)
    {
      // Delete right-most character
      if (cursorPos > 0)
      {
        // Erase previously drawn character
        DrawBigTextChar(x + cursorPos + 2, y, ' ', 1);
        cursorPos--;
      }
    }
    else if (cursorPos < maxLen)
    {
      // A character was entered, add it to the string if it's one we recognize
      if (
        (scancode >= SCANCODE_1 && scancode <= SCANCODE_0) ||
        (scancode >= SCANCODE_Q && scancode <= SCANCODE_P) ||
        (scancode >= SCANCODE_A && scancode <= SCANCODE_L) ||
        (scancode >= SCANCODE_Z && scancode <= SCANCODE_M))
      {
        // Handle upper case ...
        if (kbKeyState[SCANCODE_LEFT_SHIFT] || kbKeyState[SCANCODE_RIGHT_SHIFT])
        {
          caseOffset = 0;
        }
        else
        {
          caseOffset = 0x20;
        }

        // ... but not for numeric characters.
        if (scancode >= SCANCODE_1 && scancode <= SCANCODE_EQUAL)
        {
          caseOffset = 0;
        }

        nameBuffer[cursorPos] = KEY_NAMES[scancode][0] + caseOffset;
        cursorPos++;

        DrawBigTextChar(x + cursorPos, y, KEY_NAMES[scancode][0] + caseOffset, 2);
      }
      else if (scancode == SCANCODE_SPACE)
      {
        // Add a space
        nameBuffer[cursorPos] = ' ';
        cursorPos++;
      }
    }
  }
}


/** Lets user enter name for high-score list
 *
 * Extremely similar to SaveGameNameEntry_Impl, with the following differences:
 *
 *  - no indication if confirmed or cancelled
 *  - using a different font (small orange font)
 *  - no logic for editing an existing entry, always starts empty
 *  - supports some additional punctuation
 *
 * [NOTE] It seems like it should be possible to extract some of the common
 * logic here into a shared helper function to reduce code duplication.
 */
static void pascal RunHighScoreNameEntry(
  word x, word y, char far* nameBuffer, word maxLen)
{
  int cursorPos = 0;
  byte scancode;
  char caseOffset;

  for (;;)
  {
    // This also erases a character that was just deleted, by overdrawing it
    // with the cursor. 
    scancode = GetTextInput(x + cursorPos + 1, y);

    if (scancode == SCANCODE_ENTER)
    {
      // Editing done, terminate the string and exit
      nameBuffer[cursorPos] = '\0';
      return;
    }
    else if (scancode == SCANCODE_ESC)
    {
      // User cancelled, make output an empty string
      nameBuffer[0] = '\0';
      return;
    }
    else if (scancode == SCANCODE_BACKSPACE)
    {
      // Delete right-most character, GetTextInput() above will
      // erase it.
      if (cursorPos > 0)
      {
        cursorPos--;
      }
    }
    else if (cursorPos < maxLen)
    {
      // A new character was entered, check if we recognize it and append it
      // to our string if so
      if (
        (scancode >= SCANCODE_1 && scancode <= SCANCODE_0) ||
        (scancode >= SCANCODE_Q && scancode <= SCANCODE_P) ||
        (scancode >= SCANCODE_A && scancode <= SCANCODE_L) ||
        (scancode >= SCANCODE_Z && scancode <= SCANCODE_DOT))
      {
        // Handle upper case ...
        if (kbKeyState[SCANCODE_LEFT_SHIFT] || kbKeyState[SCANCODE_RIGHT_SHIFT])
        {
          caseOffset = 0;
        }
        else
        {
          caseOffset = 0x20;
        }

        // ... except for numerals ...
        if (scancode >= SCANCODE_1 && scancode <= SCANCODE_EQUAL)
        {
          caseOffset = 0;
        }

        // ... and punctuation.
        if (scancode == SCANCODE_COMMA || scancode == SCANCODE_DOT)
        {
          caseOffset = 0;
        }

        nameBuffer[cursorPos] = KEY_NAMES[scancode][0] + caseOffset;
        cursorPos++;

        DrawSmallTextChar(x + cursorPos, y, KEY_NAMES[scancode][0] + caseOffset);
      }
      else if (scancode == SCANCODE_SPACE)
      {
        // Insert a space
        nameBuffer[cursorPos] = ' ';
        cursorPos++;
      }
    }
  }
}
