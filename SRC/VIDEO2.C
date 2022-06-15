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

Video playback system - playback code

The majority of this code seems to have been copied from Jim Kent's article
"The FLIC file format", published March 1993 in Dr. Dobb's Journal.
I've kept the code fairly close to the version in the article, preserving
original identifier names and comments for the most part.
I've changed the formatting, adjusted some function descriptions, and added
a few additional comments. Places where the code in Duke Nukem II differs from
the article version are also highlighted/mentioned.

An online version of the article can be found here:
  https://www.drdobbs.com/windows/the-flic-file-format/184408954

It doesn't have the full source code, though, and there's no download option
anymore. I've tracked down the original files and uploaded them here:
  https://archive.org/details/flic_20220809


The original code's Copyright notice:

  Copyright (c) 1992 Jim Kent.  This file may be freely used, modified,
  copied and distributed.  This file was first published as part of
  an article for Dr. Dobb's Journal March 1993 issue.

*******************************************************************************/


/*******************************************************************************

From types.h

*******************************************************************************/


typedef signed char Char;       /* Signed 8 bits. */
typedef unsigned char Uchar;    /* Unsigned 8 bits. */
typedef short Short;            /* Signed 16 bits please. */
typedef unsigned short Ushort;  /* Unsigned 16 bits please. */
typedef long Long;              /* Signed 32 bits. */
typedef unsigned long Ulong;    /* Unsigned 32 bits. */

typedef int Boolean;      /* TRUE or FALSE value. */
typedef int ErrCode;      /* ErrXXX or Success. */
typedef int FileHandle;   /* OS file handle. */

/* Values for ErrCodes */
#define Success      0 /* Things are fine. */
#define Error       -1 /* Unclassified error. */



/*******************************************************************************

From flic.h

*******************************************************************************/


/* Flic Header */
typedef struct
{
  Long    size;     /* Size of flic including this header. */
  Ushort  type;     /* Either FLI_TYPE or FLC_TYPE below. */
  Ushort  frames;   /* Number of frames in flic. */
  Ushort  width;    /* Flic width in pixels. */
  Ushort  height;   /* Flic height in pixels. */
  Ushort  depth;    /* Bits per pixel.  (Always 8 now.) */
  Ushort  flags;    /* FLI_FINISHED | FLI_LOOPED ideally. */
  Long    speed;    /* Delay between frames. */
  Short   reserved1;  /* Set to zero. */
  Ulong   created;  /* Date of flic creation. (FLC only.) */
  Ulong   creator;  /* Serial # of flic creator. (FLC only.) */
  Ulong   updated;  /* Date of flic update. (FLC only.) */
  Ulong   updater;  /* Serial # of flic updater. (FLC only.) */
  Ushort  aspect_dx;  /* Width of square rectangle. (FLC only.) */
  Ushort  aspect_dy;  /* Height of square rectangle. (FLC only.) */
  Char    reserved2[38]; /* Set to zero. */
  Long    oframe1;  /* Offset to frame 1. (FLC only.) */
  Long    oframe2;  /* Offset to frame 2. (FLC only.) */
  Char    reserved3[40]; /* Set to zero. */

} FlicHead;


/* Values for FlicHead.type */
#define FLI_TYPE 0xAF11u  /* 320x200 .FLI type ID */
#define FLC_TYPE 0xAF12u  /* Variable rez .FLC type ID */

/* Values for FlicHead.flags */
#define FLI_FINISHED 0x0001
#define FLI_LOOPED   0x0002


/* Frame Header */
typedef struct
{
  Long   size;        /* Size of frame including header. */
  Ushort type;        /* Always FRAME_TYPE */
  Short  chunks;      /* Number of chunks in frame. */
  Char   reserved[8]; /* Always 0. */
} FrameHead;

#define FRAME_TYPE 0xF1FAu


/* Chunk Header */
typedef struct
{
  Long size;    /* Size of chunk including header. */
  Ushort type;  /* Value from ChunkTypes below. */
} ChunkHead;


