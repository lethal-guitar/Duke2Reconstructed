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

Scripting system, part 2: Script interpreter

Duke Nukem II is more complex than Cosmo and has more features, but the binary
is very close in size to Cosmo's executable. So how is that possible? One reason
is the scripting system. This makes it possible to express a lot of UI code as
scripts, which are stored in text files along with the other game data. In
Cosmo, all of that UI code was written in C and thus part of the executable.
This includes a lot of text strings, which all have to be stored in the binary
as well. In Duke Nukem II, these strings are in the script files instead.

This system has many advantages:

- Less memory is required, since script files can be loaded and unloaded as
  needed
- Reduced iteration time, since UIs can be modified without needing to
  recompile the code (perhaps even without restarting the game!)
- Artists and game designers can modify aspects of the UIs without needing to
  be fluent in C

Scripting systems are used a lot in modern games as well, for similar reasons.
Although typically they use more advanced scripting languages, which can also
be used to control aspects of the game logic like enemy behavior, conversation
trees, etc. The language used in Duke 2 is very simple: It's generally a linear
list of commands, which can optionally have parameters. The script interpreter
executes these commands one by one, and returns when reaching an `END` command.
Some special commands also make it possible to define menus and paged content,
which is a bit like a loop - script execution can jump back and forth between
parts of the script based on user input.

The vast majority of the game's UI is driven by this scripting system. The only
exceptions are the intro movie, the story screens after an episode ends, and the
bonus screen between levels - all of these are coded entirely in C.

*******************************************************************************/


/** Draw one frame of the news reporter's talking mouth animation */
void DrawNewsReporterTalkAnim(void)
{
  if (!uiReporterTalkAnimTicksLeft)
  {
    DrawSprite(ACT_NEWS_REPORTER_BABBLE, 0, 0, 0);
  }
  else
  {
    DrawSprite(ACT_NEWS_REPORTER_BABBLE, (word)RandomNumber() % 4, 0, 0);
  }
}


/** Toggle a checkbox, or query its state
  *
  * Checkboxes are implemented by the TOGGS script command.  Within the script
  * code, option ids are assigned to each checkbox. The option id determines
  * what happens when a checkbox is toggled.
  *
  * Depending on the value of the first argument, the function toggles the
  * checkbox and returns its state (checked or not), or just returns the
  * current state.
  */
bool pascal QueryOrToggleOption(bool toggle, char optionId)
{
  bool isEnabled = false;

#define HANDLE_DEBUG_SELECTION(n)         \
  if (toggle)                             \
  {                                       \
    debugSelectedFunction = n;            \
  }                                       \
  isEnabled = debugSelectedFunction == n;

  switch (optionId)
  {
    case '1':
      if (toggle)
      {
        debugSelectedFunction = 0;
      }

      isEnabled = !debugSelectedFunction;
      break;

    case '2':
      HANDLE_DEBUG_SELECTION(1);
      break;

    case '3':
      HANDLE_DEBUG_SELECTION(2);
      break;

    case '4':
      HANDLE_DEBUG_SELECTION(3);
      break;

    case 'S': // [S]oundBlaster
      if (toggle)
      {
        sndUseSbSounds = !sndUseSbSounds;
      }

      if (!SoundBlasterPresent)
      {
        sndUseSbSounds = false;
      }

      isEnabled = sndUseSbSounds;

      if (sndUseSbSounds)
      {
        sndUsePcSpeakerSounds = false;
      }
      break;

    case 'L': // Ad[L]ib
      if (toggle)
      {
        sndUseAdLibSounds = !sndUseAdLibSounds;
      }

      if (!AdLibPresent)
      {
        sndUseAdLibSounds = false;
      }

      isEnabled = sndUseAdLibSounds;

      if (sndUseAdLibSounds)
      {
        sndUsePcSpeakerSounds = false;
      }
      break;

    case 'P': // [P]C Speaker
      if (toggle)
      {
        sndUsePcSpeakerSounds = !sndUsePcSpeakerSounds;
      }

      isEnabled = sndUsePcSpeakerSounds;

      if (sndUsePcSpeakerSounds)
      {
        sndUseAdLibSounds = false;
        sndUseSbSounds = false;
      }
      break;

    case 'M': // [M]usic
      if (toggle)
      {
        sndMusicEnabled = !sndMusicEnabled;
      }

      isEnabled = sndMusicEnabled;

      if (!AdLibPresent)
      {
        sndMusicEnabled = false;
      }

      if (!sndMusicEnabled)
      {
        ResetAdLibMusicChannels();
      }
      break;
  }

#undef HANDLE_DEBUG_SELECTION

  return isEnabled;
}


