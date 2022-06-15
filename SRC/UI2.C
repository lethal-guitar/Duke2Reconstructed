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

UI code, part 2

Various functions here, including parts of the keyboard configuration and save
game menus.

*******************************************************************************/


/** Test if given scancode is already in use by another key binding
 *
 * Compares the given scancode against all key bindings except the one at index.
 */
bool pascal IsKeyBindingInUse(byte index, byte scancode)
{
  register int i;

  for (i = 1; i < 7; i++)
  {
    if (index != i)
    {
      switch (i)
      {
        case 1:
          if (kbBindingFire == scancode) { return true; }
          break;

        case 2:
          if (kbBindingJump == scancode) { return true; }
          break;

        case 3:
          if (kbBindingUp == scancode) { return true; }
          break;

        case 4:
          if (kbBindingDown == scancode) { return true; }
          break;

        case 5:
          if (kbBindingLeft == scancode) { return true; }
          break;

        case 6:
          if (kbBindingRight == scancode) { return true; }
          break;
      }
    }
  }

  return false;
}


/** Show the rebind key dialog and save the chosen key binding
 *
 * Returns once the user has successfully chosen a new key binding.
 * Notably, there's no way to cancel the dialog.
 */
void pascal RunRebindKeyDialog(byte index)
{
  byte newBinding;

retry:
  DrawText(10, 19, "Press a key to use..");

  // Erase key name display for the selected binding
  DrawText(26, index * 2 + 5, "       ");

  newBinding = GetTextInput(26, index * 2 + 5);

  if (IsKeyBindingInUse(index, newBinding))
  {
    DrawText(8, 19, "THAT KEY IS ALREADY IN USE!");
    DrawText(8, 20, "   Select another key.");

    // Just wait for any key press
    GetTextInput(30, 20);

    // Erase the error message
    DrawText(8, 19, "                           ");
    DrawText(8, 20, "                      ");
    goto retry;
  }

  // Store the new binding
  switch (index)
  {
    case 1:
      kbBindingFire = newBinding;
      break;

    case 2:
      kbBindingJump = newBinding;
      break;

    case 3:
      kbBindingUp = newBinding;
      break;

    case 4:
      kbBindingDown = newBinding;
      break;

    case 5:
      kbBindingLeft = newBinding;
      break;

    case 6:
      kbBindingRight = newBinding;
      break;
  }
}


/** Draw names of all save slots, selected index is drawn highlighted */
void pascal DrawSaveSlotNames(word selectedIndex)
{
  int i;

  for (i = 0; i < NUM_SAVE_SLOTS; i++)
  {
    if (saveSlotNames[i][0])
    {
      DrawBigText(
        13, i * 2 + 6, (char*)(saveSlotNames + i), i == selectedIndex ? 3 : 2);
    }
    else
    {
      DrawBigText(13, i * 2 + 6, "Empty", i == selectedIndex ? 3 : 2);
    }
  }
}


/** Let user enter name for saved game, or cancel
 *
 * Runs until the user either confirms by pressing enter, or cancels by
 * pressing ESC.
 * Returns true if the user confirmed, false otherwise.
 */
bool pascal RunSaveGameNameEntry(word index)
{
  if (!saveSlotNames[index][0])
  {
    // If the save slot was previously empty, erase the "Empty" shown in its
    // location
    DrawText(14, 5 + index * 2, "                  ");
    DrawText(14, 6 + index * 2, "                  ");
  }

  // See ui1.c
  return SaveGameNameEntry_Impl(
    12, 6 + index * 2, (char*)(saveSlotNames + index), SAVE_SLOT_NAME_MAX_LEN);
}


/** Draw number at position using a grey/white font
 *
 * Used for the bonus screen.
 */
void pascal DrawBigNumberGrey(word x, word y, dword num)
{
  register word len;
  register word pos = x;
  char numStr[12];

  ultoa(num, numStr, 10);
  len = DN2_strlen(numStr) * 2; // * 2

  DrawBigText(pos - len - 2, y, " ", 16);
  DrawBigText(pos - len, y, numStr, 16);
}


/** Draw number at given position in a blue font
 *
 * Used by the HUD.
 */
void pascal DrawBigNumberBlue(word x, word y, dword num)
{
  int i;
  word len;
  char numStr[12];

  ultoa(num, numStr, 10);
  len = DN2_strlen(numStr);

  for (i = len - 1; i >= 0; i--)
  {
    DrawStatusIcon_2x2(
      (*(numStr - 1 + (len - i)) - '0') * 16 + XY_TO_OFFSET(0, 7),
      x - i *2,
      y);
  }
}


/** Test if given save slot is empty */
ibool pascal IsSaveSlotEmpty(byte index)
{
  return !(saveSlotNames[index][0]);
}