enum ChunkTypes
{
  COLOR_256 = 4,  /* 256 level color palette info. (FLC only.) */
  DELTA_FLC = 7,  /* Word-oriented delta compression. (FLC only.) */
  COLOR_64  = 11, /* 64 level color palette info. */
  DELTA_FLI = 12, /* Byte-oriented delta compression. */
  BLACK     = 13, /* whole frame is color 0 */
  BYTE_RUN  = 15, /* Byte run-length compression. */
  LITERAL   = 16, /* Uncompressed pixels. */
  PSTAMP    = 18, /* "Postage stamp" chunk. (FLC only.) */
};


/*******************************************************************************

From readflic.h

*******************************************************************************/


/* Some handy macros I use in lots of programs: */

/** Clear a block of memory. */
#define ClearMem(buf, size) memset(buf, 0, size)

/** Clear a structure (pass in pointer) */
#define ClearStruct(pt) ClearMem(pt, sizeof(*(pt)))


/* Data structures peculiar to readflic program: */

typedef struct
{
  FlicHead head;  /* Flic file header. */
  int handle;     /* File handle. */
  int frame;      /* Current frame in flic. */
  char far* name; /* Name from flic_open. */
  int xoff, yoff; /* Offset to display flic at. */
} Flic;


/* Various error codes flic reader can get. */
#define ErrNoMemory -2 /* Not enough memory. */
#define ErrBadFlic  -3 /* File isn't a flic. */
#define ErrBadFrame -4 /* Bad frame in flic. */
#define ErrOpen     -5 /* Couldn't open file.  Check errno. */
#define ErrRead     -6 /* Couldn't read file.  Check errno. */


/*******************************************************************************

From pcclone.h

*******************************************************************************/


/* Pixel type. */
typedef Uchar Pixel;


/* One color map entry r,g,b 0-255. */
typedef struct
{
  Uchar r,g,b;
} Color;


/* Device specific screen type. */
typedef struct
{
  Pixel far *pixels;  /* Set to AOOO:0000 for hardware. */
  int width, height;  /* Dimensions of screen. (320x200) */
  int old_mode;       /* Mode screen was in originally. */
  Boolean is_open;    /* Is screen open? */
} Screen;


/* BigBlock - handles allocating big blocks of memory (>64K) on the PC. */
typedef struct
{
  void huge *hpt;
  void far *fpt;
} BigBlock;


/* Clock structure. */
typedef struct
{
  Ulong speed;  /* Number of clock ticks per second. */
} Clock;


/* Keyboard structure. */
typedef struct
{
  Uchar ascii;
  Ushort scancode;
} Key;


/* Machine structure - contains all the machine dependent stuff. */
typedef struct
{
  Screen screen;
  Clock clock;
  Key key;
} Machine;


/*******************************************************************************

From pcclone.c and readflic.c, plus additional code not found in the article

*******************************************************************************/


void flic_close(Flic* flic);
void screen_put_colors(Screen* s, int start, Color far* colors, int count);
void screen_put_colors_64(Screen* s, int start, Color far* colors, int count);
void screen_copy_seg(Screen* s, int x, int y, Pixel far* pixels, int count);
void screen_repeat_one(Screen* s, int x, int y, Pixel pixel, int count);


/* Output callback parameter type for decode_color.
 * Not coincidentally, screen_put_colors is of this type. */
typedef void ColorOut(Screen* s, int start, Color far* colors, int count);


/** Open a binary file to read.
 *
 * Uses the game's own filesystem functions instead of the standard library.
 * Terminates the program in case of an error, so the return value is
 * meaningless.
 */
ErrCode file_open_to_read(FileHandle* phandle, char far* name)
{
  OpenAssetFile(name, phandle);
  return Success;
}


/** Read a block
 *
 * Identical to the version in the article.
 */
