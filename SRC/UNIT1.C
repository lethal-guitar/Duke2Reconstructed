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

Translation unit 1

Includes various other files as a Unity build, and contains a few random
functions that don't really fit anywhere else.

*******************************************************************************/


#include "actors.h"
#include "basicsnd.h"
#include "common.h"
#include "digisnd.h"
#include "gfx.h"
#include "lvlhead.h"
#include "scancode.h"
#include "sounds.h"
#include "vars.h"

#include <alloc.h>
#include <ctype.h>
#include <dos.h>
#include <fcntl.h>
#include <io.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>


// [NOTE] Far variables are aligned to 16-byte addresses. This means the actual
// declared size in the original code could be any value between 8289 and 8304.
// Only 8188 bytes are actually needed though, so it's not quite clear how
// the authors arrived at the declared size value.
byte far mapExtraData[8304];

byte jsButtonsSwapped = false;

char KEY_NAMES[][6] = {
  "NULL", "ESC", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=",
  "BKSP", "TAB", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", " ", " ",
  "ENTR", "CTRL", "A", "S", "D", "F", "G", "H", "J", "K", "L", ";", "\"",
  "TILDE", "LSHFT", " ", "Z", "X", "C", "V", "B", "N", "M", ",", ".", "?",
  "RSHFT", "PRTSC", "ALT", "SPACE", "CAPLK", "F1", "F2", "F3", "F4", "F5",
  "F6", "F7", "F8", "F9", "F10", "NUMLK", "SCRLK", "HOME", "Up", "PGUP", "-",
  "Left", "5", "Right", "+", "END", "Down", "PGDN", "INS", "DEL", "SYSRQ", "",
  "", "F11", "F12", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "MACRO"
};


#include "coreutil.c"


word uiMenuCursorAnimStep = 0;
word uiTextEntryCursorAnimStep = 0;
byte uiTextEntryLastScancode = 0xFF;

byte gfxCurrentPalette[] = {
  0x0,0x0,0x0,0x10,0x10,0x10,0x20,0x20,0x20,0x30,0x30,
  0x30,0x20,0x0,0x0,0x30,0x0,0x0,0x40,0x20,0x10,0x40,0x40,
  0x0,0x0,0x10,0x0,0x0,0x0,0x20,0x0,0x0,0x30,0x0,0x0,0x40,0x0,0x20,
  0x0,0x0,0x30,0x0,0x20,0x10,0x0,0x40,0x40,0x40
};

byte INGAME_PALETTE[] = {
  0x0,0x0,0x0,0x10,0x10,0x10,0x20,0x20,0x20,0x30,0x30,
  0x30,0x20,0x0,0x0,0x30,0x0,0x0,0x40,0x1C,0x10,0x40,0x40,
  0x0,0x0,0x10,0x0,0x0,0x0,0x20,0x0,0x0,0x30,0x0,0x0,0x40,0x0,0x20,
  0x0,0x0,0x30,0x0,0x20,0x10,0x0,0x40,0x40,0x40
};

byte DUKE3D_TEASER_PALETTE[] = {
  0x0,0x0,0x0,0x1A,0x0,0x0,0x1D,0x0,0x0,0x1F,0x0,0x0,0x22,0x0,0x0,
  0x25,0x0,0x0,0x28,0x0,0x0,0x2B,0x0,0x0,0x2E,0x0,0x0,0x32,0x0,
  0x0,0x34,0x0,0x0,0x37,0x0,0x0,0x3A,0x0,0x0,0x3D,0x0,0x0,0x40,
  0x0,0x0,0x39,0x0,0x0
};

word hudLowHealthAnimStep = 0;


void pascal HUD_DrawInventory(void);


#include "draw1.c"
#include "files1.c"


/** Copy a tileset to video ram for use with BlitSolidTile()
 *
 * This enables quick drawing of tiles via the latch copy technique.
 * See gfx.asm.
 */
void pascal UploadTileset(byte far* data, word size, word destOffset)
{
  register int i;
  register word mask;

  byte far* dest = MK_FP(0xa000, destOffset);

  EGA_SET_DEFAULT_MODE();
  EGA_SET_DEFAULT_BITMASK();

  for (i = 0; i < size; i++)
  {
    for (mask = 0x0100; mask < 0x1000; mask <<= 1)
    {
      // We need to disable interrupts here, since the progress bar update code
      // in TimerInterruptHandler() also modifies the EGA registers.
      disable();

      DN2_outport(0x03c4, mask | 0x02);

      *(dest + i) = *data++;

      enable();
    }
  }
}


