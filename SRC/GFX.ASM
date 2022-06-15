;* Copyright (C) 2022, Nikolai Wuttke. All rights reserved.
;*
;* This project is based on disassembly of NUKEM2.EXE from the game
;* Duke Nukem II, Copyright (C) 1993 Apogee Software, Ltd.
;*
;* Some parts of the code are based on or have been adapted from the Cosmore
;* project, Copyright (c) 2020-2022 Scott Smitelli.
;* See LICENSE_Cosmore file at the root of the repository, or refer to
;* https://github.com/smitelli/cosmore/blob/master/LICENSE.
;*
;*
;* This program is free software: you can redistribute it and/or modify
;* it under the terms of the GNU General Public License as published by
;* the Free Software Foundation, either version 2 of the License, or
;* (at your option) any later version.
;*
;* This program is distributed in the hope that it will be useful,
;* but WITHOUT ANY WARRANTY; without even the implied warranty of
;* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;* GNU General Public License for more details.
;*
;* You should have received a copy of the GNU General Public License
;* along with this program.  If not, see <http://www.gnu.org/licenses/>.
;*


;
; Low-level graphics routines
;
; Large parts of this file are very close to or identical to the corresponding
; code in the Cosmo codebase. Because of that, I've taken over a lof of Scott
; Smitelli's documentation from the Cosmore project, adapted as needed to
; account for differences between the two games.
;
; References:
;  - [EGA]: IBM PC Hardware Reference Library - Enhanced Graphics Adapter,
;    August 2, 1984
;
; Unlike Cosmo, Duke Nukem II requires a VGA card for correct color
; reproduction.  But while the game features some 256-color scenes, the majority
; of the graphics use a 16-color EGA mode, only using the VGA to set a custom
; color palette that wouldn't be possible on an EGA card.  All of the low-level
; routines in this file only make use of EGA features, and are fully compatible
; with an actual EGA card.


IDEAL
ASSUME cs:_TEXT, ds:DGROUP, es:NOTHING


;
; Memory segment addresses.
;
EGA_SEGMENT                     EQU 0a000h
SOLID_TILE_SEGMENT              EQU (EGA_SEGMENT + 400h)


;
; Interrupt numbers.
;
INT_VIDEO_SERVICE               EQU 10h


;
; Video Service subfunctions.
;
VSVC_SET_VIDEO_MODE             EQU 0h
VSVC_SET_ACTIVE_PAGE            EQU 5h
VSVC_PALETTE_REGISTERS          EQU 10h


;
; Set/Get Palette Registers subfunctions.
;
PALREG_SET_BORDER_COLOR         EQU 1h


;
; EGA Sequencer I/O port and registers.
;
SEQUENCER_ADDR                  EQU 3c4h  ; During word access, high byte goes to 3c5h
SEQUENCER_DATA                  EQU 3c5h
SEQ_MAP_MASK                    EQU 2h


;
; EGA Graphics Controller I/O port and registers.
;
GRAPHICS_1_2_ADDR               EQU 3ceh  ; During word access, high byte goes to 3cfh
GRAPHICS_DATA                   EQU 3cfh
GFX_ENABLE_SET_RESET            EQU 1h
GFX_DATA_ROTATE                 EQU 3h
GFX_READ_MAP_SELECT             EQU 4h
GFX_MODE                        EQU 5h
GFX_COLOR_DONT_CARE             EQU 7h
GFX_BIT_MASK                    EQU 8h


;
; Other EGA registers
;
INPUT_STATUS_1_ADDR             EQU 3DAh


;
; Stride constants.
;
SCREEN_Y_STRIDE                 EQU 40
MASKED_TILE_ROW_STRIDE          EQU 5


SEGMENT _DATA

;
; Global storage in the data segment.
;

; This lookup table is used to avoid multiplication when deriving memory
; addresses from x/y coordinates (in tiles) in the various tile bitting
; functions. Each entry is (index * 8 * SCREEN_Y_STRIDE).
yOffsetTable    dw    0,  320,  640,  960, 1280, 1600, 1920, 2240, 2560, 2880
                dw 3200, 3520, 3840, 4160, 4480, 4800, 5120, 5440, 5760, 6080
                dw 6400, 6720, 7040, 7360, 7680
ENDS


SEGMENT _TEXT

;
; Global storage in the code segment.
;
drawPageSegment dw EGA_SEGMENT  ; EGA memory segment to be written to


; Subsequent instructions can use 80286 opcodes if desired, as that's the
; minimum supported CPU in-game.
P286


;
; Load a `mask` into the Map Mask [EGA, pg. 20] via the Sequencer Address
; Register [EGA, pg. 18].
;
; The bits in `mask` refer to planes 3-2-1-0. Two different byte-sized registers
; are being written with a single word-sized OUT; the high byte goes to the
; SEQUENCER_DATA I/O port.
;
MACRO SET_EGA_MAP_MASK mask
    mov   dx, SEQUENCER_ADDR
    mov   ax, (mask SHL 8) OR SEQ_MAP_MASK
    out   dx, ax
ENDM


;
; Load `map` into the Read Map Select register [EGA, pg. 50] via the Graphics 1
; & 2 Address Register [EGA, pg. 46] to select one of the four color planes
; using decimal notation.
;
; Plane numbers range from 0 (least significant) to 3 (most significant). Two
; bytes are sent in one word OUT; the high byte goes to the GRAPHICS_DATA I/O
; port.
;
MACRO SET_EGA_READ_MAP map
    mov   dx, GRAPHICS_1_2_ADDR
    mov   ax, (map SHL 8) OR GFX_READ_MAP_SELECT
    out   dx, ax
ENDM


;
; Load a `mode` byte into the Mode register [EGA, pg. 50] via the Graphics 1 & 2
; Address Register [EGA, pg. 46].
;
; Two different byte-sized registers are being written with a single word-sized
; OUT; the high byte goes to the GRAPHICS_DATA I/O port.
;
MACRO SET_EGA_GC_MODE mode
    mov   dx, GRAPHICS_1_2_ADDR
    mov   ax, (mode SHL 8) OR GFX_MODE
    out   dx, ax
ENDM


;
; Set the video mode to the specified mode number and initialize the EGA card.
;
; Also exists in the Cosmo code base in practically identical form.
;
; mode (word): The BIOS video mode to enter. Only the low byte is used.
; Returns: Nothing
; Registers destroyed: AX, DX
;
PROC _SetVideoMode FAR @@mode:WORD
    PUBLIC _SetVideoMode
    enter 0, 0

    ; Change video mode via BIOS video service interrupt [EGA, pg. 104].
    mov   ax, [@@mode]
    mov   ah, VSVC_SET_VIDEO_MODE
    int   INT_VIDEO_SERVICE

    ; Turn *on* Color Don't Care for all four color planes. This appears to be
    ; an instance where the IBM documentation is either confusing or flat-out
    ; incorrect -- bit value 1 means the Color Compare register is used during
    ; applicable memory read operations, and value 0 means the Color Compare
    ; register is ignored. This only affects memory read operations when Read
    ; Mode = 1, and the only place these types of reads occur is in the
    ; BlitMaskedTileWhiteFlash procedure.
    ; Two different byte-sized registers are being written with a single word-
    ; sized OUT; the high byte goes to the GRAPHICS_DATA I/O port.
    ; The bits in the high byte refer to planes 3-2-1-0.
    mov   dx, GRAPHICS_1_2_ADDR
    mov   ax, (0000b SHL 8) OR GFX_COLOR_DONT_CARE
    out   dx, ax

    ; Pre-select the Map Mask, but don't actually write anything to it yet.
    ; This probably isn't needed -- _usually_ any further changes to the map
    ; mask are accompanied by a re-select of this register.
    mov   dx, SEQUENCER_ADDR
    mov   al, SEQ_MAP_MASK
    out   dx, al

    pop   bp
    ret
