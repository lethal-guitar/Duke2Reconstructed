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

File system code, part 2

This file provides more filesystem-related helper functions, and the remainder
of the group file system (see files1.c).

*******************************************************************************/


/** Like Borland C's setw(), but for POSIX-style file handles */
void pascal WriteWord(word value, int fd)
{
  _write(fd, &value, sizeof(word));
}


/** Like Borland C's getw(), but for POSIX-style file handles */
word pascal ReadWord(int fd)
{
  word result;
  _read(fd, &result, sizeof(word));
  return result;
}


/** Load entire content of given file into buffer
 *
 * Like OpenAssetFile(), this first looks for the requested file in the game
 * directory, then in the group file.
 * If the file doesn't exist or can't be opened, the program is terminated.
 * There's no error handling beyond that though.
 *
 * [UNSAFE] No bounds checking, the caller must ensure that `buffer` is large
 * enough to hold the file's entire content.
 */
void pascal LoadAssetFile(const char far* name, void far* buffer)
{
  word bytesRead;
  int fd;
  register word fileSize;

  fileSize = OpenAssetFile(name, &fd);
  _dos_read(fd, buffer, fileSize, &bytesRead);
  CloseFile(fd);
}


/** Load part of given file into buffer
 *
 * This reads `size` bytes starting at `offset` from the file.
 * Like OpenAssetFile(), first looks for the requested file in the game
 * directory, then in the group file.
 * If the file doesn't exist or can't be opened, the program is terminated.
 * There's no error handling beyond that though.
 *
 * The caller must ensure that buffer is large enough.
 */
void pascal LoadAssetFilePart(
  const char far* name,
  dword offset,
  void far* buffer,
  word size)
{
  int fd;
  word bytesRead;

  OpenAssetFile(name, &fd);
  lseek(fd, offset, SEEK_CUR);
  _dos_read(fd, buffer, size, &bytesRead);
  CloseFile(fd);
}