#include "script1.c"
#include "draw3.c"
#include "ui1.c"
#include "hud1.c"


/** Shift image on screen to the left using EGA/VGA hardware panning
 *
 * `amount` must be between 0 and 7 (inclusive).
 * Used to implement screen shake effects.
 */
void SetScreenShift(byte amount)
{
  // [NOTE] Seems unnecessary?
  asm mov si, ax

  // The attribute controller uses a single I/O port for both address and data,
  // with the address being written first followed by the data. But there's
  // no way to tell if it's currently expecting an address or data - in theory,
  // a write to the port might have happened in the past, which was interpreted
  // as an address, but wasn't followed up by a data write so the graphics card
  // might still be waiting for data on the port. By first reading the status
  // register at 0x3da, the attribute controller is forced into address mode,
  // discarding whatever address might have been written in the past.
  // So this read is just here to put the hardware into a known state.
  asm mov dx, 0x3da
  asm in  al, dx

  // Write address of the "Pel Panning" register to the attribute controller.
  // This causes the next write to port 0x3c0 to set the register's value.
  // The Pel Pan register is at 0x13, but we also set bit 5 (0x20), so the
  // value written is 0x13 | 0x20 = 0x33. Mike Abrash explains why in his
  // Graphics Programming Black Book, chapter 23:
  //
  // "There is one annoying quirk about programming the AC. When the AC Index
  // register is set, only the lower five bits are used as the internal index.
  // The next most significant bit, bit 5, controls the source of the video data
  // sent to the monitor by the VGA. When bit 5 is set to 1, the output of the
  // palette RAM, derived from display memory, controls the displayed pixels;
  // this is normal operation. When bit 5 is 0, video data does not come from
  // the palette RAM, and the screen becomes a solid color. The only time bit 5
  // of the AC Index register should be 0 is during the setting of a palette
  // RAM register [...]".
  asm mov al, 0x33
  asm mov dx, 0x3c0
  asm out dx, al

  // Write the data for the Pel Pan register to the attribute controller.
  // This makes the graphics card apply a horizontal shift to the output
  // image.
  asm mov al, [amount]
  asm out dx, al

  // [NOTE] Seems unnecessary?
  asm mov ax, si
}


#include "lvlutil1.c"
#include "ui2.c"


/** Add to the player's score and update score display in HUD */
void pascal GiveScore(word score)
{
  plScore += score;

  // Update the HUD
  SetDrawPage(gfxCurrentDisplayPage);
  DrawBigNumberBlue(15, 22, plScore);

  SetDrawPage(!gfxCurrentDisplayPage);
  DrawBigNumberBlue(15, 22, plScore);
}


/** Helper function for the scripting system
 *
 * Advances the string pointed to by pText to the beginning of the next token,
 * and puts a zero terminator after the end of that token. The original
 * character (which was replaced by the zero terminator) is written to
 * the value pointed to by outOriginalEnd.
 *
 * The result of this is that *pText is now a string containing just one
 * token, and can be processed as necessary. Afterwards, the zero terminator
 * should be replaced again with the original character. 
 */
void pascal SetUpParameterRead(char far** pText, char* outOriginalEnd)
{
  *pText += FindNextToken(*pText);
  *outOriginalEnd = TerminateStrAfterToken(*pText);
}


/** Keyboard interrupt service routine
 *
 * This function is invoked whenever a key is pressed or released.
 * It updates kbKeyState and kbLastScancode to allow the game to react
 * to keyboard input.
 */
void interrupt KeyboardHandler(void)
{
  // Retrieve last key event from the keyboard controller.
  // The value is a combination of the scancode which identifies the key
  // (see scancode.h), and a flag bit (the most significant bit) indicating
  // if the key was pressed or released. A set bit indicates a key release,
  // while an unset bit means the key was pressed.
  //
  // For an in-depth explanation of IBM PC keyboard handling, see
  // https://cosmodoc.org/topics/keyboard-functions
  kbLastScancode = DN2_inportb(0x60);

  // Ignore extended multi-byte scancodes (introduced with the IBM PS/2 line).
  if (kbLastScancode != SCANCODE_EXTENDED)
  {
    // These two port writes are only needed on PC and PC XT systems, which
    // aren't supported by the game.
    DN2_outportb(0x61, DN2_inportb(0x61) | 0x80);
    DN2_outportb(0x61, DN2_inportb(0x61) & ~0x80);

    // Update our key state array
    if ((kbLastScancode & 0x80) != 0)
    {
      kbKeyState[kbLastScancode & 0x7f] = false;
    }
    else
    {
      kbKeyState[kbLastScancode & 0x7f] = true;
    }
  }

  // Acknowledge interrupt to interrupt controller
  DN2_outportb(0x20, 0x20);
}