ENDP


;
; Load the video border (overscan) register with the specified color value.
;
; This function also exists in the Cosmo code base, and is actually used in that
; game. Here in Duke Nukem II, it's unused.  Most likely it was left in as an
; oversight, or because it seemed like it might be useful to keep around.
;
; Returns: Nothing
; Registers destroyed: AX, BX
;
PROC _SetBorderColor FAR @@color_value:WORD
    PUBLIC _SetBorderColor
    enter 0, 0

    ; Change border color via BIOS video service interrupt [EGA, pg. 104].
    mov   ah, VSVC_PALETTE_REGISTERS
    mov   al, PALREG_SET_BORDER_COLOR
    mov   bx, [@@color_value]
    mov   bh, bl             ; Value in BL is not used again
    int   INT_VIDEO_SERVICE

    pop   bp
    ret
ENDP


;
; Draw a single 8x8 pixel solid tile to the current draw page.
;
; This procedure draws non-transparent tiles that are part of the game map and
; backdrops, as well as various UI elements for the HUD and menus.
;
; Source data is read from EGA memory at SOLID_TILE_SEGMENT:src_offset.
; The destination address is drawPageSegment:dest_offset.
; The EGA *must* be in latched write mode for this to work correctly.
;
; Each tile is an 8x8 pixel square. Each 8-pixel tile row occupies one byte of
; EGA address space (1 bit per pixel), for a total of 8 bytes per tile. These 8
; bytes are stored sequentially in the source memory, or at 40-byte intervals
; in the destination memory.
;
; Within the EGA, each memory read/write operation is quadrupled across the four
; color planes. Although only 8 bytes of address space are handled during each
; call to this procedure, 32 bytes of physical memory are copied internally.
;
; This also exists in Cosmo and is quite similar there. This version uses a
; combination of MOVSB and ADD instead of two MOVs with different offsets
; though. Presumably this is an optimization for smaller code size, and maybe
; speed as well.
;
; src_offset (word): Memory offset of the source tile. This value should always
;     be a multiple of 8.
; dst_offset (word): Memory offset to write to, relative to the current draw
;     page segment.
; Returns: Nothing
; Registers destroyed: AL, DX, ES
;
PROC _BlitSolidTile FAR @@src_offset:WORD, @@dst_offset:WORD
    PUBLIC _BlitSolidTile
    enter 0, 0
    push  ds
    push  di
    push  si

    ; Set up source and destination pointers from the arguments:
    ;   DS:SI <- Source tile data address (in EGA memory)
    ;   ES:DI <- Destination draw page address (in EGA memory)
    mov   dx, [drawPageSegment]
    mov   es, dx
    mov   dx, SOLID_TILE_SEGMENT
    mov   ds, dx
    ASSUME ds:NOTHING
    mov   si, [@@src_offset]
    mov   di, [@@dst_offset]
    mov   bx, 39

    ; Draw eight rows of tile pixels. All four color planes are copied, in
    ; parallel, through the EGA's internal latches. The memory read/write cycles
    ; are doing the actual work here.
    ; Latched write mode must be enabled!
    REPT 7
      movsb
      add   di, bx
    ENDM
    movsb

    pop   si
    pop   di
    pop   ds
    ASSUME ds:DGROUP
    pop   bp
    ret
ENDP


;
; Select video page to draw on (aka backbuffer)
;
; This function selects the video page that's targeted by the drawing functions
; in this file. Should be used in conjunction with SetDisplayPage to implement a
; double-buffering scheme.  Ideally, the draw page should never be set to the
; same page that's currently displayed on screen to avoid artifacts.  But the
; game doesn't always respect that.
;
; Also exists in the Cosmo codebase, albeit with an additional wrapper function
; that's not present here (compare UpdateDrawPageSegment and _SelectDrawPage
; in Cosmore's lowlevel.asm).
;
; Returns: Nothing
; Registers destroyed: AX, BX
;
PROC _SetDrawPage FAR @@page:WORD
    PUBLIC _SetDrawPage
    enter 0, 0

    ; drawPageSegment = EGA_SEGMENT + (drawPageNumber * 200h)
    mov   ax, [@@page]
    xchg  ah, al
    shl   ah, 1
    mov   bx, EGA_SEGMENT
    add   bx, ax
    mov   [drawPageSegment], bx

    pop   bp
    ret
ENDP


;
; Change the video page that is currently displayed on the screen.
;
; Although higher page numbers are accepted and will behave sensibly, pages
; beyond number 1 are used for tile storage and they will render as garbage if
; sent to the screen.
;
; Also exists in the Cosmo code base in practically identical form.
;
; page (word): Page number to show (0..n). Maximum page number varies based on
;     the video mode and installed adapter memory, but common values for the EGA
;     are 0, 1, 3, and 7. Only the low byte is used.
; Returns: Nothing
; Registers destroyed: AX
;
PROC _SetDisplayPage FAR @@page:WORD
    PUBLIC _SetDisplayPage
    enter 0, 0

    ; Change the active display page via BIOS video service interrupt [EGA,
    ; pg. 104].
    mov   ax, [@@page]
    mov   ah, VSVC_SET_ACTIVE_PAGE
    int   INT_VIDEO_SERVICE

    pop   bp
    ret
ENDP


