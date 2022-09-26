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

TODO: Document this file and the functions here

*******************************************************************************/


int pascal CalcCompressedSize(byte far* data, int size)
{
  register int i;
  register int blockStartIndex;
  int sum;

  sum = 0;
  i = 0;

  for (;;)
  {
nextBlock:
    blockStartIndex = i;

    while (data[i] == data[i + 1])
    {
      i++;

      if (i == size - 1)
      {
        sum += 2;
        return sum;
      }

      if (i - blockStartIndex == 127)
      {
        sum += 2;
        goto nextBlock;
      }

      if (data[i] != data[i + 1])
      {
        i++;
        sum += 2;
        goto nextBlock;
      }
    }

    while (data[i] != data[i + 1])
    {
      sum++;
      i++;

      if (i == size - 1)
      {
        sum++;
        return sum;
      }

      if (i - blockStartIndex == 127)
      {
        sum++;
        goto nextBlock;
      }

      if (data[i] == data[i + 1])
      {
        sum++;
      }
    }
  }
}


void pascal DecompressRLE(byte far* src, byte far* dest)
{
  sbyte i;
  sbyte value;

  for (;;)
  {
    value = *src;
    if (!value)
    {
      return;
    }

    if (value > 0)
    {
      for (i = 0; i < value; i++)
      {
        *dest++ = *(src + 1);
      }

      src += 2;
    }
    else
    {
      src++;
      for (i = 0; i < -value; i++)
      {
        *dest++ = *src++;
      }
    }
  }
}


#include "music.c"


void LoadBackdrop(void)
{
  byte far* backdropData;
  byte far* shiftedVersion;

  backdropData = MM_PushChunk(32000, CT_TEMPORARY);
  shiftedVersion = MM_PushChunk(32000, CT_TEMPORARY);

  if (mapSecondaryBackdrop)
  {
    LoadAssetFile(
      MakeFilename("DROP", mapSecondaryBackdrop, ".mni"), backdropData);

    // Copying a full backdrop image requires 8000 bytes per plane, but here,
    // only 6720 are copied. This is because a secondary backdrop is only
    // used when there is no vertical backdrop motion. Therefore,
    // the bottom 32 rows (= 4 tiles) can be skipped, as they will always be
    // covered up by the HUD.
    UploadTileset(backdropData, 6720, 0xC000);

    ShiftPixelsHorizontally(backdropData, shiftedVersion, 4);
    UploadTileset(shiftedVersion, 6720, 0xE000);
  }

  LoadAssetFile(LVL_BACKDROP_FILENAME(), backdropData);
  UploadTileset(backdropData, 8000, 0x8000);

  if (mapBackdropAutoScrollX)
  {
    ShiftPixelsHorizontally(backdropData, shiftedVersion, 2);
    UploadTileset(shiftedVersion, 6720, 0xA000);

    ShiftPixelsHorizontally(shiftedVersion, backdropData, 2);
    UploadTileset(backdropData, 6720, 0xC000);

    ShiftPixelsHorizontally(backdropData, shiftedVersion, 2);
    UploadTileset(shiftedVersion, 6720, 0xE000);
  }
  else if (mapParallaxBoth || mapParallaxHorizontal || mapBackdropAutoScrollY)
  {
    if (mapParallaxHorizontal)
    {
      ShiftPixelsHorizontally(backdropData, shiftedVersion, 4);
      UploadTileset(shiftedVersion, 6720, 0xA000);
    }
    else
    {
      ShiftPixelsHorizontally(backdropData, shiftedVersion, 4);
      UploadTileset(shiftedVersion, 8000, 0xA000);

      ShiftPixelsVertically(backdropData, shiftedVersion);
      UploadTileset(shiftedVersion, 8000, 0xC000);

      ShiftPixelsHorizontally(shiftedVersion, backdropData, 4);
      UploadTileset(backdropData, 8000, 0xE000);
    }
  }

  MM_PopChunks(CT_TEMPORARY);
}