/** Draw vertical strip of checkboxes
 *
 * See InterpretScript() in script2.c.
 */
void pascal DrawCheckboxes(byte x, byte* checkboxData)
{
  int i;

  for (i = 0; checkboxData[i] != 0xFF; i += 2)
  {
    if (QueryOrToggleOption(false, checkboxData[i + 1]))
    {
      // Checked
      DrawStatusIcon_2x2(XY_TO_OFFSET(22, 7), x - 1, checkboxData[i] - 1);
    }
    else
    {
      // Unchecked
      DrawStatusIcon_2x2(XY_TO_OFFSET(20, 7), x - 1, checkboxData[i] - 1);
    }
  }
}


/** Toggle a checkbox
 *
 * See InterpretScript() in script2.c.
 */
void pascal ToggleCheckbox(byte index, byte* checkboxData)
{
  index--;
  QueryOrToggleOption(true, checkboxData[index * 2 + 1]);
}


/** Move a section of tiles down by `distance` units
 *
 * The tiles in the original location are erased, tiles in the new location
 * are overwritten.
 * The total number of tiles contained in the specified section mustn't
 * exceed 3000.
 */
void pascal Map_MoveSection(
  word left,
  word top,
  word right,
  word bottom,
  word distance)
{
  int x;
  int y;
  word colOffset;
  word rowOffset;

  // Copy tiles from source area into temporary buffer, and erase source area
  rowOffset = 0;
  for (y = top; y <= bottom; y++)
  {
    colOffset = 0;
    for (x = left; x <= right; x++)
    {
      *(tempTileBuffer + rowOffset + colOffset) = Map_GetTile(x, y);
      Map_SetTile(0, x, y);

      colOffset++;
    }

    rowOffset += right - left + 1;
  }

  // Now use content of temp buffer to set tiles in destination area
  rowOffset = 0;
  top += distance;
  bottom += distance;

  for (y = top; y <= bottom; y++)
  {
    colOffset = 0;
    for (x = left; x <= right; x++)
    {
      Map_SetTile(*(tempTileBuffer + rowOffset + colOffset), x, y);

      colOffset++;
    }

    rowOffset += right - left + 1;
  }

  // [PERF] It would be fairly easy to redesign this function so that it
  // doesn't need a temporary buffer. That would save 3000 bytes of memory.
  //
  // The following code is all that's needed, insted of the two loops:
  //
  //   for (y = bottom; y >= top; y--)
  //   {
  //     for (x = left; x <= right; x++)
  //     {
  //       Map_SetTile(Map_GetTile(x, y), x, y + distance);
  //       Map_SetTile(0, x, y);
  //     }
  //   }
}


#include "hud2.c"
#include "hiscore.c"


/** Read names of saved games from disk
 *
 * The game's menus present saved games as a list of 8 named entries. But the
 * names of the saved games aren't stored in the saved game files themselves.
 * Instead, a separate file is used to store only the names of all saved
 * games.
 *
 * This function reads these names from the file into the global variable
 * saveSlotNames.
 */
void ReadSaveSlotNames(void)
{
  int i;
  int fd = OpenFileRW("NUKEM2.-NM");

  if (fd != -1)
  {
    for (i = 0; i < NUM_SAVE_SLOTS; i++)
    {
      _read(fd, saveSlotNames[i], 18);
    }

    CloseFile(fd);
  }
}


/** Write names of saved games to disk
 *
 * Persists current content of the global variable saveSlotNames.
 * See ReadSaveSlotNames().
 */
void WriteSaveSlotNames(void)
{
  int i;
  int fd = OpenFileW("NUKEM2.-NM");

  for (i = 0; i < NUM_SAVE_SLOTS; i++)
  {
    _write(fd, saveSlotNames[i], 18);
  }

  CloseFile(fd);
}


#include "files2.c"
#include "memory.c"


/** Load 16-color fullscreen image and prepare palette
 *
 * Loads the image specified by the filename, writes it into the framebuffer,
 * and prepares the palette.  To actually make the image appear with the
 * correct palette, a FadeInScreen() call is needed.  Consequently, the screen
 * should be faded out before calling this function.
 *
 * Always loads the image onto display page 0, and makes 0 the active (visible)
 * page.
 */
