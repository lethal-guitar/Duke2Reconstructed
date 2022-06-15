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

#ifndef DIGISND_H
#define DIGISND_H


// Some of the declarations in this file seem to be slightly different between
// the library's code itself, and the game code which makes use of the library.
// We use a define here so that we can have these differences without needing
// two separate files.
#ifdef SNDLIB_SRC_BUILD

#define NULL 0

enum { false = 0, true = 1 };

typedef unsigned char byte;
typedef unsigned int  word;
typedef unsigned long dword;
typedef word bool;

#endif

typedef word wbool;

// When not compiling the library itself, this header file is always included
// after including the game's 'types.h' header, so the game's own type
// definitions take effect in the client code instead of the library's.


typedef void (far *SoundFinishedCallback)(void);
typedef void (far *NewVocSectionCallback)(word, dword, byte huge*);


// [NOTE] These variables are actually word-sized, but the game code treats
// them as byte-sized. Most likely, the code here was using a typedef like
// `bool` which was defined as a byte in the game code, but as a word in the
// implementation of the library.  So it seems that - if the header file Jason
// gave the developers did contain the typedefs - they were deleted by the
// developers, maybe due to conflicting with Duke 2's own type definitions. And
// they forgot to check that their definition of `bool` matched Jason's, hence
// the mismatch.
//
// Now, this doesn't cause any observable problems, but it does result in
// slightly different code generation, which is less efficient in some cases.
//
// Another aspect is that these are declared as `far`, which causes the compiler
// to emit more expensive segment access code when loading these variables, even
// though there is only a single data segment.
//
// None of this is a serious problem, since these variables aren't accessed too
// frequently. It's just interesting to see.
extern far bool AdLibPresent;
extern far bool SoundBlasterPresent;

#ifdef SNDLIB_SRC_BUILD
char* SB_Init(char* blasterEnvVar);
#else
char far* SB_Init(char far* blasterEnvVar);
#endif

void SB_Shutdown(void);

void SB_PlaySample(byte huge* data, long sampleRate, dword length);
void SB_PlaySilence(long sampleRate, dword length);
wbool SB_IsSamplePlaying(void);
void SB_StopSound(void);
void SB_SetSoundFinishedCallback(SoundFinishedCallback callback);

void SB_PlayVoc(byte huge* data, bool includesHeader);
wbool SB_IsVocPlaying(void);
void SB_SetNewVocSectionCallback(NewVocSectionCallback callback);

#endif