ErrCode file_read_block(FileHandle handle, void far* block, unsigned size)
{
  unsigned size1 = size;

  if (_dos_read(handle, block, size1, &size1) != Success)
  {
    return ErrRead;
  }
  else
  {
    return Success;
  }
}


/** Read a big block
 *
 * Almost identical to the version in the article, but uses far pointers
 * instead of huge pointers. This breaks the ability of the code to read
 * blocks larger than 64k, and makes this entire function unnecessary.
 */
static ErrCode file_read_big_block(FileHandle handle, BigBlock* bb, Ulong size)
{
  char far* pt = (char far*)bb->hpt;
  unsigned size1;
  ErrCode err;

  while (size != 0)
  {
    size1 = ((size > 0xFFF0) ? 0xFFF0 : size);

    if ((err = file_read_block(handle, pt, size1)) < Success)
    {
      return err;
    }

    pt += size1;    /* Advance pointer to next batch. */
    size -= size1;  /* Subtract current batch from size to go. */
  }

  return Success;
}


/** Wait until current frame delay elapsed or key pressed
 *
 * This has some vague resemblance to the function wait_til()
 * from the article's code, but it uses global state instead
 * of accepting a parameter, and uses the game's own timing
 * and keyboard handling systems.
 * Additionally, returns -1 (`Error`) instead of `ErrCancel`
 * if interrupted by a keypress.
 */
ErrCode AwaitNextFrame(void)
{
  sysFastTicksElapsed = 0;

  do
  {
    // See comment in MusicService() in music.c
    if (ANY_KEY_PRESSED() || hackStopApogeeLogo)
    {
      return Error;
    }
  }
  while (sysFastTicksElapsed < flicFrameDelay);

  return Success;
}


/** Decode color map, invoking `output` to write to the hardware palette
 *
 * The two color compressions schemes are identical except that RGB values are
 * either in range 0-63 or 0-255. Passing in an output callback that does
 * appropriate shifting on the way to the real palette lets us use the same code
 * for both COLOR_64 and COLOR_256 compression.
 *
 * Identical to the version in the article, but uses far pointers only.
 */
static void
  decode_color(Uchar far* data, Flic* flic, Screen* s, ColorOut* output)
{
  int start = 0;
  Uchar far* cbuf = (Uchar far*)data;
  Short far* wp = (Short far*)cbuf;
  Short ops;
  int count;

  ops = *wp;
  cbuf += sizeof(*wp);

  while (--ops >= 0)
  {
    start += *cbuf++;

    if ((count = *cbuf++) == 0)
    {
      count = 256;
    }

    (*output)(s, start, (Color far*)cbuf, count);

    cbuf += 3 * count;
    start += count;
  }
}


/** Allocate a big block.
 *
 * Mostly identical to the version in the article, but using the game's own
 * memory manager instead of the standard library. Only supports block sizes
 * <= 64k, so the whole "big block" system is kind of pointless and could
 * have been removed.
 */
ErrCode big_alloc(BigBlock* bb, Ulong size)
{
  bb->fpt = MM_PushChunk((word)size, CT_TEMPORARY);
  bb->hpt = bb->fpt;

  // [NOTE] This can never happen, since MM_PushChunk terminates the program
  // if we run out of memory.
  if (!bb->fpt)
  {
    return ErrNoMemory;
  }
  else
  {
    return Success;
  }
}


/** Free up a big block.
 *
 * Mostly identical to the version in the article, but using the game's own
 * memory manager instead of the standard library.
 */
void big_free(BigBlock* bb)
{
  if (bb->fpt != NULL)
  {
    MM_PopChunk(CT_TEMPORARY);
    ClearStruct(bb);
  }
}


/** Decode COLOR_256 chunk
 *
 * Identical to the version in the article, but uses far pointers only.
 */
static void decode_color_256(Uchar far* data, Flic* flic, Screen* s)
{
  decode_color(data, flic, s, screen_put_colors);
}


/** Decode COLOR_64 chunk
 *
 * Identical to the version in the article, but uses far pointers only.
 */