/** Run the script code contained in `text`
 *
 * `text` must point to the first command of the script. This function keeps
 * running until script execution finishes. Depending on the script, this could
 * involve waiting for user input.
 *
 * The script _must_ be terminated by a `//END` command, otherwise the function
 * will keep reading past the end of the script.
 *
 * For context, here are some (shortened) examples from one of the game's
 * script files:
 *
 *   No_Can_Order
 *
 *   //CENTERWINDOW 7 8 36
 *   //SKLINE
 *   //CWTEXT Please read the Ordering Info
 *   //CWTEXT screen to find out how to
 *   //CWTEXT receive the next three exciting
 *   //CWTEXT episodes of Duke Nukem II!
 *   //WAIT
 *   //END
 *
 *   &Instructions
 *
 *   //PAGESSTART
 *   //NOSOUNDS
 *   //SETCURRENTPAGE
 *   //FADEOUT
 *   //LOADRAW KEYBORD1.MNI
 *   //FADEIN
 *   //WAIT
 *
 *   //APAGE
 *   //FADEOUT
 *   //LOADRAW KEYBORD2.MNI
 *   //FADEIN
 *   //WAIT
 *
 *   ... part of the code omitted for brevity ...
 *
 *   //APAGE
 *   //FADEOUT
 *   //LOADRAW HINTS.MNI
 *   //FADEIN
 *   //WAIT
 *   //PAGESEND
 *   //END
 *
 * Also see https://github.com/lethal-guitar/RigelEngine/wiki/DukeScript.
 */
