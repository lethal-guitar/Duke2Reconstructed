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

High-level sound playback code

The code here integrates the lower-level sound libraries (basicsnd and digisnd)
into one unified facade in the form of a simple PlaySound() function. We also
find some additional infrastructure here for loading and unloading sound
effects.

*******************************************************************************/


/** Return true if there's no digitized sound in the game data for the given id
 *
 * This essentially redundantly declares which sound effects have a digitized
 * version, and which ones don't. Redundant in the sense that this information
 * can also be derived from looking at the list of available files in the group
 * file and/or game directory. Unfortunately, due to being hardcoded, the game
 * can't be extended with additional custom digitized sound effects.
 */
static bool IsAdLibOnlySound(byte id)
{
  switch (id)
  {
    case SND_MENU_SELECT:
    case SND_ENEMY_HIT:
    case SND_SWOOSH:
    case SND_DUKE_JUMPING:
    case SND_LAVA_FOUNTAIN:
    case SND_DUKE_LANDING:
    case SND_MESSAGE_TYPING:
    case SND_FORCE_FIELD_FIZZLE:
    case SND_UNKNOWN1:
    case SND_MENU_TOGGLE:
    case SND_FALLING_ROCK:
    case SND_EARTHQUAKE:
    case SND_TELEPORT:
    case SND_UNKNOWN2:
    case SND_HEALTH_PICKUP:
    case SND_LETTERS_COLLECTED_CORRECTLY:
      return true;
  }

  return false;
}


/** Load all sound effects, except for those used in the intro movie */
void LoadSoundEffects(void)
{
  word size;
  word i;

  InitPcSpeaker(true, 60);

  // Load 'basic' sound effects from the AUDIOHED/AUDIOT files (Id Software
  // audio package format). This includes AdLib and PC speaker versions of all
  // sound effects (except for the intro sounds).
  //
  // [NOTE] It's not clear why these GetAssetFileSize() calls are here, since
  // the result is never used. The buffer sizes are hardcoded.
  // It would actually be more robust to allocate memory based on file size for
  // these two blocks of data, since that would avoid crashes caused by larger
  // than expected data files.  Maybe this was planned at some point but not
  // implemented completely?  Alternatively, maybe a more flexible scheme was
  // used during development but not included in the shipping game, and these
  // calls are remnants of that?
  //
  // [UNSAFE] Both sndPackageHeader and sndAudioData have fixed sizes. There's
  // no checking that the files actually fit into the available space.
  size = GetAssetFileSize("AUDIOHED.MNI");
  LoadAssetFile("AUDIOHED.MNI", sndPackageHeader);

  size = GetAssetFileSize("AUDIOT.MNI");
  LoadAssetFile("AUDIOT.MNI", sndAudioData);

  // Load digitized sound effects (.VOC files). Some sounds have no digitized
  // version. The intro sounds are not loaded here, they are only loaded when
  // needed (i.e., when the intro video is about to be played).
  //
  // [NOTE] Which sounds have digitized versions is hardcoded into the game. It
  // would also have been possible to simply try loading .VOC files for all
  // sound effects, and ignoring those where no file is found. That would have
  // made the game easier to mod, since it would then be possible to provide
  // additional digitized sound effects. But that's not what was done here.
  for (i = 0; i < LAST_DIGITIZED_SOUND_ID + 1; i++)
  {
    if (IsAdLibOnlySound(i) == false)
    {
      size = GetAssetFileSize(MakeFilename("SB_", i + 1, ".MNI"));

      sndDigitizedSounds[i] = MM_PushChunk(size, CT_COMMON);
      LoadAssetFile(MakeFilename("SB_", i + 1, ".MNI"), sndDigitizedSounds[i]);
    }
  }
}


/** Load sound effects used during the intro movie
 *
 * Unlike the regular sound effects, which are permanently kept in memory, the
 * intro sound effects are only loaded as needed to save memory.
 */
void LoadIntroSoundEffects(void)
{
  register word size;
  int i;

  if (SoundBlasterPresent == false)
  {
    return;
  }

  for (i = 42; i < 49; i++)
  {
    // Intro sound files are numbered starting at 3, but internally referred to
    // by IDs starting at 42. Subtracting 39 here translates between these two
    // numbering schemes.
    //
    // [NOTE] I'm not sure how the numbering scheme for the intro sound effects
    // came about. The highest regular (i.e., non-intro) sound has an ID of 33,
    // and that's also the highest index stored in the AUDIOT package.  If the
    // internal numbers (starting at 42) are meant to be a continuation of the
    // regular sound IDs, this leaves a gap of 8 IDs (34 to 41).  Why this gap?
    // Why number the files differently?
    size = GetAssetFileSize(MakeFilename("INTRO", i - 39, ".MNI"));
    sndDigitizedSounds[i] = MM_PushChunk(size, CT_INTRO_SOUND_FX);
    LoadAssetFile(
      MakeFilename("INTRO", i - 39, ".MNI"),
      sndDigitizedSounds[i]);
  }
}