static void decode_color_64(Uchar far* data, Flic* flic, Screen* s)
{
  decode_color(data, flic, s, screen_put_colors_64);
}


/** Decode RLE-compressed frame
 *
 * Identical to the version in the article, but uses far pointers only.
 */
static void decode_byte_run(Uchar far* data, Flic* flic, Screen* s)
{
  int x, y;
  int width = flic->head.width;
  int height = flic->head.height;
  Char psize;
  Char far* cpt = data;
  int end;

  y = flic->yoff;
  end = flic->xoff + width;

  while (--height >= 0)
  {
    x = flic->xoff;
    cpt += 1; /* skip over obsolete opcount byte */
    psize = 0;

    while ((x += psize) < end)
    {
      psize = *cpt++;
      if (psize >= 0)
      {
        screen_repeat_one(s, x, y, *cpt++, psize);
      }
      else
      {
        psize = -psize;
        screen_copy_seg(s, x, y, (Pixel far*)cpt, psize);
        cpt += psize;
      }
    }

    y++;
  }
}


/** Decode FLI-style delta compressed frame
 *
 * Implements generally the same logic as the version in the article, but has
 * been completely rewritten in Assembly for speed. Directly writes to video
 * memory instead of using the screen_repeat_one/screen_copy_seg functions, and
 * ignores the xoff/yoff members. Also uses a far pointer instead of a huge
 * pointer for the data.
 */
static void decode_delta_fli(Uchar far* data, Flic* flic, Screen* s)
{
#if 0
  int xorg = flic->xoff;
  int yorg = flic->yoff;
  Short huge *wpt = (Short huge *)data;
  Uchar huge *cpt = (Uchar huge *)(wpt + 2);
  int x,y;
  Short lines;
  Uchar opcount;
  Char psize;

  y = yorg + *wpt++;
  lines = *wpt;

  while (--lines >= 0)
  {
    x = xorg;
    opcount = *cpt++;

    while (opcount > 0)
    {
      x += *cpt++;
      psize = *cpt++;

      if (psize < 0)
      {
        psize = -psize;
        screen_repeat_one(s, x, y, *cpt++, psize);
        x += psize;
        opcount-=1;
      }
      else
      {
        screen_copy_seg(s, x, y, (Pixel far *)cpt, psize);
        cpt += psize;
        x += psize;
        opcount -= 1;
      }
    }

    y++;
  }
#else
  int rowsLeft;

  asm push  ds

  // Load data ptr into DS:SI
  asm lds   si, [data]

  // Set up ES:DI to point to start of video memory (A000:0000)
  asm mov   ax, 0xA000
  asm mov   es, ax
  asm xor   di, di

  // Read one word of data from DS:SI. This indicates how many rows we should
  // skip, so we advance the target pointer (in DI) by the value we read times
  // the width of the screen
  asm lodsw
  asm mov   dx, 320
  asm mul   dx
  asm add   di, ax

  // Read 2nd word, this tells us how many rows we need to update.
  // Store it in rowsLeft.
  asm lodsw
  asm mov   [rowsLeft], ax

  // DX tracks the beginning of the current screen row
  asm mov   dx, di

  asm xor   ah, ah

row_loop:
  // Set destination pointer to start of current screen row
  asm mov   di, dx

  // Read the first byte of the current row data, this tells us how many updates
  // (operations) we need to do for this row.
  asm lodsb

  // Use BL register to keep track of number of ops for this row
  asm mov   bl, al

  // Check if the count is zero
  asm test  bl, bl
  asm jmp   test_row_done

col_loop:
  // Read first byte of the operation, this is an X offset, so add it to the
  // destination pointer
  asm lodsb
  asm add   di, ax

  // Read the 2nd byte, and check if it's negative
  asm lodsb
  asm test  al, al
  asm js    repeat_byte

  // value is positive, so it indicates the number of bytes to copy.
  // REP MOVSB performs the copy (CX bytes).
  asm mov   cx, ax
  asm rep   movsb

  // Decrement operation count, continue with the next op if there are some
  // left, otherwise go to the next row.
  asm dec   bl
  asm jnz   col_loop
  asm jmp   next_row

  // value is negative, we need to repeat the following byte `-value` times
repeat_byte:
  asm neg   al     // N = -N
  asm mov   cx, ax // set repeat count for REP STOSB below

  // Load the byte that we need to repeat
  asm lodsb

  // write it N times
  asm rep stosb

  // Decrement operation count, then check if we're done with the current row
  asm dec bl

test_row_done:
  // If the count is non-zero, process the row, otherwise go to the next one.
  // This instruction here also doubles as the loop end condition for the inner
  // loop's negative value case (repeat_byte).
  asm jnz   col_loop

next_row:
  // Advance row start pointer by screen width, decrement row counter and exit
  // the loop once it reaches 0
  asm add   dx, 320
  asm dec   [rowsLeft]
  asm jnz   row_loop

  asm pop   ds
#endif
}


