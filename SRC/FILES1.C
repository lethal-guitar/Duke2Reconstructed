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

File system code, part 1

This file provides a few filesystem-related helper functions, and part of the
group file system.

The game stores almost all of its assets (graphics, sound effects, levels, etc.)
in a single group file, `NUKEM2.CMP`. The group file simply stores files without
compression. On startup, the game reads the group file's header into memory,
which contains the offsets to each individual file.

*******************************************************************************/


/** Open a file for reading
 *
 * Technically, the file can also be written to, but the game only ever reads
 * from files opened via this function.
 */
int pascal OpenFileRW(char* name)
{
  return open(name, O_RDWR | O_BINARY);
}


/** Open a file for writing */
int pascal OpenFileW(char* name)
{
  return open(name, O_WRONLY | O_CREAT | O_BINARY, S_IWRITE);
}


/** Close a file handle */
void pascal CloseFile(int fd)
{
  close(fd);
}


#include "draw2.c"


/** Open a file handle for an asset file with the given name
 *
 * The resulting file handle is written to the pOutFd parameter, the size of
 * the file is returned. If there's an error or the file can't be found, the
 * program is terminated.
 * The file handle should be closed using CloseFile().
 *
 * Before using this function, the group file dictionary must be loaded into
 * memory using LoadGroupFileDict().
 *
 * This function looks for the requested file in the game's directory first,
 * and then in the group file if it doesn't find it. This makes it possible
 * to override entries from the group file by putting a replacement file with
 * the same name into the game directory.
 */
dword pascal OpenAssetFile(const char far* name, int* pOutFd)
{
  char uppercaseName[14];
  register int dictOffset;
  dword offset;
  dword size;

  CopyStringUppercased(name, uppercaseName);

  // First, see if the file exists in the game directory
  *pOutFd = OpenFileRW(uppercaseName);

  // If it does, get its size and return the file handle.
  if (*pOutFd != -1)
  {
    return filelength(*pOutFd);
  }

  // Otherwise, try to load it from the group file
  *pOutFd = OpenFileRW("NUKEM2.CMP");
  if (*pOutFd == -1)
  {
    goto error;
  }

  // Look for an entry in the dictionary with a matching filename
  for (dictOffset = 0; dictOffset < sizeof(fsGroupFileDict); dictOffset += 20)
  {
    // A zero entry indicates we've reached the end of the dictionary,
    // so we've failed to find the file.
    if (!fsGroupFileDict[dictOffset])
    {
      goto error;
    }

    if (StringStartsWith(uppercaseName, (char*)fsGroupFileDict + dictOffset))
    {
      // We've found a matching entry. Extract offset and file size from the
      // entry and then seek the file handle to the right place
      lseek(*pOutFd, dictOffset + 12, SEEK_SET);
      _read(*pOutFd, &offset, sizeof(dword));
      _read(*pOutFd, &size, sizeof(dword));

      lseek(*pOutFd, offset, SEEK_SET);
      return size;
    }
  }

error:
  fsNameForErrorReport = uppercaseName;
  Quit(fsNameForErrorReport);
}


/** Return size of an asset file
 *
 * Before using this function, the group file dictionary must be loaded into
 * memory using LoadGroupFileDict().
 *
 * If there's an error or the file can't be found, the program is terminated.
 */
word pascal GetAssetFileSize(const char far* name)
{
  int fd;
  register word fileSize;

  fileSize = OpenAssetFile(name, &fd);
  CloseFile(fd);
  return fileSize;
}


/** Load group file dictionary into memory
 *
 * This initializes the filesystem layer. Must be called before using any of the
 * asset file related functions can be used.
 */
void LoadGroupFileDict(void)
{
  int fd = OpenFileRW("NUKEM2.CMP");

  if (fd != -1)
  {
    // [UNSAFE] fsGroupFileDict has a fixed size. There's no checking here that
    // the file actually fits within the available space.
    _read(fd, fsGroupFileDict, sizeof(fsGroupFileDict));
    CloseFile(fd);
  }
}
