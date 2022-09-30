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

Level loading utilities, part 1

Various functions needed for loading levels.

*******************************************************************************/


/** Set derived map size variables based on the width */
void pascal SetMapSize(word width)
{
  // In Duke Nukem II, the map data is stored in a fixed-size buffer. Levels
  // can have different widths, but since the total size is fixed, the height
  // is derived from the width. The mapData buffer holds 32750 tile values in
  // total, so the height can be derived by: 32750 / width.
  //
  // This is what this function does, but it also has an additional task.
  // There are a few places in the code where values need to be multiplied by
  // the width of the map. Since multiplication is quite expensive on the CPUs
  // of the time, this is instead done via left-shifting, which is equivalent
  // to multiplying with a power of 2. The code here thus determines the shift
  // value needed to achieve the same result as if by multiplication.
  // This could be done by computing log2(width), but instead, we simply use
  // a lookup table of all possible width values, and then search for a match.
  //
  // [PERF] Missing `static` causes a copy operation here
  const word MAP_WIDTHS[] = { 32, 64, 128, 256, 512, 1024 };

  word i;
  for (i = 0; i < 6; i++)
  {
    if (MAP_WIDTHS[i] == width)
    {
      // Set mapWidthShift to log2(width). log2(32) is 5, and increments by one
      // with each doubling of the width.
      mapWidthShift = i + 5;

      // Set mapBottom to `height - 1`. The map data buffer is actually 32750
      // tiles long, but for some reason 32768 was used here. To compensate for
      // the difference, subtract 1 again.
      mapBottom = 32768 / width - 1 - 1;
      break;
    }
  }
}


/** Set variables based on a few level header bytes */
void pascal ParseLevelFlags(
  byte flags, byte secondaryBackdrop, byte unused1, byte unused2)
{
  mapParallaxBoth =               flags & 0x1;
  mapParallaxHorizontal =         flags & 0x2;
  mapBackdropAutoScrollX =        flags & 0x8;
  mapBackdropAutoScrollY =        flags & 0x10;
  mapHasEarthquake =              flags & 0x20;
  mapHasReactorDestructionEvent = flags & 0x40;
  mapSwitchBackdropOnTeleport =   flags & 0x80;

  mapSecondaryBackdrop = secondaryBackdrop;
}


/** Makes a copy of srcImage with all pixels shifted up by 4, with wrap-around
  *
  * srcImage must be a 320x200 image in backdrop/tileset format.  A copy of the
  * image is written to destImage, but with all pixels shifted up by 4, with
  * wrap-around. I.e. the 4 top-most rows of pixels are removed from the top
  * and placed at the bottom instead.
  */
void pascal ShiftPixelsVertically(byte far* srcImage, byte far* destImage)
{
  register word col;
  register word i;
  word bufferIndex;
  word offset;
  word row;
  byte tempBuffer[4 * 160];

  // Backdrops (and non-masked tilesets) are laid out as a sequence of 8x8 pixel
  // blocks (tiles). A consecutive span of 4 bytes describes a line of 8 pixels.
  // A block has 8 of these, so it occupies 8*4 = 32 consecutive bytes.
  // After that, the next block starts, etc.
  // A backdrop is 320x200, so it consists of 40x25 tiles
  // (320/8 = 40, 200/8 = 25).

  // First, copy the 4 top rows of pixels into our temp buffer, so that we can
  // place them at the bottom of the output image later.
  bufferIndex = 0;
  offset = 0;

  for (col = 0; col < 40; col++) // for each column of tiles
  {
    // Copy the first 4 lines of this tile. Each line is 4 bytes, so we need to
    // copy 4*4 = 16 bytes.
    for (i = 0; i < 16; i++)
    {
      *(tempBuffer + bufferIndex++) = *(srcImage + offset++);
    }

    // Skip the remaining 4 lines, this puts us at the start of the next tile
    offset += 16;
  }

  // Now, copy the source image to the destination, but offset by 4
  // (i.e., starting at y = 4).
  for (row = 0; row < 25; row++)
  {
    offset = 0;

    // Copy the lower 4 lines of the source image's current tile row to the
    // upper 4 lines of the destination's current tile row
    for (col = 0; col < 40; col++)
    {
      for (i = 0; i < 16; i++)
      {
        // Since a tile is 32 bytes, each row of tiles occupies 40*32 = 1280
        // bytes.

        // Offset the source by 16 to get to the lower 4 lines of each tile
        *(destImage + offset + (row*40*32)) =
          *(srcImage + (row*40*32) + offset++ + 16);
      }

      offset += 16;
    }

    // Copy the upper 4 lines of the next tile row to the lower 4 lines of the
    // current destination row
    offset = 0;
    for (col = 0; col < 40; col++)
    {
      for (i = 0; i < 16; i++)
      {
        // This time, we offset the destination by 16 to get to the lower 4
        // lines. The source is offset by 40*32 to skip to the beginning of the
        // next row of tiles.
        //
        // [BUG] This actually ends up reading out of bounds on the last loop
        // iteration, since 24*1280 + 1280 = 32000, which is right after the
        // end of the source image. So the bottom-most rows of the destination
        // are filled with random memory here, but it doesn't matter because
        // they are overwritten again further down when the temp buffer is
        // written to the destination.
        *(destImage + offset + (row*40*32) + 16) =
          *(srcImage + (row*40*32) + offset++ + 40*32);
      }

      offset += 16;
    }
  }

  // Finally, copy the contents of the temp buffer to the last 4 lines of the
  // destination, so that the source's upper 4 lines end up at the bottom of
  // the output image.
  //
  // [NOTE] I actually don't know why the temp buffer is used. The source image
  // is never modified, so we could just read directly from the source here,
  // and remove the temp buffer as well as the copy at the beginning.  One
  // possible theory is that the function used to operate in place at one
  // point, which would necessitate a temp buffer. This was later changed to
  // make a copy of the image instead, but the temp buffer wasn't removed.
  offset = 0;
  bufferIndex = 0;

  for (col = 0; col < 40; col++)
  {
    for (i = 0; i < 16; i++)
    {
      // 24*40*32 skips over 24 rows of tiles, adding 16 gets us to the
      // bottom 4 lines of the current tile
      *(destImage + offset++ + (24*40*32 + 16)) = *(tempBuffer + bufferIndex++);
    }

    offset += 16;
  }
}