/** Decode a LITERAL chunk. Copy data to screen one line at a time.
 *
 * Identical to the version in the article, but uses far pointers only.
 * This chunk type isn't used in any of the video files shipping with the game,
 * but support was left in the code.
 */
static void decode_literal(Uchar far* data, Flic* flic, Screen* s)
{
  int i;
  int height = flic->head.height;
  int width = flic->head.width;
  int x = flic->xoff;
  int y = flic->yoff;

  for (i = 0; i < height; ++i)
  {
    screen_copy_seg(s, x, y + i, (Pixel far*)data, width);
    data += width;
  }
}


/** Open flic file. Read header, verify it's a flic.
 *
 * Almost ientical to the version in the article aside from some removed code.
 */
ErrCode flic_open(Flic* flic, char far* name)
{
  ErrCode err;
  ClearStruct(flic); /* Start at a known state. */

  if ((err = file_open_to_read(&flic->handle, name)) >= Success)
  {
    if (
      (err = file_read_block(flic->handle, &flic->head, sizeof(flic->head))) >=
      Success)
    {
      /* Save name for future use. [NOTE] Never actually used. */
      flic->name = name;

      // Support for the .FLC variant of the file format has been removed,
      // since the game only uses the .FLI variant.
#if 0
      if (flic->head.type == FLC_TYPE)
      {
        /* Seek frame 1. */
        lseek(flic->handle, flic->head.oframe1, SEEK_SET);
        return Success;
      }
#endif

      if (flic->head.type == FLI_TYPE)
      {
        /* Do some conversion work here. */
        flic->head.oframe1 = sizeof(flic->head);

        // [NOTE] Unnecessary, since the game ignores the speed field and
        // hardcodes playback speed instead (see OnNewVideoFrame() function)
        flic->head.speed = flic->head.speed * 1000L / 70L;
        return Success;
      }
      else
      {
        err = ErrBadFlic;
      }
    }

    flic_close(flic); /* Close down and scrub partially opened flic. */
  }

  return err;
}


/** Close flic file and scrub flic
 *
 * Identical to the version in the article.
 */
void flic_close(Flic* flic)
{
  close(flic->handle);
  ClearStruct(flic); /* Discourage use after close. */
}


/** Decode a loaded frame onto the screen
 *
 * Loops through all chunks calling the appropriate chunk decoder for each one.
 *
 * Almost ientical to the version in the article, but some code has been
 * removed, and huge pointers have been replaced with far pointers.
 */