word pascal InterpretScript(char far* text)
{
  register int msgBoxTextY;
  word i;
  word numericParams[3];
  word offset;
  bool hasCheckboxes = false;
  byte checkboxesX;
  bool pagingOnly = false;
  byte numCheckboxes = 0;
  byte scancode;
  char originalStringEnd;
  byte numKeyCodes = 0;
  byte checkboxData[11];
  char paramBuffer[14];
  byte keyCodes[20];

  uiDemoTimeoutTime = 0;
  uiMessageBoxShift = 0;
  uiMenuState = 0;
  scriptPageIndex = 0;
  uiMenuCursorPos = 0;
  uiReporterTalkAnimTicksLeft = 0;

  // This is a fairly straightforward dispatch loop. It interprets the current
  // command and then advances `text` to point to the next command. The loop
  // can be terminated by reaching an `END` command, or by user input during
  // the `DELAY` and `WAIT` commands.
  for (;;)
  {
    originalStringEnd = 0;

    if (StringStartsWith("//FADEIN", text))
    {
      FadeInScreen();
    }
    else if (StringStartsWith("//BABBLEON", text))
    {
      SetUpParameterRead(&text, &originalStringEnd);
      CopyStringUppercased(text, paramBuffer);

      // Set state. The animation itself is handled by the `DELAY` command.
      uiReporterTalkAnimTicksLeft = atoi(paramBuffer);

      UnterminateStr(text, originalStringEnd);
    }
    else if (StringStartsWith("//BABBLEOFF", text))
    {
      uiReporterTalkAnimTicksLeft = 0;
    }
    else if (StringStartsWith("//PAK", text))
    {
      DrawSprite(ACT_PRESS_ANY_KEY, 0, 0, 0);
    }
    else if (StringStartsWith("//NOSOUNDS", text))
    {
      pagingOnly = true;
    }
    else if (StringStartsWith("//FADEOUT", text))
    {
      FadeOutScreen();
    }
    else if (StringStartsWith("//SHIFTWIN", text))
    {
      // Some of the script files have an argument following the command, but
      // the shift value is hard-coded. Most likely, it used to be configurable
      // at some point during development.
      uiMessageBoxShift = 3;
    }
    else if (StringStartsWith("//GETPAL", text))
    {
      SetUpParameterRead(&text, &originalStringEnd);

      SetDrawPage(0);

      // Load a palette into the buffer. It won't be sent to the VGA hardware
      // yet, that happens within FadeInScreen().
      LoadAssetFile(text, gfxCurrentPalette);

      // Clear the screen, since we're about to change the palette and whatever
      // data is currently in the framebuffer would most likely look incorrect
      // with the new palette.
      CLEAR_SCREEN();

      UnterminateStr(text, originalStringEnd);
    }
    else if (StringStartsWith("//LOADRAW", text))
    {
      SetUpParameterRead(&text, &originalStringEnd);

      // This writes the image into the framebuffer and prepares the palette.
      // A fade-in is needed to actually make the image visible on screen.
      DrawFullscreenImage(text);

      UnterminateStr(text, originalStringEnd);
    }
    else if (StringStartsWith("//SETKEYS", text))
    {
      SetUpParameterRead(&text, &originalStringEnd);

      // SETKEYS has one parameter, which is a sequence of scancodes encoded
      // as ASCII characters. Here, we load those scancodes into the keyCodes
      // array. This array is used when handling a `WAIT` command.
      for (i = 0; *(text + i) != '\0'; i++)
      {
        // SCANCODE_D is 0x20 which is a space character. Since that would
        // denote the end of the current token, it can't be used directly in
        // the script code. It's instead encoded as 0x5F in the code, and then
        // replaced with the right scancode value here.
        if (*(text + i) == 0x5F)
        {
          keyCodes[i] = SCANCODE_D;
        }
        else
        {
          // In all other cases, the characters value is already the scancode.
          keyCodes[i] = *(text + i);
        }
      }

      numKeyCodes = i;

      UnterminateStr(text, originalStringEnd);
    }
    else if (StringStartsWith("//DELAY", text))
    {
      byte result;

      SetUpParameterRead(&text, &originalStringEnd);
      CopyStringUppercased(text, paramBuffer);

      // This will also animate the news reporter's talking mouth if there was
      // a `BABBLEON` before.
      result = AwaitInputOrTimeout(atoi(paramBuffer));

      UnterminateStr(text, originalStringEnd);

      // Abort script execution if the delay was interrupted by pressing ESC
      if (result == SCANCODE_ESC)
      {
        scriptPageIndex = 0xFF;
        return;
      }
    }
    else if (StringStartsWith("//Z", text))
    {
      SetUpParameterRead(&text, &originalStringEnd);
      CopyStringUppercased(text, paramBuffer);

      // Enable the menu cursor, and/or set its position
      uiMenuCursorPos = atoi(paramBuffer);

      UnterminateStr(text, originalStringEnd);
    }
    else if (StringStartsWith("//MENU", text))
    {
      SetUpParameterRead(&text, &originalStringEnd);
      CopyStringUppercased(text, paramBuffer);

      // Read the menu ID
      uiCurrentMenuId = atoi(paramBuffer);

      // Restore a previously set menu selection for the menu ID
      scriptPageIndex = uiMenuSelectionStates[uiCurrentMenuId];
      uiMenuState = 1;

      UnterminateStr(text, originalStringEnd);

      // Skip forward to the script page corresponding to the restored menu
      // selection
      for (i = 1; i < scriptPageIndex; i++)
      {
        text += FindTokenForwards("//APAGE", text);
      }
    }
    else if (StringStartsWith("//TOGGS", text))
    {
      hasCheckboxes = true;

      // Read X position
      SetUpParameterRead(&text, &originalStringEnd);
      CopyStringUppercased(text, paramBuffer);

      checkboxesX = atoi(paramBuffer);

      UnterminateStr(text, originalStringEnd);

      // Read number of checkboxes
      SetUpParameterRead(&text, &originalStringEnd);
      CopyStringUppercased(text, paramBuffer);

      numCheckboxes = atoi(paramBuffer);

      UnterminateStr(text, originalStringEnd);

      // Read checkbox specifications
      //
      // [UNSAFE] No range checking - relies on the count specified in the
      // script code to be correct.
      for (i = 0; i < numCheckboxes * 2; i += 2)
      {
        // Read checkbox Y position
        SetUpParameterRead(&text, &originalStringEnd);
        CopyStringUppercased(text, paramBuffer);

        checkboxData[i] = atoi(paramBuffer);

        UnterminateStr(text, originalStringEnd);

        // Read option ID char
        SetUpParameterRead(&text, &originalStringEnd);

        checkboxData[i + 1] = *text;

        UnterminateStr(text, originalStringEnd);
      }

      // Terminate list of checkbox specifications
      checkboxData[i] = -1;

      // Draw the checkboxes
      DrawCheckboxes(checkboxesX, checkboxData);
    }
    else if (StringStartsWith("//CENTERWINDOW", text))
    {
      // Read 3 numerical parameters: Y, width, height
      for (i = 0; i < 3; i++)
      {
        SetUpParameterRead(&text, &originalStringEnd);
        CopyStringUppercased(text, paramBuffer);

        numericParams[i] = atoi(paramBuffer);

        UnterminateStr(text, originalStringEnd);
      }

      msgBoxTextY = numericParams[0] + 1;

      // Animate sliding in of a message box frame of the specified size
      UnfoldMessageBoxFrame(
        numericParams[0], numericParams[1], numericParams[2]);
    }
    else if (StringStartsWith("//SKLINE", text))
    {
      // Skip one line of text in the message box
      msgBoxTextY++;
    }
    else if (StringStartsWith("//EXITTODEMO", text))
    {
      // Enable the timer. The timer is implemented in AwaitInput(), which is
      // used by the `WAIT` command.
      uiDemoTimeoutTime = 1;
    }
    else if (StringStartsWith("//SETCURRENTPAGE", text))
    {
      SetDrawPage(gfxCurrentDisplayPage);
      uiDisplayPageChanged = true;
    }
    else if (StringStartsWith("//KEYS", text))
    {
      DrawKeyBindings();
    }
    else if (StringStartsWith("//GETNAMES", text))
    {
      SetUpParameterRead(&text, &originalStringEnd);
      CopyStringUppercased(text, paramBuffer);

      // The parameter is the index of the selected slot
      DrawSaveSlotNames(atoi(paramBuffer));

      UnterminateStr(text, originalStringEnd);
    }
    else if (StringStartsWith("//CWTEXT", text))
    {
      // For the CWTEXT command, the rest of the line after the command itself
      // is what we use as the text to draw
      text += FindNextToken(text);
      originalStringEnd = TerminateStrAtEOL(text);

      // Draw the text centered. There's no checking here that the text
      // actually first into the message box frame, it will overflow if not.
      DrawText(
        (SCREEN_WIDTH_TILES / 2) - DN2_strlen(text) / 2 - uiMessageBoxShift,
        msgBoxTextY,
        text);
      msgBoxTextY++;

      UnterminateStr(text, originalStringEnd);
    }
    else if (StringStartsWith("//XYTEXT", text))
    {
      // Read two numerical parameters: x, y
      for (i = 0; i < 2; i++)
      {
        text += FindNextToken(text);
        originalStringEnd = TerminateStrAfterToken(text);
        CopyStringUppercased(text, paramBuffer);

        numericParams[i] = atoi(paramBuffer);

        UnterminateStr(text, originalStringEnd);
      }

      // The rest of the current line after the two x/y parameters is used
      // unchanged
      text += FindNextToken(text);
      originalStringEnd = TerminateStrAtEOL(text);

      // See the DrawText function for the different features supported here.
      // This can also draw sprites in addition to text.
      DrawText(numericParams[0] - uiMessageBoxShift, numericParams[1], text);

      UnterminateStr(text, originalStringEnd);
    }
    else if (StringStartsWith("//HELPTEXT", text))
    {
      // Read two numerical parameters: episode, level
      for (i = 0; i < 2; i++)
      {
        text += FindNextToken(text);
        originalStringEnd = TerminateStrAfterToken(text);
        CopyStringUppercased(text, paramBuffer);

        numericParams[i] = atoi(paramBuffer);

        UnterminateStr(text, originalStringEnd);
      }

      // Rest of the current line is the text to show.
      text += FindNextToken(text);
      originalStringEnd = TerminateStrAtEOL(text);

      if (
        gmCurrentLevel == numericParams[1] - 1 &&
        gmCurrentEpisode == numericParams[0] - 1)
      {
        // We need to uppercase the text, since ShowInGameMessage() uses a font
        // which only has uppercase letters, and doesn't uppercase the text
        // by itself while printing.
        //
        // [UNSAFE] No range checking, text might be longer than
        // uiHintMessageBuffer.
        CopyStringUppercased(text, uiHintMessageBuffer);
        ShowInGameMessage(uiHintMessageBuffer);
      }

      UnterminateStr(text, originalStringEnd);
    }
    // WAITCURSOREND has an identical prefix to WAIT, so we need to check for
    // the former first.
    else if (StringStartsWith("//WAITCURSOREND", text) ||
             StringStartsWith("//WAIT", text))
    {
      register word offsetToNextToken;
      word offset;

      // [NOTE] I'm not sure if the `WAITCURSOREND` command is fully
      // implemented - it's not used in any of the game's script files.

      if (StringStartsWith("//WAITCURSOREND", text))
      {
        i = 1;
      }
      else
      {
        i = 0;

        // Automatically do a fade-in on the first WAIT command, if there
        // was a MENU command before.
        if (uiMenuState == 1)
        {
          uiMenuState = 2;
          FadeInScreen();
        }
      }

      // Keep running the menu or paged content navigation until an entry is
      // activated, or the user aborts by pressing ESC.
      for (;;)
      {
        if (i == 0)
        {
          // Reset demo timeout timer after each interaction
          if (uiDemoTimeoutTime)
          {
            uiDemoTimeoutTime = 1;
          }

          // Wait for input, this keeps animating and drawing the menu cursor
          // if it's enabled
          scancode = AwaitInput();
        }
        else
        {
          // 18 is SCREEN_WIDTH_TILES - 2, but using that here would change the
          // generated ASM code
          scancode = GetTextInput(
            numericParams[2] / 2 + 18 - uiMessageBoxShift,
            numericParams[0] + numericParams[1] - 2);
        }

        // Wait for roughly 53 ms
        WaitTicks(15);

        // If there was a SETKEYS command before beginning the WAIT, check if
        // one of the specified keys was pressed
        if (numKeyCodes)
        {
          register int j;

          for (j = 0; j < numKeyCodes; j++)
          {
            if (scancode == keyCodes[j])
            {
              // Key was pressed, treat it as if the corresponding menu index
              // was activated
              scriptPageIndex = j + 1;
              return;
            }
          }
        }

        // If we're not in a menu or paged content, continue script execution
        if (!scriptPageIndex)
        {
          goto nextCommand;
        }

        // Abort script execution if ESC was pressed
        if (scancode == SCANCODE_ESC)
        {
          if (uiMenuState)
          {
            // Persist current menu selection so it can be restored later
            // (see handling of MENU command above)
            uiMenuSelectionStates[uiCurrentMenuId] = scriptPageIndex;
            uiMenuState = 0;
          }

          scriptPageIndex = 0xFF;
          return;
        }

        // Never true due to the similar check above
        if (scriptPageIndex == 0)
        {
          goto nextCommand;
        }

        switch (scancode)
        {
          case SCANCODE_ENTER:
          case SCANCODE_SPACE:
            if (pagingOnly)
            {
              // For paged content without menu functionality, enter & space
              // act like down arrow/right arrow. The WAIT command is typically
              // the last command on each page, so the next command right after
              // is the start of the next page.
              //
              // When already on the last page however, this will end script
              // execution, since the next command after PAGESEND will usually
              // be END. This is different from using the down or right arrow
              // key, which will navigate back to the first page.
              goto nextCommand;
            }

            // Toggle checkbox if there are some and one is selected
            if (hasCheckboxes && scriptPageIndex <= numCheckboxes)
            {
              ToggleCheckbox(scriptPageIndex, checkboxData);
              DrawCheckboxes(checkboxesX, checkboxData);

              if (!pagingOnly)
              {
                PlaySound(SND_MENU_TOGGLE);
              }
              break;
            }
            else
            {
              // A menu entry was activated, abort script execution.
              // The calling code looks at scriptPageIndex to figure out which
              // entry was activated.
              return;
            }

          case 0xDF: // timed out to demo
            // 8 entries is the maximum that fits on screen with the design of
            // the menus, so we use 9 to indicate the timeout. But this seems a
            // little too brittle. A value like 0xFE or perhaps a dedicated
            // flag variable would seem better.
            scriptPageIndex = 9;
            return;

          case SCANCODE_UP:
          case SCANCODE_PGUP:
          case SCANCODE_LEFT:
            if (!pagingOnly)
            {
              PlaySound(SND_MENU_SELECT);
            }

            WaitTicks(2);

            if (scriptPageIndex == 1) // at top of menu or 1st page?
            {
              // Search for the end of the last page.
              //
              // This also increments scriptPageIndex for each 'APAGE' found,
              // effectively setting the index to the last page along the way.
              // See FindTokenForwards() in script1.c.
              text += FindTokenForwards("//PAGESEND", text);

              // Navigate to beginning of last page
              text -= FindTokenBackwards("//APAGE", text);
            }
            else // we can still go up
            {
              scriptPageIndex--;

              // Find beginning of current page
              text -= FindTokenBackwards("//APAGE", text);

              if (scriptPageIndex > 1)
              {
                // Find beginning of previous page
                text -= FindTokenBackwards("//APAGE", text);
              }
              else
              {
                // Find beginning of first page
                text -= FindTokenBackwards("//PAGESSTART", text);
              }
            }

            // Start executing content of the newly selected page
            goto nextCommand;

          case SCANCODE_RIGHT:
          case SCANCODE_DOWN:
          case SCANCODE_PGDOWN:
            offsetToNextToken = 0;

            if (!pagingOnly)
            {
              PlaySound(SND_MENU_SELECT);
            }

            WaitTicks(2);

            // In order to navigate to the next page, we only need to skip to
            // the next script command: The WAIT command is typically the last
            // command on each page, so the next command right after is the
            // start of the next page. But in case the last page is already
            // selected, we want to jump back to the first page. In order to
            // achieve this, we need to figure out if the next command is
            // PAGESEND. But there could be whitespace preceding the next
            // command. The following code is an attempt to handle this, but
            // it's not correct and thus only works if there's no blank line
            // between WAIT and PAGESEND - which is the case for all script
            // files coming with the game.

            // First, skip ahead to the next token. If there's a blank line
            // following the current token, this will set offsetToNextToken to
            // the distance to the beginning of that blank line, otherwise
            // to the beginning of the next token.
            offsetToNextToken += FindNextToken(text + offsetToNextToken);

            // [BUG] Now we attempt to skip over any blank lines, but this
            // doesn't work.
            //
            // Line breaks are encoded as "\r\n" in DOS.
            // If `text + offsetToNextToken` points at the next token (i.e.
            // there was no blank line after the current token), then the
            // FindNextToken() call here will return the offset to the token
            // after the next one, which will always be at least 4.
            // If it points at the start of a blank line, the call will return
            // 2.  This means that this loop will never run more than once.
            // Because of that, `offsetToNextToken - offset` after this loop is
            // always equivalent to `offsetToNextToken` before this loop.  So
            // it effectively does nothing, since the code below always uses
            // `offsetToNextToken - offset`.
            //
            // For this code to work correctly, the condition should be `offset
            // <= 2`, and the code below should use `offsetToNextToken` without
            // subtracting `offset`.
            //
            // A much easier way however would be to simply do something like:
            //
            //   while (*(text + offsetToNextToken) != '/') offsetToNextToken++;
            //
            // Since commands always start with "//" and we want to find the
            // start of the next command after the current one.
            do
            {
              offset = FindNextToken(text + offsetToNextToken);
              offsetToNextToken += offset;
            }
            while (offset < 2);

            // At bottom of menu or last page?
            if (StringStartsWith(
              "//PAGESEND", text + offsetToNextToken - offset))
            {
              // Navigate to start of first page
              text -= FindTokenBackwards("//PAGESSTART", text);
              scriptPageIndex = 1;

              // Start executing content of the newly selected page
              goto nextCommand;
            }
            else
            {
              // Navigate down - see above
              scriptPageIndex++;

              // Start executing content of the newly selected page
              goto nextCommand;
            }
        }
      }

      // This does nothing, but is required to produce assembly matching the
      // original
      while (false) {};
    }
    else if (StringStartsWith("//END", text))
    {
      // End of script, stop execution
      return;
    }
    else if (StringStartsWith("//PAGESSTART", text))
    {
      // Enable the paging functionality in the WAIT command
      scriptPageIndex = 1;
    }

    // If none of the if statements matched, the current script string was
    // either an unrecognized command, or a blank line. We simply go to the
    // next command in that case.

nextCommand:
    // [PERF] If there's a blank line after the current command, this will put
    // us at the start of that blank line. On the next loop iteration, we will
    // then try to dispatch that blank line as a command, but we won't
    // recognize it, so we continue on to the next token after that. This might
    // again be a blank line, in which case this all keeps going until we
    // arrive at the start of the next command.
    //
    // Again, a much simpler and also more efficient solution would be to
    // advance `text` until we find a `/`.
    offset = FindNextToken(text);

    // [BUG] This condition is never true, FindNextToken doesn't ever return 0.
    // As a consequence of this, malformed script files can easily produce an
    // infinite loop in this function.
    if (offset == 0)
    {
      return;
    }
    else
    {
      text += offset;
      continue; // [NOTE] Redundant
    }

    return; // [NOTE] Unreachable
  }
}


