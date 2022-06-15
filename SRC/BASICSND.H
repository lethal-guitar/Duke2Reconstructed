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

#include "types.h"


typedef struct
{
  int priority;
} PCSound;


typedef struct
{
  byte mChar;
  byte cChar;
  byte mScale;
  byte cScale;
  byte mAttack;
  byte cAttack;
  byte mSus;
  byte cSus;
  byte mWave;
  byte cWave;
  byte nConn;

  // According to a comment in the Wolfenstein 3D source,
  // these unused bytes are for the Muse tool.
  byte unused[5];
} Instrument;


typedef struct
{
  int priority;
  Instrument inst;
  byte block;
} AdLibSound;


void InitPcSpeaker(ibool useLookupTable, word factor);
void PcSpeakerService(void);

void PlayPcSpeakerSound(PCSound far* sound, long length);
void StopPcSpeakerSound(void);
ibool IsPcSpeakerPlaying(void);

void AdLibSoundService(void);

void PlayAdLibSound(AdLibSound far* sound, long size);
void StopAdLibSound(void);
ibool IsAdLibPlaying(void);