static ErrCode
  decode_frame(Flic* flic, FrameHead* frame, Uchar far* data, Screen* s)
{
  int i;
  ChunkHead far* chunk;

  for (i = 0; i < frame->chunks; ++i)
  {
    chunk = (ChunkHead far*)data;
    data += chunk->size;

    switch (chunk->type)
    {
      case COLOR_256:
        decode_color_256((Uchar far*)(chunk + 1), flic, s);
        break;

#if 0
      case DELTA_FLC:
        decode_delta_flc((Uchar far*)(chunk + 1), flic, s);
        break;
#endif

      case COLOR_64:
        decode_color_64((Uchar far*)(chunk + 1), flic, s);
        break;

      case DELTA_FLI:
        decode_delta_fli((Uchar far*)(chunk + 1), flic, s);
        break;

#if 0
      case BLACK:
        decode_black((Uchar far*)(chunk + 1), flic, s);
        break;
#endif

      case BYTE_RUN:
        decode_byte_run((Uchar far*)(chunk + 1), flic, s);
        break;

      case LITERAL:
        decode_literal((Uchar far*)(chunk + 1), flic, s);
        break;

      default:
        break;
    }
  }

  return Success;
}


/** Advance to the next frame in the flic file
 *
 * Identical to the version in the article.
 */
ErrCode flic_next_frame(Flic* flic, Screen* screen)
{
  FrameHead head;
  ErrCode err;
  BigBlock bb;
  long size;

  if ((err = file_read_block(flic->handle, &head, sizeof(head))) >= Success)
  {
    if (head.type == FRAME_TYPE)
    {
      size = head.size - sizeof(head); /* Don't include head. */

      if (size > 0)
      {
        if ((err = big_alloc(&bb, size)) >= Success)
        {
          if ((err = file_read_big_block(flic->handle, &bb, size)) >= Success)
          {
            err = decode_frame(flic, &head, (Uchar far*)bb.hpt, screen);
          }

          big_free(&bb);
        }
      }
    }
    else
    {
      err = ErrBadFrame;
    }
  }

  return err;
}


/** Locates 2nd frame of flic file and stores its address
 *
 * This is needed for looped playback.
 * Identical to the version in the article.
 */
static ErrCode fill_in_frame2(Flic* flic)
{
  FrameHead head;
  ErrCode err;

  lseek(flic->handle, flic->head.oframe1, SEEK_SET);

  if ((err = file_read_block(flic->handle, &head, sizeof(head))) < Success)
  {
    return err;
  }

  flic->head.oframe2 = flic->head.oframe1 + head.size;

  return Success;
}


/** Play back opened flic file specified number of times
 *
 * This is still recognizably similar to the version in the article, but has
 * been modified quite a bit to use the game's own timing system and to repeat
 * the video a set number of times instead of indefinitely.
 */
ErrCode flic_play_loop(
  Flic* flic,
  Machine* machine,
  int numRepetitions,
  word videoType)
{
  int i;
  int repetition;
  ErrCode err;

  if (flic->head.oframe2 == 0)
  {
    fill_in_frame2(flic);
  }

  /* Seek to first frame. */
  lseek(flic->handle, flic->head.oframe1, SEEK_SET);

  /* Display first frame. */
  if ((err = flic_next_frame(flic, &machine->screen)) < Success)
  {
    return err;
  }

  OnNewVideoFrame(videoType, 0);
  if (AwaitNextFrame())
  {
    return -Error;
  }

  for (repetition = 0; repetition < numRepetitions; repetition++)
  {
    lseek(flic->handle, flic->head.oframe2, SEEK_SET);

    // On the last repetition, we skip the ring frame, since it represents
    // the 1st frame.
    for (
      i = 0;
      i < flic->head.frames + (repetition + 1 == numRepetitions ? -1 : 0);
      ++i)
    {
      if ((err = flic_next_frame(flic, &machine->screen)) < Success)
      {
        return err;
      }

      // When we've reached the ring frame, we pass 0 as the frame number
      // to OnNewVideoFrame() instead of i + 1, since it represents the 1st
      // frame.
      OnNewVideoFrame(videoType, i + 1 == flic->head.frames ? 0 : i + 1);
      if (AwaitNextFrame())
      {
        return -Error;
      }
    }
  }

  return Success;
}


/** Set count colors in color map starting at start.
 *
 * RGB values go from 0 to 64.
 *
 * Identical to the version in the article.
 */