/** Return pointer to start of script
 *
 * Script files contain multiple named scripts. This function navigates to the
 * first command of the script with the given name.
 */
char far* FindScriptByName(char far* scriptName, char far* text)
{
  // [PERF] What we actually want to achieve with this function is to find
  // a line starting with the scriptName. The easiest way to do that would be
  // something like the following (range checking omitted):
  //
  //   while (!StringStartsWith(scriptName, text) // while not found...
  //   {
  //      // skip to end of current line
  //     while (*text != '\n') text++;
  //
  //     // then skip to start of next line
  //     text++;
  //   }
  //
  // Instead, the code here navigates token by token, which means that it can
  // take several string comparison function calls to skip a single line.

  for (;;)
  {
    if (StringStartsWith(scriptName, text))
    {
      // We've found the script. Advance to the next token, so that the script
      // interpreter will see the script's first command when we pass the return
      // value of this function to InterpretScript() (see ShowScriptedUI() in
      // main.c).
      //
      // [PERF] All the script files shipping with the game have a blank line
      // between the script name and the first command, which means that this
      // FindNextToken call advances to the start of the blank line, not the
      // first command. The script interpreter then spends some time skipping
      // to the actual first command. Instead of FindNextToken(), it would be
      // better to simply advance `text` until we find a '/':
      //
      //   while (*text != '/') text++;
      //
      // This could've been abstracted with a function FindNextCommand,
      // which could also have been used in InterpretScript().
      return text + FindNextToken(text);
    }

    // [UNSAFE] No checking if we've reached the end of the string. If the
    // specified script doesn't exist, this loop never terminates.
    text += FindNextToken(text);

    continue; // [NOTE] Redundant
  }
}