;
; Draw a single 8x8 pixel sprite tile to the draw page.
;
; This procedure draws tiles that are part of actors, decorations, the player,
; and other non-map sprites, with transparency. It is also responsible for
; drawing some UI elements.
;
; The tile data contains five bit planes: mask, and 4 color planes.
; The mask plane comes first, followed by the least significant color bit plane,
; up to the most significant bit.
; Transparent areas have their mask bits set to 1.
;
; NOTE: Transparent areas in the tile *must* have the bits for all other color
; planes set to 0.
;
; The basic theory of operation is as follows:
; 1. Read the first four rows of mask bits from the source tile data. This
;    starts at the 0th byte of the tile data and repeats at 5-byte intervals.
;    Store these rows in four byte-width registers. (Ideally all eight rows
;    would be read, but the 286 does not have enough registers to do this. Four
;    rows are all that can be stored.)
; 2. For each of the four color planes:
;    a: Set the EGA map mask register to only apply memory write operations to
;       the current color plane being operated on.
;    b: Set the EGA read map select register to only load data from the color
;       plane being operated on during memory read operations.
;    c: For each pixel row in the tile:
;       A: Read one byte (i.e. eight pixels, or one tile row) from the EGA
;          memory to capture what's been written onto the current color plane in
;          the draw page thus far.
;       B: AND this with the mask data for the row. The first four rows are in
;          registers from step 1, the remaining rows must be read from the tile
;          source. This turns off (zeros) the pixels that are covered by an
;          opaque area of the tile.
;       C: OR the result with the color data from the tile source. This data is
;          located at 5-byte intervals, starting at an offset between 1 and 4
;          depending on the current color plane being operated on.
;       D: Write the result back to the original location in the EGA memory.
;          This updates one row of pixels on the current color plane.
;       E. Advance the read position by a 5-byte step, and advance the write
;          position by a 40-byte step, in preparation for the next pixel row.
;
; Also exists in the Cosmo code base and is fairly similar, but there are some
; differences in the inner loop, presumably to further optimize the code.
;
; src (far pointer): Memory address of the first byte of tile data to read.
; x (word): X-position on the screen, in tiles. (0..39, leftmost column is 0)
; y (word): Y-position on the screen, in tiles. (0..24, topmost row is 0)
; Returns: Nothing
; Registers destroyed: AX, BX, CX, DX, ES
;
PROC _BlitMaskedTile FAR @@src:FAR PTR, @@x:WORD, @@y:WORD
    PUBLIC _BlitMaskedTile
    enter 0, 0
    push  si
    push  di
    push  ds

    ; Set up source and destination pointers from the arguments:
    ;   DS:SI <- Source tile data address (could be anywhere in memory)
    ;   ES:DI <- Destination draw page address (in EGA memory)
    mov   di, [@@y]
    shl   di, 1
    mov   di, [yOffsetTable+di]

    add   di, [@@x]
    mov   ax, [drawPageSegment]
    lds   si, [@@src]
    ASSUME ds:NOTHING
    mov   es, ax

    ; Mask data uses eight bytes in each tile (one byte per pixel row), but
    ; there are only four byte-width registers available to hold this data.
    ; Load the first four rows of mask data into registers, starting from DS:SI
    ; and continuing in 5-byte steps, then accept the fact that the last four
    ; rows must be repeatedly re-read from slower memory.
    mov   bl, [si]
    mov   bh, [si+5]
    mov   cl, [si+10]
    mov   ch, [si+15]

    ; The actual data movement is largely the same across the four color
    ; planes, so use a repeating macro to handle it.
    IRP plane_num, <0, 1, 2, 3>
        ; Configure the Map Mask and enable only one of the four color planes.
        ; This restricts EGA memory writes to only the current color plane.
        SET_EGA_MAP_MASK <1 SHL plane_num>

        ; Configure the Read Map Select register, which makes future EGA memory
        ; reads only fetch data from the current color plane.
        SET_EGA_READ_MAP plane_num

        ; The first four rows of mask data are in byte registers, and the last
        ; four rows are still in memory at DS:SI+20 and 5-byte intervals beyond.
        ; The first row of color data is at DS:SI+plane_num+1, and subsequent
        ; rows are also spaced at 5-byte intervals.
        ;
        ; For each row of pixels in the tile, read the EGA memory contents for
        ; what's already been drawn on the preselected color plane. AND this
        ; with the mask data, so each pixel that is covered by an opaque area
        ; gets set to 0. OR the result with the pixel data for the tile, and
        ; write the result back into the original location in EGA memory.
        srcoff = 0

        IRP maskreg, <bl, bh, cl, ch, [si+20], [si+25], [si+30], [si+35]>
            mov   al, [es:di]
            and   al, maskreg
            or    al, [si+(plane_num + 1 + srcoff)]
            mov   [es:di], al

            srcoff = srcoff + MASKED_TILE_ROW_STRIDE

            IF srcoff NE 8 * MASKED_TILE_ROW_STRIDE
                add   di, SCREEN_Y_STRIDE
            ELSE
                ; The 8th row is handled specially: This SUB instruction undoes
                ; the ADDs done during the previous 7 rows, in order to get back
                ; to the first row for the next 8 row writes (which go to the
                ; next plane).  However, we don't need the SUB for the last
                ; plane, so we skip it if we've reached plane number 3 (which is
                ; the 4th plane).
                IF plane_num NE 3
                    sub   di, SCREEN_Y_STRIDE * 7
                ENDIF
            ENDIF
        ENDM
    ENDM

    pop   ds
    ASSUME ds:DGROUP
    pop   di
    pop   si
    pop   bp
    ret
ENDP


;
; This is basically like BlitMaskedTile, but the y coordinate is
; given in pixels instead of tiles.
;
; Only used for drawing the moving text in the Duke3D teaser screen.
;
; Unique to Duke Nukem II.
;
; src (far pointer): Memory address of the first byte of tile data to read.
; x (word): X-position on the screen, in tiles. (0..39, leftmost column is 0)
; y (word): Y-position on the screen, in pixels. (0..199, topmost row is 0)
; Returns: Nothing
; Registers destroyed: AX, BX, CX, DX, ES
;
PROC _BlitMaskedTile_FlexibleY FAR @@src:FAR PTR, @@x:WORD, @@y:WORD
    PUBLIC _BlitMaskedTile_FlexibleY
    enter 0, 0
    push  si
    push  di
    push  ds

    ; Set up source and destination pointers from the arguments:
    ;   DS:SI <- Source tile data address (could be anywhere in memory)
    ;   ES:DI <- Destination draw page address (in EGA memory)
    ; Unlike in BlitSolidTile, we don't use the yOffsetTable here, since the
    ; y coordinate is already in pixels. Instead, we simply multiply y by
    ; SCREEN_Y_STRIDE to get to the row's starting address.
    mov   ax, [@@y]
    mov   bx, SCREEN_Y_STRIDE
    mul   bx
    mov   di, ax

    add   di, [@@x]
    mov   ax, [drawPageSegment]
    lds   si, [@@src]
    ASSUME ds:NOTHING
    mov   es, ax

    ; The rest of this routine is identical to BlitSolidTile, so refer to
    ; that function for comments.
    mov   bl, [si]
    mov   bh, [si+5]
    mov   cl, [si+10]
    mov   ch, [si+15]

    IRP plane_num, <0, 1, 2, 3>
        SET_EGA_MAP_MASK <1 SHL plane_num>
        SET_EGA_READ_MAP plane_num

        srcoff = 0

        IRP maskreg, <bl, bh, cl, ch, [si+20], [si+25], [si+30], [si+35]>
            mov   al, [es:di]
            and   al, maskreg
            or    al, [si+(plane_num + 1 + srcoff)]
            mov   [es:di], al

            srcoff = srcoff + MASKED_TILE_ROW_STRIDE

            IF srcoff NE 8 * MASKED_TILE_ROW_STRIDE
                add   di, SCREEN_Y_STRIDE
            ELSE
                IF plane_num NE 3
                    sub   di, SCREEN_Y_STRIDE * 7
                ENDIF
            ENDIF
        ENDM
    ENDM

    pop   ds
    ASSUME ds:DGROUP
    pop   di
    pop   si
    pop   bp
    ret
ENDP