/** Makes a copy of srcImage with all pixels shifted (rotated) left by amount
  *
  * srcImage must be a 320x200 image in backdrop/tileset format.
  * A copy of the image is written to destImage, but with all pixels shifted left
  * by amount, with wrap-around. I.e. the left-most columns of pixels are removed
  * from the left side of the image, and placed on the right instead.
  *
  * This only works correctly for amounts of 2 and 4!
  */
void pascal ShiftPixelsHorizontally(byte far* srcImage, byte far* destImage, byte amount)
{
  register word rowStart;
  register word colStart;
  word plane;
  word lineStart;
  byte tempBuffer[4];
  byte leftSideShift;

  // Backdrops (and non-masked tilesets) are laid out as a sequence of 8x8 pixel
  // blocks (tiles). A consecutive span of 4 bytes describes a row of 8 pixels.
  // A block has 8 rows of these, so it occupies 8*4 = 32 consecutive bytes.
  // After that, the next block starts, etc.
  // A backdrop is 320x200, so it consists of 40x25 tiles
  // (320/8 = 40, 200/8 = 25).
  //
  // Pixels are arranged in planar order. The first byte of each 8-pixel row
  // holds the first plane, the next one the 2nd plane etc.  This means that
  // each byte holds the bits for 8 pixels. Since we want to move pixels around
  // horizontally by less than 8, this means we need to address individual
  // bits.  Left-shifting a byte by the 'amount' value will move pixels to the
  // left within the byte, opening up the right-most pixel positions to be
  // replaced with the left-most pixels of the next tile. To extract those from
  // the corresponding tile's byte, we need to shift down by the "inverse"
  // amount. This could easily be computed as 8 - amount, but instead, the
  // following just explicitly handles an amount of 2. Changing the following
  // if/else to 'leftSideShift = 8 - amount' would make this function more
  // generic.
  if (amount == 2)
  {
    leftSideShift = 6;
  }
  else
  {
    leftSideShift = amount;
  }

  // Go through the image's tiles row by row. A new row of tiles
  // starts every 40*32 = 1280 bytes, and there are 25 rows in total
  for (rowStart = 0; rowStart < 25 * 1280; rowStart += 1280)
  {
    // Go through the lines within the tile row. One new line every 4 bytes
    for (lineStart = 0; lineStart < 4 * 8; lineStart += 4)
    {
      // Copy the left-most pixels of the left-most tile into our temp buffer
      for (plane = 0; plane < 4; plane++)
      {
        // Downshift to extract the N left-most pixels
        tempBuffer[plane] =
          *(srcImage + plane + lineStart + rowStart) >> leftSideShift;
      }

      // Now go through all tiles within the current row, at the current line
      for (colStart = 0; colStart < 40 * 32; colStart += 32)
      {
        for (plane = 0; plane < 4; plane++)
        {
          // Copy pixels to destination, but shifted left by amount.  The
          // left-shift moves the pixels in the line to the left, erasing the
          // left-most pixels and leaving 0-bits in the right-most pixel
          // positions.
          *(destImage + colStart + plane + lineStart + rowStart) =
            *(srcImage + colStart + plane + lineStart + rowStart) << amount;
        }

        if (colStart != 39 * 32) // Skip during last iteration
        {
          for (plane = 0; plane < 4; plane++)
          {
            // Take the left-most pixels from the next source tile, and place
            // them into the 0-bits left at the destination by the previous
            // loop.  Offsetting the source by 32 gets us to the next tile.
            *(destImage + colStart + plane + lineStart + rowStart) =
              *(destImage + plane + lineStart + rowStart + colStart) |
              (*(srcImage + plane + lineStart + rowStart + colStart + 32)
               >> leftSideShift);
          }
        }
      }

      // Finally, place the contents of the temp buffer at the very end of the
      // line. This makes the original image's left-most pixels appear at the
      // end of the destination, creating the wrap-around.
      for (plane = 0; plane < 4; plane++)
      {
        // Offset destination by 39*32 to get to the last tile in the current
        // row
        *(destImage + plane + rowStart + lineStart + 39*32) =
        *(destImage + plane + rowStart + lineStart + 39*32) |
          tempBuffer[plane];
      }
    }
  }
}
