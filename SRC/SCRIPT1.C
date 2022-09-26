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

Scripting system, part 1: Text parsing helper functions

A set of string parsing helper functions used by the scripting system. Almost
none of these functions do any bounds checking, and can cause infinite loops
if given incorrect or unexpected data. On a modern OS, they would probably
crash at some point, but DOS doesn't have any memory protection and the code
will happily keep reading random memory until the pointer wraps around - which
happens after 64 kB, since these are far pointers and thus the segment part
isn't incremented (that only happens automatically when using huge pointers) -
and then keep going.


This makes the game very sensitive to modified or corrupted script files.

*******************************************************************************/


/** Return distance to beginning of next token in string
 *
 * This function assumes that there is only a single space between tokens -
 * which is the case in all of the game's script files.
 *
 * For example, with the following string:
 *
 *     "FIRST SECOND"
 *      ^
 *    str
 *
 * The result is 6.
 *
 * [UNSAFE] No bounds checking. Will read out of bounds if no space or line
 * break character is found.
 */
word pascal FindNextToken(char far* str)
{
  register word i;

  // Advance to the first space or line break and count distance
  for (i = 1; *str != ' ' && *str != '\n'; i++, str++);

  return i;
}


/** Return distance to specified token, searching backwards
 *
 * If the string already starts with the specified token, the return value is 1.
 *
 * [UNSAFE] No bounds checking. Will read out of bounds and won't terminate if
 * the token can't be found.
 */
word pascal FindTokenBackwards(char far* token, char far* str)
{
  register word i = 1;

  for (;;)
  {
    // Go back until we hit a space or linebreak
    while (*str != ' ' && *str != '\n')
    {
      i++;
      str--;
    }

    // Is the token after the current whitespace the one we want?
    if (StringStartsWith(token, str + 1))
    {
      // Done, return distance
      return i;
    }

    // Skip current whitespace
    i++;
    str--;
  }
}


/** Return distance to specified token, searching forwards
 *
 * Also has some special logic to increment the global variable
 * scriptPageIndex each time an `APAGE` token is found, in case the desired
 * token isn't `APAGE`.
 *
 * [UNSAFE] No bounds checking. Will read out of bounds and won't terminate if
 * the token can't be found.
 */
word pascal FindTokenForwards(char far* token, char far* str)
{
  register word i = 1;

  for (;;)
  {
    // Go forward until we hit a space or linebreak
    while (*str != ' ' && *str != '\n')
    {
      i++;
      str++;
    }

    // Is the token after the current whitespace the one we want?
    if (StringStartsWith(token, str + 1))
    {
      // Then we're done, return distance
      return i;
    }
    else if (StringStartsWith("//APAGE", str + 1))
    {
      // If the token isn't the desired one, but indicates the start of
      // a page, increment the current page index.
      // This is very specific to how this function is used in InterpretScript
      // (see script2.c).
      scriptPageIndex++;
    }

    // Skip current whitespace
    i++;
    str++;
  }
}


/** Put a zero terminator after the end of the current token in str
 *
 * The idea behind this function is to make str look as if it ends after the
 * current token, in order to let other string processing functions "see" only
 * the current token without needing to copy that substring.
 *
 * Should be undone with UnterminateStr() afterwards.
 *
 * [UNSAFE] No bounds checking. Will read out of bounds and won't terminate if
 * no space or line break can be found.
 */
char pascal TerminateStrAfterToken(char far* str)
{
  register char originalEnd;

  for (;; str++)
  {
    originalEnd = *str;

    // Is the current char a space or carriage return? DOS uses'\r\n'-style
    // line breaks, so this is looking for the start of a line break.
    if (*str == ' ' || *str == '\r')
    {
      // Then replace it with a zero terminator
      *str = '\0';

      return originalEnd;
    }
  }
}


/** Put a zero terminator at the end of the current line in str
 *
 * Same idea here as with TerminateStrAfterToken(), but for when an entire line
 * should be isolated, not just a single token.
 *
 * Should be undone with UnterminateStr() afterwards.
 *
 * [UNSAFE] No bounds checking. Will read out of bounds and won't terminate if
 * no line break can be found.
 */
char pascal TerminateStrAtEOL(char far* str)
{
  register char originalEnd;

  for (;; str++)
  {
    originalEnd = *str;

    // Is the current char a carriage return? DOS uses '\r\n'-style line
    // breaks, so this is looking for the start of a line break.
    if (*str == '\r')
    {
      // Then replace it with a zero terminator
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
  // Find next zero terminator
  while (*str++ != '\0');

  // str was already incremented in the loop above, so the terminator is
  // the preceding character
  *(str - 1) = newEnd;
}