/** Deallocate memory used by intro sound effects */
void UnloadIntroSoundEffects(void)
{
  MM_PopChunks(CT_INTRO_SOUND_FX);
}


/** Play AdLib or PC Speaker sound effect */
static void pascal PlayBasicSound(ibool useAdLib, word id)
{
  dword offsetToSound;
  dword size;
  byte far* data;

  if (useAdLib)
  {
    // Skip forward to the AdLib sound effects in the audio package
    id += STARTADLIBSOUNDS;
  }

  offsetToSound = sndPackageHeader[id];

  data = sndAudioData + offsetToSound;

  // Extract size from the start of the data
  size = *((dword*)data);
  data += sizeof(dword);

  StopAdLibSound();

  // [NOTE] It's not clear why the size is adjusted before calling these
  // functions - the implementations of the functions just subtract the same
  // value again before using the size. All of this would have been much simpler
  // if the size would just be passed along unchanged. Or even better, pass the
  // data pointer to the library unchanged and let the latter deal with
  // extracting the size. Who knows why it was done this way...
  if (useAdLib)
  {
    size += sizeof(AdLibSound);
    PlayAdLibSound((AdLibSound*)data, size);
  }
  else
  {
    size += sizeof(PCSound);
    PlayPcSpeakerSound((PCSound*)data, size);
  }
}


/** Play sound effect using appropriate device as configured/available
 *
 * This is what the rest of the code uses to trigger sound effects.  Kicks off
 * playback and returns immediately, with playback continuing concurrently.
 *
 * This function takes the chosen sound options into account, and uses either
 * SoundBlaster, AdLib, or PC Speaker depending on those options as well as the
 * nature of the sound and the availability of the chosen sound devices.  If
 * SoundBlaster output is chosen, but a sound has no digitized version, it will
 * be played using the AdLib. If AdLib and SoundBlaster are both enabled, and
 * the sound does have a digitized version, it will be played on both devices at
 * the same time.
 */
void pascal PlaySound(int id)
{
  byte priority = SOUND_PRIORITY[id];
  byte priorityFallback = SOUND_PRIORITY[id];

  if (!sndSoundEnabled)
  {
    return;
  }

  if (sndUseSbSounds && SoundBlasterPresent)
  {
    if (!SB_IsSamplePlaying())
    {
      sndCurrentPriority = 0;
    }

    if (!IsAdLibPlaying())
    {
      // [NOTE] Managing the priority for AdLib sound effects is redundant,
      // since the basicsnd library and the AUDIOT format already have a concept
      // of priority.
      sndCurrentPriorityFallback = 0;
    }

    if (IsAdLibOnlySound(id))
    {
      if (priorityFallback < sndCurrentPriorityFallback)
      {
        return;
      }
      else
      {
        sndCurrentPriorityFallback = priorityFallback;
        PlayBasicSound(true, id);
      }

      return;
    }

    // [NOTE] Comparing against 40 here is odd - the first intro sound is at id
    // 42, and the last regular sound is at 33. Comparing against either of
    // these two numbers would make more sense to me.  Also see comment in
    // LoadIntroSoundEffects().
    if (id > 40)
    {
      // Intro sounds only exist as VOC files
      SB_PlayVoc(sndDigitizedSounds[id], true);
      return;
    }

    if (priority >= sndCurrentPriority)
    {
      SB_PlayVoc(sndDigitizedSounds[id], true);
      sndCurrentPriority = priority;

      // no return here - we want to also play AdLib at the same time,
      // if applicable.
    }
  }

  if (id < NUM_SOUNDS && (sndUseAdLibSounds || sndUsePcSpeakerSounds))
  {
    // [NOTE] This really should be inside the following if-statement.  As it
    // stands, the priority is always reset to 0 if PC Speaker is the only
    // chosen output device. This doesn't cause problems in practice only
    // because - as mentioned above - the basicsnd library already handles
    // priority for AdLib and PC Speaker sounds.  But this also means that all
    // the priority checks for AdLib or PC Speaker in this function could be
    // removed.
    if (!IsAdLibPlaying())
    {
      sndCurrentPriority = 0;
    }

    if (sndUseAdLibSounds)
    {
      if (AdLibPresent && priority >= sndCurrentPriority)
      {
        sndCurrentPriority = priority;
        PlayBasicSound(true, id);
      }
    }
    else
    {
      if (!IsPcSpeakerPlaying())
      {
        sndCurrentPriority = 0;
      }

      if (sndUsePcSpeakerSounds && priority >= sndCurrentPriority)
      {
        sndCurrentPriority = priority;
        PlayBasicSound(false, id);
      }
    }
  }
}


/** Stops all sound effects on all playback devices */
void StopAllSound(void)
{
  SB_StopSound();
  StopAdLibSound();
  StopPcSpeakerSound();
}