;
; Draw a single 8x8 pixel monochrome font tile to the current draw page
;
; This function is used to render glyphs from the large font used in the menus,
; which can appear in different colors. This only draws to one of the 4 EGA
; color planes, as specified in the plane argument. To make a glyph appear in a
; specific color, this function needs to be invoked once for each plane that has
; a 1 bit set in the color index.
;
; Unique to Duke Nukem II.
;
; src (far pointer): Memory address of the first byte of font data to read.
; x (word): X-position on the screen, in tiles. (0..39, leftmost column is 0)
; y (word): Y-position on the screen, in tiles. (0..24, topmost row is 0)
; plane (word): Target color plane to write to
; Returns: Nothing
; Registers destroyed: AX, BX, CX, DX, ES
;
PROC _BlitFontTile FAR @@src:FAR PTR, @@x:WORD, @@y:WORD, @@plane:WORD
    PUBLIC _BlitFontTile
    enter 0, 0
    push  si
    push  di
    push  ds

    mov   cx, [@@plane]

    ; Set up source and destination pointers from the arguments:
    ;   DS:SI <- Source tile data address (could be anywhere in memory)
    ;   ES:DI <- Destination draw page address (in EGA memory)
    mov   di, [@@y]
    shl   di, 1
    mov   di, [yOffsetTable+di]

    add   di, [@@x]
    mov   ax, [drawPageSegment]
    lds   si, [@@src]
    ASSUME ds:NOTHING
    mov   es, ax

    ; Select the EGA bit plane that was passed in `plane` for writing by
    ; configuring the map mask.
    mov   dx, SEQUENCER_ADDR
    mov   ax, 1h SHL 8
    shl   ax, cl
    add   ax, SEQ_MAP_MASK
    out   dx, ax

    ; Select the EGA bit plane that was passed in `plane` for reading by
    ; configuring the read map.
    mov   ax, 1h SHL 8
    mul   cl
    add   ax, GFX_READ_MAP_SELECT
    mov   dx, GRAPHICS_1_2_ADDR
    out   dx, ax

    ; Write 8 rows of pixels. This works very similarly to BlitMaskedTile,
    ; except that the layout of the source data is different. Since font glyphs
    ; are monochrome, each tile consists only of 2 bit planes, not 5 like for
    ; sprite tiles. The first plane stores a mask, the 2nd one stores the pixel
    ; data. Rows are 2 bytes apart in the source data. Hence, the mask for each
    ; row is at DS:SI + row * 2, and the data is at DS:SI + row * 2 + 1.
    row = 0
    REPT 8
        mov   al, [es:di]
        and   al, [si+(row * 2)]
        or    al, [si+(row * 2 + 1)]
        mov   [es:di], al

        row = row + 1

        IF row NE 8
            add   di, SCREEN_Y_STRIDE
        ELSE
            ; This is unnecessary, since DI isn't used after the end of the
            ; write loop.
            sub   di, SCREEN_Y_STRIDE * 7
        ENDIF
    ENDM

    pop   ds
    ASSUME ds:DGROUP
    pop   di
    pop   si
    pop   bp
    ret
ENDP


;
; Apply water effect (color change) to already drawn 8x8 pixel block
;
; This function turns all pixels in the specified 8x8 block within the current
; draw page into shades of blue (plus dark green), making it look like the block
; is under water.
;
; Unique to Duke Nukem II.
;
; x (word): X-position on the screen, in tiles. (0..39, leftmost column is 0)
; y (word): Y-position on the screen, in tiles. (0..24, topmost row is 0)
; Returns: Nothing
; Registers destroyed: AX, ES
;
PROC _ApplyWaterEffect FAR @@x:WORD, @@y:WORD
    PUBLIC _ApplyWaterEffect
    enter 0, 0
    push  di
    push  ds

    ; Load destination draw page address (in EGA memory) into ES:DI
    mov   di, [@@y]
    shl   di, 1
    mov   di, [yOffsetTable+di]

    add   di, [@@x]
    mov   ax, [drawPageSegment]
    ASSUME ds:NOTHING
    mov   es, ax

    ; The water effect works by remapping all color values to the range (8..11),
    ; which contains three shades of blue plus a dark green in the in-game
    ; palette. Conceptually, assuming linear memory, this can be achieved with
    ; the following C code:
    ;
    ;   *dest = *dest % 4 + 8
    ;
    ; But we're dealing with the EGA's planar memory here, so the approach needs
    ; to be different if we want to be efficient. First off, let's express our
    ; logic using bitwise operations instead of arithmetic. Doing a modulo with
    ; a power of of two value is equivalent to doing an AND with the value - 1,
    ; i.e. AND 3 in this case. And we can express the addition as a logical OR
    ; instead. So that gives us:
    ;
    ;   *dest = (*dest & 3) | 8;
    ;
    ; Now we need to apply these using the EGA hardware. We first write a 0 to
    ; plane 2, effectively unsetting the 3rd bit. This has the same effect as if
    ; we would AND each pixel with 1011b. But we will replace the 4th bit in the
    ; next step, so we can ignore it's initial value. Thus, this operation is
    ; basically equivalent to the AND with 3 described above.
    ;
    ; Next, we write a byte of all 1s to plane 3, setting the most significant
    ; bit in each pixel. This has the same effect as OR-ing the value with
    ; 1000b, aka 8.
    ;
    ; This way, we can achieve the desired color remapping by doing just 16
    ; writes to EGA memory, and no reads, which is pretty good.
    IRP plane_num, <2, 3>
        SET_EGA_MAP_MASK <1 SHL plane_num>
        SET_EGA_READ_MAP plane_num

        ; Write all 0s to plane 2, and all 1s to plane 3 (see above).
        IF plane_num EQ 2
            mov   al, 0
        ELSE
            mov   al, 0ffh
        ENDIF

        ; Write value 8 times to cover 8 rows of pixels
        destoffset = 0
        REPT 8
            mov   [es:di + destoffset], al
            destoffset = destoffset + SCREEN_Y_STRIDE
        ENDM
    ENDM

    pop   ds
    ASSUME ds:DGROUP
    pop   di
    pop   bp
    ret
ENDP


