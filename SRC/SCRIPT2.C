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

TODO: Further document this file and the functions here

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


/** Called by the scripting system when a checkbox is toggled
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

#define HANDLE_DEBUG_SELECTION(n) \
  if (toggle)                     \
  {                               \
    debugSelectedFunction = n;    \
  }                               \
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
      uiMessageBoxShift = 3;
    }
    else if (StringStartsWith("//GETPAL", text))
    {
      SetUpParameterRead(&text, &originalStringEnd);

      SetDrawPage(0);
      LoadAssetFile(text, gfxCurrentPalette);
      FillScreenRegion(SFC_BLACK, 0, 0, 39, 24);

      UnterminateStr(text, originalStringEnd);
    }
    else if (StringStartsWith("//LOADRAW", text))
    {
      SetUpParameterRead(&text, &originalStringEnd);

      DrawFullscreenImage(text);

      UnterminateStr(text, originalStringEnd);
    }
    else if (StringStartsWith("//SETKEYS", text))
    {
      SetUpParameterRead(&text, &originalStringEnd);

      for (i = 0; *(text + i) != '\0'; i++)
      {
        if (*(text + i) == 0x5F)
        {
          keyCodes[i] = SCANCODE_D;
        }
        else
        {
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
      result = AwaitInputOrTimeout(atoi(paramBuffer));

      UnterminateStr(text, originalStringEnd);

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
      uiMenuCursorPos = atoi(paramBuffer);

      UnterminateStr(text, originalStringEnd);
    }
    else if (StringStartsWith("//MENU", text))
    {
      SetUpParameterRead(&text, &originalStringEnd);

      CopyStringUppercased(text, paramBuffer);
      uiCurrentMenuId = atoi(paramBuffer);

      scriptPageIndex = uiMenuSelectionStates[uiCurrentMenuId];
      uiMenuState = 1;

      UnterminateStr(text, originalStringEnd);

      for (i = 1; i < scriptPageIndex; i++)
      {
        text += FindTokenForwards("//APAGE", text);
      }
    }
    else if (StringStartsWith("//TOGGS", text))
    {
      hasCheckboxes = true;

      SetUpParameterRead(&text, &originalStringEnd);
      CopyStringUppercased(text, paramBuffer);
      checkboxesX = atoi(paramBuffer);
      UnterminateStr(text, originalStringEnd);

      SetUpParameterRead(&text, &originalStringEnd);
      CopyStringUppercased(text, paramBuffer);
      numCheckboxes = atoi(paramBuffer);
      UnterminateStr(text, originalStringEnd);

      for (i = 0; i < numCheckboxes * 2; i += 2)
      {
        SetUpParameterRead(&text, &originalStringEnd);
        CopyStringUppercased(text, paramBuffer);
        checkboxData[i] = atoi(paramBuffer);
        UnterminateStr(text, originalStringEnd);

        SetUpParameterRead(&text, &originalStringEnd);

        checkboxData[i + 1] = *text;

        UnterminateStr(text, originalStringEnd);
      }

      checkboxData[i] = -1;

      DrawCheckboxes(checkboxesX, checkboxData);
    }
    else if (StringStartsWith("//CENTERWINDOW", text))
    {
      for (i = 0; i < 3; i++)
      {
        SetUpParameterRead(&text, &originalStringEnd);
        CopyStringUppercased(text, paramBuffer);
        numericParams[i] = atoi(paramBuffer);
        UnterminateStr(text, originalStringEnd);
      }

      msgBoxTextY = numericParams[0] + 1;

      UnfoldMessageBoxFrame(numericParams[0], numericParams[1], numericParams[2]);
    }
    else if (StringStartsWith("//SKLINE", text))
    {
      msgBoxTextY++;
    }
    else if (StringStartsWith("//EXITTODEMO", text))
    {
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
      DrawSaveSlotNames(atoi(paramBuffer));

      UnterminateStr(text, originalStringEnd);
    }
    else if (StringStartsWith("//CWTEXT", text))
    {
      text += FindNextToken(text);
      originalStringEnd = TerminateStrAtEOL(text);

      DrawText(20 - DN2_strlen(text) / 2 - uiMessageBoxShift, msgBoxTextY, text);
      msgBoxTextY++;

      UnterminateStr(text, originalStringEnd);
    }
    else if (StringStartsWith("//XYTEXT", text))
    {
      for (i = 0; i < 2; i++)
      {
        text += FindNextToken(text);
        originalStringEnd = TerminateStrAfterToken(text);

        CopyStringUppercased(text, paramBuffer);
        numericParams[i] = atoi(paramBuffer);

        UnterminateStr(text, originalStringEnd);
      }

      text += FindNextToken(text);
      originalStringEnd = TerminateStrAtEOL(text);

      DrawText(numericParams[0] - uiMessageBoxShift, numericParams[1], text);

      UnterminateStr(text, originalStringEnd);
    }
    else if (StringStartsWith("//HELPTEXT", text))
    {
      for (i = 0; i < 2; i++)
      {
        text += FindNextToken(text);
        originalStringEnd = TerminateStrAfterToken(text);

        CopyStringUppercased(text, paramBuffer);
        numericParams[i] = atoi(paramBuffer);

        UnterminateStr(text, originalStringEnd);
      }

      text += FindNextToken(text);
      originalStringEnd = TerminateStrAtEOL(text);

      if (
        gmCurrentLevel == numericParams[1] - 1 &&
        gmCurrentEpisode == numericParams[0] - 1)
      {
        CopyStringUppercased(text, uiHintMessageBuffer);
        ShowInGameMessage(uiHintMessageBuffer);
      }

      UnterminateStr(text, originalStringEnd);
    }
    else if (StringStartsWith("//WAITCURSOREND", text) ||
             StringStartsWith("//WAIT", text))
    {
      register word pageOffset;
      word offset;

      if (StringStartsWith("//WAITCURSOREND", text))
      {
        i = 1;
      }
      else
      {
        i = 0;

        if (uiMenuState == 1)
        {
          uiMenuState = 2;
          FadeInScreen();
        }
      }

      for (;;)
      {
        if (i == 0)
        {
          if (uiDemoTimeoutTime)
          {
            uiDemoTimeoutTime = 1;
          }

          scancode = AwaitInput();
        }
        else
        {
          scancode = GetTextInput(
            numericParams[2] / 2 + 18 - uiMessageBoxShift,
            numericParams[0] + numericParams[1] - 2);
        }

        WaitTicks(15);

        if (numKeyCodes)
        {
          register int j;

          for (j = 0; j < numKeyCodes; j++)
          {
            if (scancode == keyCodes[j])
            {
              scriptPageIndex = j + 1;
              return;
            }
          }
        }

        if (!scriptPageIndex)
        {
          goto nextCommand;
        }

        if (scancode == SCANCODE_ESC)
        {
          if (uiMenuState)
          {
            uiMenuSelectionStates[uiCurrentMenuId] = scriptPageIndex;
            uiMenuState = 0;
          }

          scriptPageIndex = 0xFF;
          return;
        }

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
              goto nextCommand;
            }

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
              return;
            }

          case 0xDF: // timed out to demo
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

            if (scriptPageIndex == 1)
            {
              // Also increments scriptPageIndex for each 'APAGE' found,
              // effectively setting the index to the last page
              text += FindTokenForwards("//PAGESEND", text);

              text -= FindTokenBackwards("//APAGE", text);
            }
            else
            {
              scriptPageIndex--;
              text -= FindTokenBackwards("//APAGE", text);

              if (scriptPageIndex > 1)
              {
                text -= FindTokenBackwards("//APAGE", text);
              }
              else
              {
                text -= FindTokenBackwards("//PAGESSTART", text);
              }
            }

            goto nextCommand;

          case SCANCODE_RIGHT:
          case SCANCODE_DOWN:
          case SCANCODE_PGDOWN:
            pageOffset = 0;

            if (!pagingOnly)
            {
              PlaySound(SND_MENU_SELECT);
            }

            WaitTicks(2);

            pageOffset += FindNextToken(text + pageOffset);

            do
            {
              offset = FindNextToken(text + pageOffset);
              pageOffset += offset;
            }
            while (offset < 2);

            if (StringStartsWith("//PAGESEND", text + pageOffset - offset))
            {
              text -= FindTokenBackwards("//PAGESSTART", text);

              scriptPageIndex = 1;
              goto nextCommand;
            }
            else
            {
              scriptPageIndex++;
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
      return;
    }
    else if (StringStartsWith("//PAGESSTART", text))
    {
      scriptPageIndex = 1;
    }

nextCommand:
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
      continue;
    }

    return;
  }
}


char far* FindScriptByName(char far* scriptName, char far* text)
{
  for (;;)
  {
    if (StringStartsWith(scriptName, text))
    {
      return text + FindNextToken(text);
    }

    text += FindNextToken(text);
    continue;
  }
}