void pascal DrawFullscreenImage(char far* filename)
{
  int i;
  int j;
  int fd;
  word bytesRead;
  word mask = 0x100;
  byte far* vram = MK_FP(0xa000, 0);
  byte far* data = MM_PushChunk(2000, CT_TEMPORARY);

  OpenAssetFile(filename, &fd);

  EGA_SET_DEFAULT_MODE();
  EGA_SET_DEFAULT_BITMASK();

  SetDrawPage(0);

  for (j = 0; (unsigned)j < 32000; j += 8000)
  {
    // Loading the image in 2000-byte chunks makes this a bit more complicated
    // than one might expect, but this was likely done to reduce the amount of
    // memory needed. This function is also used for menus while in-game, so the
    // game itself is already using a lot of the available memory.  Allocating
    // the entire 8000 bytes at once might be too much and cause a crash.
    //
    // [NOTE] It seems this complexity could've been avoided entirely by
    // reading straight into video memory, which would require 0 additional
    // buffer space. Not sure why that's not done here.
    _dos_read(fd, data, 2000, &bytesRead);
    DN2_outport(0x3c4, 2 | mask);
    for (i = 0; (unsigned)i < 2000; i++)
    {
      vram[i] = data[i];
    }

    _dos_read(fd, data, 2000, &bytesRead);
    for (i = 0; (unsigned)i < 2000; i++)
    {
      vram[i + 2000] = data[i];
    }

    _dos_read(fd, data, 2000, &bytesRead);
    DN2_outport(0x3c4, 2 | mask); // [NOTE] Redundant
    for (i = 0; (unsigned)i < 2000; i++)
    {
      vram[i + 4000] = data[i];
    }

    _dos_read(fd, data, 2000, &bytesRead);
    for (i = 0; (unsigned)i < 2000; i++)
    {
      vram[i + 6000] = data[i];
    }

    mask <<= 1;
  }

  _dos_read(fd, gfxCurrentPalette, 16 * 3, &bytesRead);
  CloseFile(fd);

  SetDisplayPage(0);

  MM_PopChunk(CT_TEMPORARY);
}


#include "particls.c"
#include "lvlutil2.c"
#include "bonusscr.c"


/** Concatenate prefix, number, and postfix into a single string */
char far* MakeFilename(char far* prefix, byte number, char far* postfix)
{
  word position;
  char numberString[4];

  CopyStringUppercased(prefix, tempFilename);
  position = DN2_strlen(prefix);

  ultoa(number, numberString, 10);

  // [NOTE] The cast below is unnecessary
  CopyStringUppercased(numberString, ((char far*)tempFilename + position));
  position = position + DN2_strlen(numberString);

  CopyStringUppercased(postfix, tempFilename + position);

  return tempFilename;
}


/** Load and display specified text-mode screen */
void ShowTextScreen(const char far* name)
{
  // The files containing text mode screens are in a
  // format which is ready to be copied into
  // text mode video memory, so this is pretty simple.
  // See https://moddingwiki.shikadi.net/wiki/B800_Text
  void far* vram = MK_FP(0xB800, 0);
  LoadAssetFile(name, vram);

  // [NOTE] All call sites of this function are followed
  // by a call to MoveCursorToBottom(), since directly
  // writing to video memory doesn't inform DOS about
  // the change in screen content. It would've been handy
  // to simply do that within this function, not sure why
  // that wasn't done.
}


#include "video1.c"


/** Load and display specified 256-color image and wait for input.
 *
 * Only used to show the anti-piracy message (LCR.MNI) in the registered
 * version of the game.
 */
void ShowVGAScreen(const char far* filename)
{
  register int i;
  register int pi;
  void far* vram = MK_FP(0xA000, 0);
  byte palette[256 * 3];

  // Load palette from the file
  LoadAssetFilePart(filename, 0, palette, sizeof(palette));

  // Zero out the current VGA palette. This blanks the screen, and prevents a
  // partial image from being briefly visible while we load it into video
  // memory.
  for (i = 0; i < 256; i++)
  {
    SetPaletteEntry_256(i, 0, 0, 0);
  }

  pi = 0;

  // Load the image data directly into video memory
  LoadAssetFilePart(filename, sizeof(palette), vram, 320 * 200);

  // Set the palette. This makes the already loaded image appear on screen.
  for (i = 0; i < 256; i++)
  {
    SetPaletteEntry_256(i, palette[pi++], palette[pi++], palette[pi++]);
  }

  AwaitInput();
}


#include "video2.c"