;
; Apply water effect with animated surface (wave left)
;
; This is like ApplyWaterEffect, but it applies a wave pattern to the top 2 rows
; of the block instead of filling it completely. This is used to draw an
; animated surface on top of the water.
;
; Unique to Duke Nukem II.
;
; x (word): X-position on the screen, in tiles. (0..39, leftmost column is 0)
; y (word): Y-position on the screen, in tiles. (0..24, topmost row is 0)
; Returns: Nothing
; Registers destroyed: AX, ES
;
PROC _ApplyWaterEffectWave0 FAR @@x:WORD, @@y:WORD
    PUBLIC _ApplyWaterEffectWave0

    enter 0, 0
    push  di
    push  ds

    ; Load destination draw page address (in EGA memory) into ES:DI
    mov   di, [@@y]
    shl   di, 1
    mov   di, [yOffsetTable+di]

    add   di, [@@x]
    mov   ax, [drawPageSegment]
    ASSUME ds:NOTHING
    mov   es, ax

    ; The general principle here is the same as in ApplyWaterEffect, but the
    ; first two rows are more involved in order to create the wave pattern.  The
    ; pattern we want to create is:
    ;
    ; _ X X _ _ _ _ _
    ; X X X X X _ _ X
    ;
    ; To do this, we have to first read the data present at the destination,
    ; apply bitwise operations on the CPU, and then write it back.  This allows
    ; us to use a bit pattern which makes it so that the effect applies only to
    ; some of the pixels, instead of all of them.  For plane 2, the bit pattern
    ; has 0s in all the locations matching the pixels we want to change, for
    ; plane 3, it's the inverse. The end result is that we still unset the bits
    ; on plane 2 and set the bits on plane 3, just as we do in the regular
    ; ApplyWaterEffect, but limited to the pixels we want to change.
    IRP plane_num, <2, 3>
        SET_EGA_MAP_MASK <1 SHL plane_num>
        SET_EGA_READ_MAP plane_num

        IF plane_num EQ 2
            ; Unset bits on plane 2, for the desired pixels only

            ; 1st row
            mov   al, [es:di]
            and   al, 10011111b
            mov   [es:di], al

            ; 2nd row
            mov   al, [es:di + SCREEN_Y_STRIDE]
            and   al, 00000110b
            mov   [es:di + SCREEN_Y_STRIDE], al

            ; value for remaining rows
            mov   al, 0
        ELSE
            ; Set bits on plane 3, for the desired pixels only

            ; 1st row
            mov   al, [es:di]
            or    al, 01100000b
            mov   [es:di], al

            ; 2nd row
            mov   al, [es:di + SCREEN_Y_STRIDE]
            or    al, 11111001b
            mov   [es:di + SCREEN_Y_STRIDE], al

            ; value for remaining rows
            mov   al, 0FFh
        ENDIF

        ; For the remaining 6 rows, operate just as in ApplyWaterEffect.
        destoffset = SCREEN_Y_STRIDE * 2
        REPT 6
            mov   [es:di + destoffset], al
            destoffset = destoffset + SCREEN_Y_STRIDE
        ENDM
    ENDM

    pop   ds
    ASSUME ds:DGROUP
    pop   di
    pop   bp
    ret
ENDP


;
; Apply water effect with animated surface (calm surface)
;
; This is like ApplyWaterEffect, but it skips the top-most row of pixels
; in the block instead of filling it completely. This is used to draw an animated
; surface on top of the water.
;
; Unique to Duke Nukem II.
;
; x (word): X-position on the screen, in tiles. (0..39, leftmost column is 0)
; y (word): Y-position on the screen, in tiles. (0..24, topmost row is 0)
; Returns: Nothing
; Registers destroyed: AX, ES
;
PROC _ApplyWaterEffectWave1 FAR @@x:WORD, @@y:WORD
    PUBLIC _ApplyWaterEffectWave1
    enter 0, 0
    push  di
    push  ds

    ; Load destination draw page address (in EGA memory) into ES:DI
    mov   di, [@@y]
    shl   di, 1
    mov   di, [yOffsetTable+di]

    add   di, [@@x]
    mov   ax, [drawPageSegment]
    ASSUME ds:NOTHING
    mov   es, ax

    ; The code here is identical to ApplyWaterEffect, except that the 1st row of
    ; pixels in the destination block is skipped.
    IRP plane_num, <2, 3>
        SET_EGA_MAP_MASK <1 SHL plane_num>
        SET_EGA_READ_MAP plane_num

        IF plane_num EQ 2
            mov   al, 0
        ELSE
            mov   al, 0FFh
        ENDIF

        ; We're skipping the first row
        destoffset = SCREEN_Y_STRIDE
        REPT 7
            mov   [es:di + destoffset], al
            destoffset = destoffset + SCREEN_Y_STRIDE
        ENDM
    ENDM

    pop   ds
    ASSUME ds:DGROUP
    pop   di
    pop   bp
    ret
ENDP


;
; Apply water effect with animated surface (wave right)
;
; This is like ApplyWaterEffect, but it applies a wave pattern to the top 2 rows
; of the block instead of filling it completely. This is used to draw an
; animated surface on top of the water.
;
; Unique to Duke Nukem II.
;
; x (word): X-position on the screen, in tiles. (0..39, leftmost column is 0)
; y (word): Y-position on the screen, in tiles. (0..24, topmost row is 0)
; Returns: Nothing
; Registers destroyed: AX, ES
;
PROC _ApplyWaterEffectWave2 FAR @@x:WORD, @@y:WORD
    PUBLIC _ApplyWaterEffectWave2

    enter 0, 0
    push  di
    push  ds

    ; Load destination draw page address (in EGA memory) into ES:DI
    mov   di, [@@y]
    shl   di, 1
    mov   di, [yOffsetTable+di]

    add   di, [@@x]
    mov   ax, [drawPageSegment]
    ASSUME ds:NOTHING
    mov   es, ax

    ; This function is the same as ApplyWaterEffectWave0, only the bit patterns
    ; are different in order to create a mirror image of the shape created in
    ; the former.
    IRP plane_num, <2, 3>
        SET_EGA_MAP_MASK <1 SHL plane_num>
        SET_EGA_READ_MAP plane_num

        IF plane_num EQ 2
            mov   al, [es:di + SCREEN_Y_STRIDE]
            and   al, 01100000b
            mov   [es:di + SCREEN_Y_STRIDE], al

            mov   al, [es:di]
            and   al, 11111001b
            mov   [es:di], al

            mov   al, 0
        ELSE
            mov   al, [es:di + SCREEN_Y_STRIDE]
            or    al, 10011111b
            mov   [es:di + SCREEN_Y_STRIDE], al

            mov   al, [es:di]
            or    al, 00000110b
            mov   [es:di], al

            mov   al, 0FFh
        ENDIF

        destoffset = SCREEN_Y_STRIDE * 2
        REPT 6
            mov   [es:di + destoffset], al
            destoffset = destoffset + SCREEN_Y_STRIDE
        ENDM
    ENDM

    pop   ds
    ASSUME ds:DGROUP
    pop   di
    pop   bp
    ret
ENDP


