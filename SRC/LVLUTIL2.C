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

Level loading utilities, part 2: RLE compression, backdrop loading

*******************************************************************************/


/** Calculate bytes needed to represent `data` in RLE-compressed form
 *
 * This function is not used in the shipping game. We can only speculate as to
 * what it was used for - perhaps the code to compress the extra data in level
 * files was part of the game executable, not the level editor? Or maybe it
 * was used for some experiments to make a decision on what kind of compression
 * scheme to use, and then never removed? Perhaps the decompression code was
 * copied from another code base, and this function was accidentally copied
 * along with it?
 */
int pascal CalcCompressedSize(byte far* data, int size)
{
  // TODO: Document how this works

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


/** Decompress RLE-compressed data from `src` into `dest`
 *
 * This is used to decompress the extra map data in level files. See
 * LoadMapData() in main.c, and UpdateAndDrawGame() in game2.c
 */
void pascal DecompressRLE(byte far* src, byte far* dest)
{
  sbyte i;
  sbyte marker;

  for (;;)
  {
    marker = *src;

    // A 0 marker indicates the end of the compressed data
    if (!marker)
    {
      return;
    }

    if (marker > 0)
    {
      // A positive marker means that we should repeat the byte following the
      // marker. The marker itself indicates the number of repetitions.
      for (i = 0; i < marker; i++)
      {
        *dest++ = *(src + 1);
      }

      src += 2;
    }
    else
    {
      // A negative marker means that we should copy `-marker` bytes following
      // the marker unchanged.
      src++;
      for (i = 0; i < -marker; i++)
      {
        *dest++ = *src++;
      }
    }
  }
}


#include "music.c"


/** Load backdrop image(s) for current level and prepare parallax scrolling
 *
 * LoadLevelHeader() must be called before using this function.
 */
void LoadBackdrop(void)
{
  byte far* backdropData;
  byte far* shiftedVersion;

  // The game's parallax scrolling and continuous scrolling appears to move
  // the background in increments of 4 or 2 pixels, but the game can actually
  // only draw images at positions that are a multiple of 8. Everything is tied
  // to a fixed grid of 8x8-pixel cells, aka tiles.
  // In order to still create the illusion of finer-grained scrolling, this
  // function creates multiple copies of the backdrop image which have been
  // shifted up and/or left by a number of pixels. The logic in UpdateBackdrop()
  // (in game2.c) then chooses the right version of the backdrop depending on
  // what scroll offset is currently needed.

  // Allocate temporary buffers for working with the images. The images are
  // in the same format as the game's tilesets, and are stored in video memory.
  // This allows us to draw them quickly using the latch copy technique
  // (see BlitSolidTile() in gfx.asm). But since we are going to create modifed
  // copies of the image(s), we also need some buffers in main memory.
  backdropData = MM_PushChunk(32000, CT_TEMPORARY);
  shiftedVersion = MM_PushChunk(32000, CT_TEMPORARY);

  // In total, there can be up to 4 versions of the backdrop image. These
  // are stored in video memory at offsets 0x8000 through 0xE000.
  // Offsets 0x0000 and 0x2000 are the first two VGA screen pages, which act
  // as front-buffer and back-buffer for the game. Offset 0x4000 is used for
  // the level-specific tileset, and 0x6000 holds the status icon tileset.
  //
  // How the 4 possible image slots are used depends on the level's background
  // scrolling mode:
  //
  // | Mode                     | 0x8000 | 0xA000 | 0xC000 | 0xE000 |
  // | ------------------------ | ------ | ------ | ------ | ------ |
  // | No scrolling             |  0,  0 | N/A    | N/A    | N/A    |
  //
  // | Horizontal parallax      |  0,  0 | -4,  0 | N/A    | N/A    |
  //
  // | Horiz. parallax with     |  0,  0 | -4,  0 |  0,  0 | -4,  0 |
  // | secondary backdrop          ^^^^^^^^^^^^^^    ^^^^^^^^^^^^^^
  //                               primary image     secondary img.
  //
  // | Bidirectional parallax   |  0,  0 | -4,  0 |  0, -4 | -4, -4 |
  //
  // | Auto-scroll (horizontal) |  0,  0 | -2,  0 | -4,  0 | -6,  0 |
  //
  // | Auto-scroll (vertical)   |  0,  0 | -4,  0 |  0, -4 | -4, -4 |
  //
  // This arrangement means that a secondary backdrop can only be used with
  // horizontal only parallax. The 3rd and 4th image slot are needed for the
  // secondary backdrop, so there's no space for the 2 additional images needed
  // for vertical parallax.

  // Upload secondary backdrop if there is one. Unlike the primary, which is
  // specified by filename in the level file, the secondary one is just
  // referenced by number.
  if (mapSecondaryBackdrop)
  {
    LoadAssetFile(
      MakeFilename("DROP", mapSecondaryBackdrop, ".mni"), backdropData);

    // Upload the unmodified secondary backdrop.
    //
    // Copying a full backdrop image requires 8000 bytes per plane, but here,
    // only 6720 are copied. This is because a secondary backdrop is only
    // used when there is no vertical backdrop motion. Therefore,
    // the bottom 32 rows (= 4 tiles) can be skipped, as they will always be
    // covered up by the HUD. 32 rows times 40 bytes per row is 1280, which
    // is the difference to get from 6720 to 8000.
    UploadTileset(backdropData, 6720, 0xC000);

    // Create and upload copy shifted left by 4.
    ShiftPixelsHorizontally(backdropData, shiftedVersion, 4);
    UploadTileset(shiftedVersion, 6720, 0xE000);
  }

  // Upload unmodified primary backdrop
  LoadAssetFile(LVL_BACKDROP_FILENAME(), backdropData);
  UploadTileset(backdropData, 8000, 0x8000);

  if (mapBackdropAutoScrollX)
  {
    // Like for the secondary backdrop, we only copy the first 6720 bytes here
    // since the bottom 32 rows are never visible when horizontal auto-scrolling
    // is active.

    // Create and upload copy shifted left by 2
    ShiftPixelsHorizontally(backdropData, shiftedVersion, 2);
    UploadTileset(shiftedVersion, 6720, 0xA000);

    // Create and upload copy shifted left by 4, by further shifting the copy
    // from the previous step
    ShiftPixelsHorizontally(shiftedVersion, backdropData, 2);
    UploadTileset(backdropData, 6720, 0xC000);

    // Create and upload copy shifted left by 6, by further shifting the copy
    // from the previous step
    ShiftPixelsHorizontally(backdropData, shiftedVersion, 2);
    UploadTileset(shiftedVersion, 6720, 0xE000);
  }
  else if (mapParallaxBoth || mapParallaxHorizontal || mapBackdropAutoScrollY)
  {
    if (mapParallaxHorizontal)
    {
      // Create and upload copy shifted left by 4.
      //
      // Like for the secondary backdrop, we only copy the first 6720 bytes
      // here since the bottom 32 rows are never visible when only horizontal
      // parallax is active.
      ShiftPixelsHorizontally(backdropData, shiftedVersion, 4);
      UploadTileset(shiftedVersion, 6720, 0xA000);
    }
    else
    {
      // For both bidirectional parallax as well as vertical auto-scrolling,
      // we create three copies which are shifted by (-4, 0), (0, -4), and
      // (-4, -4), respectively.
      ShiftPixelsHorizontally(backdropData, shiftedVersion, 4);
      UploadTileset(shiftedVersion, 8000, 0xA000);

      ShiftPixelsVertically(backdropData, shiftedVersion);
      UploadTileset(shiftedVersion, 8000, 0xC000);

      ShiftPixelsHorizontally(shiftedVersion, backdropData, 4);
      UploadTileset(backdropData, 8000, 0xE000);
    }
  }

  // Free temporary buffers.
  //
  // [UNSAFE] In case any other memory was allocated using the CT_TEMPORARY
  // chunk type right before calling this function, this will also deallocate
  // that unrelated memory. It would be better to do two separate singular
  // MM_PopChunk() calls here.
  MM_PopChunks(CT_TEMPORARY);
}