void screen_put_colors_64(Screen* s, int start, Color far* colors, int count)
{
  int end = start + count;
  int ix;

  for (ix = start; ix < end; ix++)
  {
    DN2_outportb(0x3C8, ix);
    DN2_outportb(0x3C9, colors->r);
    DN2_outportb(0x3C9, colors->g);
    DN2_outportb(0x3C9, colors->b);

    ++colors;
  }
}


/** Set count colors in color map starting at start.
 *
 * RGB values go from 0 to 255.
 *
 * Identical to the version in the article.
 */
void screen_put_colors(Screen* s, int start, Color far* colors, int count)
{
  int end = start + count;
  int ix;

  for (ix = start; ix < end; ix++)
  {
    DN2_outportb(0x3C8, ix);
    DN2_outportb(0x3C9, colors->r >> 2);
    DN2_outportb(0x3C9, colors->g >> 2);
    DN2_outportb(0x3C9, colors->b >> 2);

    ++colors;
  }
}


/** Copy pixels from memory to screen.
 *
 * Almost identical to the version in the article, but line clipping has been
 * removed.
 */
void screen_copy_seg(Screen* s, int x, int y, Pixel far* pixels, int count)
{
  Pixel far *pt;
  int xend;
  int unclipped_x = x;
  int dx;

#if 0
  /* First let's do some clipping. */
  if (!line_clip(s, &x, &y, &count))
  {
    return;
  }
#endif

  // [NOTE] This does nothing, it should've been removed along with
  // the line_clip call above
  dx = x - unclipped_x; /* Clipping change in start position. */
  if (dx != 0)
  {
    pixels += dx;      /* Advance over clipped pixels. */
  }

  /* Calculate start screen address. */
  pt = s->pixels + (unsigned)y * (unsigned)s->width + (unsigned)x;

  /* Copy pixels to display. */
  while (--count >= 0)
  {
    *pt++ = *pixels++;
  }
}


/** Draw a horizontal line of a solid color
 *
 * Almost identical to the version in the article, but line clipping has been
 * removed.
 */
void screen_repeat_one(Screen* s, int x, int y, Pixel color, int count)
{
  Pixel far *pt;

#if 0
  /* First let's do some clipping. */
  if (!line_clip(s, &x, &y, &count))
  {
    return;
  }
#endif

  /* Calculate start screen address. */
  pt = s->pixels + (unsigned)y * (unsigned)s->width + (unsigned)x;

  /* Repeat pixel on display. */
  while (--count >= 0)
  {
    *pt++ = color;
  }
}


/** Initialize Screen struct
 *
 * This has been heavily stripped down compared to the version
 * in the article. The game has its own systems for "opening"
 * the screen so it doesn't need to do very much here.
 */
ErrCode screen_open(Screen* s)
{
  s->width  = 320;
  s->height = 200;
  s->pixels = MK_FP(0xA000, 0); /* Base video screen address. */

  return Success;
}


/** Plays a video file
 *
 * Plays back the given video file, repeated numRepetitions times.
 * The video type defines the playback speed, sound effects etc.
 * (see OnNewVideoFrame()).
 *
 * Returns false if the video played to completion, true if it was
 * interrupted by the user or if an error occured.
 */
bool PlayVideo(char far* filename, word videoType, int numRepetitions)
{
  byte result;
  Flic flic;
  Machine machine;

  screen_open(&machine.screen);

  flicFrameDelay = 0;

  if (flic_open(&flic, filename) != Success)
  {
    // Empty if statement body, kept for parity with the original machine code.
    // If the file can't be opened, the entire program will be terminated, same
    // if there's not enough memory. But this doesn't cover other error
    // conditions like a corrupt file or non-flic file.
  }

  result = flic_play_loop(&flic, &machine, numRepetitions, videoType);

  flic_close(&flic);

  if (result != Success)
  {
    return true;
  }

  return false;
}