;
; Draw a single 8x8 pixel sprite tile, with a translucency effect
;
; This function is used to implement the effect used when Duke has the cloaking
; device. It works just like BlitMaskedTile, but by operating only on one of the
; 4 planes, it achieves an interesting color blend effect.  I'd recommend
; reading the comments in BlitMaskedTile first to get an idea of the basic
; principle of drawing masked sprite tiles.
;
; Unique to Duke Nukem II. Cosmo also features a translucency effect, but it
; works differently.
;
; src (far pointer): Memory address of the first byte of tile data to read.
; x (word): X-position on the screen, in tiles. (0..39, leftmost column is 0)
; y (word): Y-position on the screen, in tiles. (0..24, topmost row is 0)
; Returns: Nothing
; Registers destroyed: AX, BX, CX, DX, ES
;
PROC _BlitMaskedTileTranslucent FAR @@src:FAR PTR, @@x:WORD, @@y:WORD
    PUBLIC _BlitMaskedTileTranslucent
    enter 0, 0
    push  si
    push  di
    push  ds

    ; Set up source and destination pointers from the arguments:
    ;   DS:SI <- Source tile data address (could be anywhere in memory)
    ;   ES:DI <- Destination draw page address (in EGA memory)
    mov   di, [@@y]
    shl   di, 1
    mov   di, [yOffsetTable+di]
    add   di, [@@x]
    mov   ax, [drawPageSegment]
    lds   si, [@@src]
    ASSUME ds:NOTHING
    mov   es, ax

    ; Mask data uses eight bytes in each tile (one byte per pixel row), but
    ; there are only four byte-width registers available to hold this data.
    ; Load the first four rows of mask data into registers, starting from DS:SI
    ; and continuing in 5-byte steps, then accept the fact that the last four
    ; rows must be repeatedly re-read from slower memory.
    mov   bl, [si]
    mov   bh, [si+5]
    mov   cl, [si+10]
    mov   ch, [si+15]

    ; Write only to the 2nd plane
    SET_EGA_MAP_MASK <1 SHL 1>

    ; Read only from the 2nd plane
    SET_EGA_READ_MAP 1

    ; This does exactly the same thing as the read/write loop in BlitMaskedTile,
    ; but only for one plane. Specifically, the 2nd plane in video memory is
    ; read & written, and the 1st plane of the sprite data is used for OR-ing
    ; the framebuffer contents. Importantly, the AND operation for applying the
    ; sprite mask also only applies to the 2nd plane!  The result is that for
    ; each pixel covered by a sprite pixel, the 2nd bit is replaced with the
    ; sprite color's 1st bit. Expressed in C code (with dest and src being
    ; pointers to color indices in linear form, i.e. not planar):
    ;
    ;   *dest = (*dest & ~2) | ((*src & 1) << 1);
    srcoff = 0
    dstoff = 0
    IRP maskreg, <bl, bh, cl, ch, [si+20], [si+25], [si+30], [si+35]>
        mov   al, [es:di + dstoff]
        and   al, maskreg
        or    al, [si+(1 + srcoff)]
        mov   [es:di + dstoff], al

        srcoff = srcoff + MASKED_TILE_ROW_STRIDE
        dstoff = dstoff + SCREEN_Y_STRIDE
    ENDM

    pop   ds
    ASSUME ds:DGROUP
    pop   di
    pop   si
    pop   bp
    ret
ENDP


;
; Draw a single 8x8 pixel masked tile to the draw page.
;
; This procedure draws tiles that are part of the game map, with transparency.
;
; This works almost identically to BlitSolidTile, so the comments here aren't
; anywhere near as thorough. Consult the other procedure for a more detailed
; description of what the code here is doing.
;
; These are the differences between this procedure and BlitSolidTile:
; - The pointer passed in the `src` argument is subtracted by 8,000. This
;   unexpected and frankly dangerous behavior is due to the split between solid
;   and masked tiles in the map data. Very briefly, solid tiles are represented
;   in the map data by values 0..7,999, and masked tiles are represented by
;   values 8,000..14,400. The C code uses 8,000 as a split point to decide
;   which procedure should be used to draw each tile. Unfortunately, it passes
;   the raw map value in `src` without correcting for the masked tile offset.
;   The subtraction of 8,000 normalizes the value and allows all masked tile
;   memory to be read.
; - Before drawing begins, the EGA registers are reprogrammed to ensure that
;   data flows from the processor directly to the EGA memory during write
;   operations, without the internal latches interfering.
; - After drawing is done, the map mask is explicitly set to binary 1111, making
;   future EGA memory reads affect all four color planes in parallel,
;   and the EGA registers are once again reprogrammed to allow the internal
;   latches to influence the data written by the processor.
;   All of this restores the EGA to the state needed for BlitSolidTile.
;
; Also exists in the Cosmo code base in a very similar form. The differences
; are that Cosmo has a different cut-off point for the start of masked tiles
; (at 16,000 instead of 8,0000), and that the function takes separate x and y
; coordinates in Cosmo.
;
; src (far pointer): Memory address of the first byte of tile data to read
;     *plus* a constant offset of 8,000. In other words, if the intention is to
;     read the 0th byte in memory, src must be a pointer at offset 8,000.
; dest (word): Memory offset to write to, relative to the current draw
;     page segment.
; Returns: Nothing
; Registers destroyed: AX, BX, CX, DX, ES
;
PROC _BlitMaskedMapTile FAR @@src:FAR PTR, @@dest:WORD
    PUBLIC _BlitMaskedMapTile
    enter 0, 0
    push  si
    push  di
    push  ds

    ; Set up source and destination pointers from the arguments:
    ;   DS:SI <- Source tile data address (could be anywhere in memory)
    ;   ES:DI <- Destination draw page address (in EGA memory)
    ; NOTE: The source address ends up being 8,000 bytes below what was
    ; passed in the `src` argument.
    mov   di, [@@dest]
    mov   ax, [drawPageSegment]
    lds   si, [@@src]
    ASSUME ds:NOTHING
    sub   si, 8000
    mov   es, ax

    ; Configure the Mode register by zeroing everything out.
    ;   Bits     | Meaning
    ;   ---------+--------
    ;   ......00 | Write Mode = Each memory plane is written with the
    ;            |     processor data.
    ;   .....0.. | Test Condition = off
    ;   ....0... | Read Mode = The processor reads data from the memory
    ;            |     plane selected by the read map select register.
    ;   ...0.... | Odd/Even = off
    ;   ..0..... | Shift Register = 0
    ;   00...... | Not Used
    ; The only seemingly important bits are those for Write Mode. This
    ; procedure requires memory to be written with processor data (as
    ; opposed to EGA latch data) and the correct mode must be ensured.
    SET_EGA_GC_MODE 000000b

    ; Store first four rows of mask data.
    mov   bl, [si]
    mov   bh, [si+5]
    mov   cl, [si+10]
    mov   ch, [si+15]

    ; Repeating macro for data movement.
    IRP plane_num, <0, 1, 2, 3>
        ; Configure the Map Mask and enable only the current color plane.
        SET_EGA_MAP_MASK <1 SHL plane_num>

        ; Configure the Read Map Select register to isolate current color plane.
        SET_EGA_READ_MAP plane_num

        srcoff = 0
        dstoff = 0

        ; Draw eight rows for one color plane.
        IRP maskreg, <bl, bh, cl, ch, [si+20], [si+25], [si+30], [si+35]>
            mov   al, [es:di+dstoff]
            and   al, maskreg
            or    al, [si+(plane_num + 1 + srcoff)]
            mov   [es:di+dstoff], al
            srcoff = srcoff + MASKED_TILE_ROW_STRIDE
            dstoff = dstoff + SCREEN_Y_STRIDE
        ENDM
    ENDM

    ; Reset the map mask register to write to all four color planes.
    SET_EGA_MAP_MASK 1111b  ; Bits are planes 3210

    ; Reset the Write Mode value in the Mode register.
    ;   Bits     | Meaning
    ;   ---------+--------
    ;   ......01 | Write Mode = Each memory plane is written with the
    ;            |     contents of the processor latches. These latches are
    ;            |     loaded by a processor read operation.
    ; The other bits retain the same value (and meaning) as above.
    SET_EGA_GC_MODE 000001b

    pop   ds
    ASSUME ds:DGROUP
    pop   di
    pop   si
    pop   bp
    ret
ENDP


