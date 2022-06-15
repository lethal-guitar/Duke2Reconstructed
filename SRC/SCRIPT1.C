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

Scripting system, part1: Text parsing helper functions

TODO: Further document this file and the functions here

*******************************************************************************/


/** Return distance to beginning of next token in string
 *
 * This function assumes that there is only a single white space between
 * tokens.
 */
word pascal FindNextToken(char far* str)
{
  register word i;

  for (i = 1; *str != ' ' && *str != '\n'; i++, str++);

  return i;
}


word pascal FindTokenBackwards(char far* token, char far* str)
{
  register word i = 1;

  for (;;)
  {
    while (*str != ' ' && *str != '\n')
    {
      i++;
      str--;
    }

    if (StringStartsWith(token, str + 1))
    {
      return i;
    }

    i++;
    str--;
  }
}


word pascal FindTokenForwards(char far* token, char far* str)
{
  register word i = 1;

  for (;;)
  {
    while (*str != ' ' && *str != '\n')
    {
      i++;
      str++;
    }

    if (StringStartsWith(token, str + 1))
    {
      return i;
    }
    else if (StringStartsWith("//APAGE", str + 1))
    {
      scriptPageIndex++;
    }

    i++;
    str++;
  }
}


/** Put a zero terminator after the end of the current token in str
 *
 * The idea behind this function is to make str look as if it ends
 * after the current token, in order to let other string processing
 * functions "see" only the current token without needing to copy
 * that substring.
 *
 * Should be undone with UnterminateStr() afterwards.
 */
char pascal TerminateStrAfterToken(char far* str)
{
  register char originalEnd;

  for (;; str++)
  {
    originalEnd = *str;

    if (*str == ' ' || *str == '\r')
    {
      *str = '\0';

      return originalEnd;
    }
  }
}


/** Put a zero terminator at the end of the current line in str
 *
 * Same idea here as with TerminateStrAfterToken(), but for when
 * an entire line should be isolated, not just a single token.
 *
 * Should be undone with UnterminateStr() afterwards.
 */
char pascal TerminateStrAtEOL(char far* str)
{
  register char originalEnd;

  for (;; str++)
  {
    originalEnd = *str;

    if (*str == '\r')
    {
      *str = '\0';

      return originalEnd;
    }
  }
}


/** Replaces string's zero terminator with newEnd
 *
 * Used to undo the effect of TerminateStrAfterToken()/TerminateStrAtEOL().
 */
void pascal UnterminateStr(char far* str, char newEnd)
{
  while (*str++ != '\0');

  *(str - 1) = newEnd;
}