;
; Draw a single 8x8 pixel sprite tile, as a white shape, to the draw page.
;
; This procedure draws tiles as solid white shapes, to represent actors that are
; taking damage or to get the player's attention.
;
; NOTE: This assumes port 3C4h has been set to 2h (sequencer index = map mask).
; This is the only state that the game ever leaves the register in, but it's an
; unsafe assumption.
;
; The basic theory of operation is as follows.
; 1. Program the EGA registers to accept the upcoming drawing technique. This
;    procedure uses an approach not seen in any other area of the game, so the
;    setup is a little unfamiliar in some places:
;    a. Set the Map Mask register to all 1's. This sets it so that any bits that
;       the processor sets to 1 will apply to all four color planes, resulting
;       in white.
;    b. Set the Function Select value to OR any data written by the processor
;       against the EGA latches. This allows the processor to write 1 bits and
;       set the color to white, and write 0 bits to retain the current color,
;       whatever it might be.
;    c. Set the Read Mode value to combine the value of all four color planes
;       during memory reads. Considering the value already in the Color Don't
;       Care register (set in the SetVideoMode procedure), this is a rather
;       convoluted way to ensure the EGA returns FFh for all memory reads.
; 2. For each pixel row in the tile:
;    a. Read one byte (i.e. eight pixels, or one tile row) from the source
;       tile's mask data.
;    b. Invert the bits of the mask data, so each uncovered pixel has a 0 bit
;       while covered/opaque areas are 1 bits.
;    c. AND this value with the contents of the EGA memory. This causes a memory
;       read which sets the EGA's internal latches and returns FFh. (See step
;       1c for the cause of this behavior.) AND against FFh doesn't have any
;       interesting effects, so the result is the mask data being written
;       directly into memory. Written 1 bits become white, and 0 bits retain the
;       latched value and, consequently, the previous color.
;    d. Advance the read position by a 5-byte step, and advance the write
;       position by a 40-byte step, in preparation for the next pixel row.
; 3. Reset the Function Select and Read Mode values to their natural state.
;
; The tile data contains five bit planes -- mask, and 4 color planes, in that
; order --- with 1-bit color depth on each plane. Transparent areas have their
; mask bits set to 1.
;
; Also exists in the Cosmo code base in practically identical form.
;
; src (far pointer): Memory address of the first byte of tile data to read.
; x (word): X-position on the screen, in tiles. (0..39, leftmost column is 0)
; y (word): Y-position on the screen, in tiles. (0..24, topmost row is 0)
; Returns: Nothing
; Registers destroyed: AX, DX, ES
;
PROC _BlitMaskedTileWhiteFlash FAR @@src:FAR PTR, @@x:WORD, @@y:WORD
    PUBLIC _BlitMaskedTileWhiteFlash
    enter 0, 0
    push  si
    push  di
    push  ds

    ; Set up source and destination pointers from the arguments:
    ;   DS:SI <- Source tile data address (could be anywhere in memory)
    ;   ES:DI <- Destination draw page address (in EGA memory)
    mov   di, [@@y]
    shl   di, 1
    mov   di, [yOffsetTable+di]

    add   di, [@@x]
    mov   ax, [drawPageSegment]
    lds   si, [@@src]
    ASSUME ds:NOTHING
    mov   es, ax

    ; Program the EGA Map Mask [EGA, pg. 20] to enable writing to all four color
    ; planes in parallel. NOTE: This is making an *unsafe* assumption that the
    ; value SEQ_MAP_MASK has been previously loaded into the SEQUENCER_ADDR I/O
    ; port. The assumption seems to not cause issues in the game.
    mov   dx, SEQUENCER_DATA
    mov   al, 1111b          ; Bits are planes 3210
    out   dx, al

    ; Select the Data Rotate (Function Select) register [EGA, pg. 49] via the
    ; Graphics 1 & 2 Address Register [EGA, pg. 46] and set the Function Select
    ; bits. Two different byte-sized registers are being written with a single
    ; word-sized OUT; the high byte goes to the GRAPHICS_DATA I/O port.
    ;   Bits     | Meaning
    ;   ---------+--------
    ;   .....000 | Rotate Count = 0
    ;   ...10... | Function Select = Data written to memory OR'ed with data
    ;            |     already in the latches.
    ;   000..... | Not Used
    ; The only parameter being changed here is Function Select, which is
    ; programmed to OR any data written by the processor with the contents of
    ; the EGA's internal latches. Concretely, this means that the memory bits
    ; will get unconditionally set to 1 where the processor writes a 1, and the
    ; memory bits will retain their latched value (whatever they may have been)
    ; where the processor writes a 0.
    mov   dx, GRAPHICS_1_2_ADDR
    mov   ax, (10000b SHL 8) OR GFX_DATA_ROTATE
    out   dx, ax

    ; Next move to the Mode register [EGA, pg. 50] and set the Read Mode bit. As
    ; before, two bytes are being written with one word OUT.
    ;   Bits     | Meaning
    ;   ---------+--------
    ;   ......00 | Write Mode = Each memory plane is written with the
    ;            |     processor data.
    ;   .....0.. | Test Condition = off
    ;   ....1... | Read Mode = The processor reads the results of the
    ;            |     comparison of the 4 memory planes and the color
    ;            |     compare register.
    ;   ...0.... | Odd/Even = off
    ;   ..0..... | Shift Register = 0
    ;   00...... | Not Used
    ; The only parameter being changed here is Read Mode, which is programmed to
    ; combine bits from all four planes into one result during processor read
    ; operations. Normally this mode is used to test screen pixels against the
    ; Color Compare register, but the global initialization of Color Don't Care
    ; in the SetVideoMode procedure rendered this test inert. The combination of
    ; all these factors means that all subsequent memory reads will return
    ; binary 1 in every position, regardless of what colors or images are
    ; presently in memory.
    mov   ax, (001000b SHL 8) OR GFX_MODE
    out   dx, ax

    srcpos = 0
    dstpos = 0

    ; The source tile's mask data is located in memory at DS:SI and 5-byte
    ; intervals beyond.
    ;
    ; For each row of pixels in the tile, read one row (8 bits) of mask data
    ; from the source and invert the bits to normalize the representation.  The
    ; result is a 1 bit in each location where the tile covers the background.
    ; AND this with the EGA memory contents to apply the change.
    ;
    ; The underlying behavior here is unintuitive and perhaps a bit surprising.
    ; Because of the way the EGA Read Mode and Color Don't Care registers were
    ; programmed above, every EGA memory read (including the one implicit in the
    ; AND instruction) returns FFh in every case. AND against FFh is doing the
    ; same thing as MOV would have, with one important exception: The AND sets
    ; the EGA latch state by reading from memory first. That's important here.
    ;
    ; On the write side, the Function Select value specifies that each bit
    ; written should be OR'd with the value presently in the EGA latches.
    ; Wherever the processor writes a 1 bit, the 1 value prevails. Wherever the
    ; processor writes a 0 bit, that value is OR'd with the latch contents and
    ; the latched value remains. The Map Mask register value specifies that all
    ; four color planes are affected during writes, which causes a white pixel
    ; to be created where the processor writes a 1 bit, and the original color
    ; to be maintained at each 0 bit.
    REPT 8
        mov   al, [si+srcpos]
        not   al
        and   [es:di+dstpos], al
        srcpos = srcpos + MASKED_TILE_ROW_STRIDE
        dstpos = dstpos + SCREEN_Y_STRIDE
    ENDM

    ; Reset the Function Select value in the Data Rotate (Function Select)
    ; register.
    ;   Bits     | Meaning
    ;   ---------+--------
    ;   ...00... | Function Select = Data written to memory is unmodified.
    ; The other bits retain the same value (and meaning) as above.
    mov   ax, (00000b SHL 8) OR GFX_DATA_ROTATE
    out   dx, ax

    ; Reset the Read Mode value in the Mode register.
    ;   Bits     | Meaning
    ;   ---------+--------
    ;   ....0... | Read Mode = The processor reads data from the memory
    ;            |     plane selected by the read map select register.
    ; The other bits retain the same value (and meaning) as above.
    mov   ax, (000000b SHL 8) OR GFX_MODE
    out   dx, ax

    pop   ds
    ASSUME ds:DGROUP
    pop   di
    pop   si
    pop   bp
    ret
ENDP


;
; Set a single pixel at the given pixel coordinates to the specified color
;
; The map mask must be set to allow writes to all planes at the same time for
; this to work correctly. This isn't always the case in the game.
;
; Unique to Duke Nukem II.
;
; x (word): X-position on the screen, in pixels. (0..319, leftmost column is 0)
; y (word): Y-position on the screen, in pixels. (0..199, topmost row is 0)
; color (byte): color index to set the pixel to. (0..15)
; Returns: Nothing
; Registers destroyed: AX, BX, CX, DX, ES
;
PROC _SetPixel FAR @@x:WORD, @@y:WORD, @@color:BYTE
    PUBLIC _SetPixel
    enter 0, 0

    mov   ax, [@@y]
    mov   bx, [@@x]
    mov   cl, bl

    ; Determine the byte address of the target pixel in video memory. This is
    ; basically doing BX = x / 8 + y * SCREEN_Y_STRIDE.
    mov   dx, SCREEN_Y_STRIDE
    mul   dx
    shr   bx, 1
    shr   bx, 1
    shr   bx, 1
    add   bx, ax

    ; Load ES with current draw page segment. After this, ES:BX points to the
    ; byte address in video memory holding the target pixel.
    mov   ax, [drawPageSegment]
    mov   es, ax

    ; Now set a bitmask. Individual pixels can't be addressed directly in EGA
    ; memory, since each byte holds the data for 8 pixels (one bit plane).
    ; To change a single pixel therefore requires bit-wise operations. Here,
    ; we're using the EGA's bitmask feature to write only to the target pixel.
    ;
    ; CL was set to the low byte of the x parameter further up. This means it
    ; currently holds (x % 256). We now need to determine the bitmask that will
    ; address the desired pixel within the byte that's addressed by BX.
    ; AND-ing CL with 7 is equivalent to doing CL % 8, so that gives us the
    ; index of the pixel within the byte. XOR-ing with 7 in this case has the
    ; same effect as doing (7 - CL) - it basically inverts the index, turning
    ; 0 into 7, 1 into 6, etc.
    ; Finally, we store (1 << CL) in AH, which creates a single bit that
    ; corresponds to the pixel's location within the byte.  This is the bitmask
    ; we need.
    and   cl, 7
    xor   cl, 7
    mov   ah, 1
    shl   ah, cl

    ; Apply the bitmask that we've just computed
    mov   dx, GRAPHICS_1_2_ADDR
    mov   al, GFX_BIT_MASK
    out   dx, ax

    ; Read from video memory to fill up the EGA latches. This is important to
    ; make sure that other pixels than the one we want to modify retain their
    ; values. When writing to the target address, the bitmask causes bits that
    ; aren't changed to be loaded from the latches instead of using the
    ; CPU-supplied value.
    mov   al, [es:bx]

    ; Zero out the pixel. This only works correctly if the map mask is set to
    ; write to all planes.
    mov   [byte ptr es:bx], 0

    ; The rest of the routine only changes DL in order to target different EGA
    ; registers. DH still holds 03h from when we set the bitmask above, so we
    ; don't need to set it again.
    mov   dl, 0C4h ; Sequencer address register

    ; Select map mask and write `color` into the data register with a single
    ; word OUT. This sets the map mask to enable only those planes which have
    ; their corresponding bits set in the desired color index.
    mov   ah, [@@color]
    mov   al, SEQ_MAP_MASK
    out   dx, ax

    ; Write all 1s into the target pixel, thanks to the map mask we've just set
    ; this causes the pixel to take on the color we want to set, since only
    ; the planes are written to which have a 1 bit in the color value.
    mov   [byte ptr es:bx], 0FFh

    ; Reset the map mask to enable writes to all planes. AL still holds
    ; SEQ_MAP_MASK. This restores the initial state required by this function.
    mov   ah, 0Fh
    out   dx, ax

    ; Reset the EGA's bit mask back to its default value, undoing the bit mask
    ; change we did earlier.
    mov   dl, 0CEh ; Graphics 1/2 address register
    mov   ax, (0FFh SHL 8) OR GFX_BIT_MASK
    out   dx, ax

    ; Set all 0s in the Enable Set/Reset register. I'm not sure why this is
    ; here, since this function doesn't modify this register in any way. I can
    ; only imagine that this code was copied or adapted from some other source,
    ; where this register change made sense in the original context, and the
    ; authors left it in.
    mov   ax, (00h SHL 8) OR GFX_ENABLE_SET_RESET
    out   dx, ax

    mov   sp, bp
    pop   bp
    retf
ENDP


;
; Wait for the beginning of the CRT's next vertical blanking interval
;
; This function is extremely similar to VL_WaitVBL from the Wolfenstein 3D
; source (in ID_VL_A.ASM). The one difference is that the number of loop
; iterations is hardcoded to 1, instead of being a parameter.
;
; Unique to Duke Nukem II.
;
; Returns: Nothing
; Registers destroyed: AX, CX, DX
;
PROC _AwaitVblank FAR
    PUBLIC _AwaitVblank

    mov   dx, INPUT_STATUS_1_ADDR

    ; Set iteration count for the LOOP instruction below.  Since this is
    ; hardcoded to 1 iteration, the entire loop (label, LOOP instruction,
    ; setting CX) could be removed.
    mov   cx, 1

repeat:

    ; First, wait for a currently ongoing vertical retrace to end.  Reading the
    ; Input Status #1 register returns a value with bit 3 ( = 8) indicating if
    ; the display is currently in the vertical blanking interval. Here, we keep
    ; reading the status register until the bit becomes 0.
await_vblank_end:
    in    al, dx
    test  al, 8
    jnz   await_vblank_end

    ; Now wait for the begin of the next vertical retrace.  Same thing as above,
    ; but we keep looping as long as the bit is not set (i.e., is 0).
await_vblank_begin:
    in    al, dx
    test  al, 8
    jz    await_vblank_begin

    ; Since CX was set to 1 above, this LOOP will always be a no-op.
    loop  repeat
    ret
ENDP

ENDS
