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

"Main" code - main() function, menu system & intro/attract loop, level loading

*******************************************************************************/


/** Start playback of the loading screen music */
void PlayLoadingScreenMusic(void)
{
  StopMusic();

  // [HACK] Instead of allocating dedicated memory for the loading screen song,
  // the memory for the backdrop offset table is repurposed to hold the music
  // data. Most likely this was done in order to reduce memory usage during level
  // loading. The backdrop offset table buffer is allocated as "common" chunk,
  // which means it stays allocated over the whole program's life time. In other
  // words, it's already there - reusing it for a different purpose doesn't
  // require any additional memory on top of what the game already uses.
  // My guess is that it wasn't possible to stay within the limits of available
  // memory without this hack. There might not have been any music during the
  // loading screen otherwise. It's a clever way to work around these
  // limitations, but also clearly a hack.
  //
  // [UNSAFE] There is no check that the music file fits into the buffer, so
  // this can easily cause a buffer overflow if the file is replaced with a
  // bigger one.
  PlayMusic("MENUSNG2.IMF", bdOffsetTable);
}


/** Fill the backdrop offset lookup table
 *
 * bdOffsetTable is a 1-dimensional array storing a 2-dimensional lookup table
 * of 80x50 values. The values are simply counting up from 0 up to 8000 in
 * steps of 8, but after every 40 values, the last 40 values are repeated. The
 * 2nd half of the table is repeating the first half.  In other words, within a
 * row, the values at indices 0 to 39 are identical to the values at indices 40
 * to 79.  Within the whole table, rows 0 to 24 are identical to rows 25 to 49.
 * The table is basically 4 copies of a 40x25 table arranged in a 4x4 grid.
 *
 * To make this a bit easier to imagine, let's say the table were only 8x6.
 * If that were the case, it would look like this:
 *
 * |  0 |  8 | 16 | 24 |  0 |  8 | 16 | 24 |
 * | 32 | 40 | 48 | 56 | 32 | 40 | 48 | 56 |
 * | 64 | 72 | 80 | 88 | 64 | 72 | 80 | 88 |
 * |  0 |  8 | 16 | 24 |  0 |  8 | 16 | 24 |
 * | 32 | 40 | 48 | 56 | 32 | 40 | 48 | 56 |
 * | 64 | 72 | 80 | 88 | 64 | 72 | 80 | 88 |
 *
 * The purpose of this table is to accelerate backdrop drawing, by turning
 * expensive modulo operations into cheaper memory reads. See UpdateBackdrop()
 * in game2.c for more details.
 */
void InitBackdropOffsetTable(void)
{
  register word x;
  register word y;
  word value = 0;

  // See PlayLoadingScreenMusic() - because the backdrop offset table is reused
  // for the loading screen music, the backdrop offset table has to be
  // recreated whenever the loading screen music has been playing.
  // This always happens at the end of the level loading process, so the
  // switchover to the level-specific music was also placed in here.
  StopMusic();
  PlayMusic(LVL_MUSIC_FILENAME(), sndInGameMusicBuffer);

  for (y = 0; y < 25 * 80; y += 80)
  {
    for (x = 0; x < 40; x++)
    {
      // Top-left quadrant
      *(bdOffsetTable + (y + x)) = value;

      // Top-right quadrant (horizontal copy)
      *(bdOffsetTable + (y + x + 40)) = value;

      // Bottom-left quadrant (vertical copy of left half)
      *(bdOffsetTable + (y + x + 2000)) = value;

      // Bottom-right quadrant (vertical copy of right half)
      *(bdOffsetTable + (y + x + 2000 + 40)) = value;

      value += 8;
    }
  }
}


/** Run script with given name from given script file */
byte pascal ShowScriptedUI(char far* scriptName, char far* filename)
{
  char far* text;

  uiDisplayPageChanged = false;

  text = MM_PushChunk(GetAssetFileSize(filename), CT_TEMPORARY);
  LoadAssetFile(filename, text);

  InterpretScript(FindScriptByName(scriptName, text));

  if (uiMenuState && uiDemoTimeoutTime < 200)
  {
    uiMenuSelectionStates[uiCurrentMenuId] = scriptPageIndex;
  }

  MM_PopChunk(CT_TEMPORARY);

  uiMenuCursorPos = 0;
  uiReporterTalkAnimTicksLeft = 0;

  if (uiDisplayPageChanged)
  {
    SetDrawPage(!gfxCurrentDisplayPage);
  }

  return scriptPageIndex;
}


/** Load the header and actor list for the given level */
void pascal LoadLevelHeader(char far* filename)
{
  word headerSize;

  // Load the header size - this is the offset to the start of the map data
  LoadAssetFilePart(filename, 0, &headerSize, sizeof(word));

  // Load the header data, this includes the list of actors to spawn into the
  // level.
  //
  // [UNSAFE] No checking that headerSize is less than or equal to the size of
  // levelHeaderData, which is fixed at compile time.
  LoadAssetFilePart(filename, sizeof(word), levelHeaderData, headerSize);

  // The header data is laid out as follows:
  //
  // | offset | what               | type                   |
  // | ------ | ------------------ | ---------------------- |
  // |      0 | tileset filename   | string                 |
  // |     13 | backdrop filename  | string                 |
  // |     26 | music filename     | string                 |
  // |     39 | flags              | byte (bitmask)         |
  // |     40 | alt. backdrop num  | byte                   |
  // |     41 | unused             | byte                   |
  // |     42 | unused             | byte                   |
  // |     43 | # actor desc words | word                   |
  // |     45 | actor list start   | word[]                 |
  // |  N - 2 | map width          | word                   |
  //
  // With N referring to `headerSize`.

  mapWidth = READ_LVL_HEADER_WORD(headerSize - 2);

  // The actor descriptions themselves are handled in SpawnLevelActors() and
  // LoadSpritesForLevel().
  levelActorListSize = READ_LVL_HEADER_WORD(43);

  // Interpret the remaining header data, this defines which type of parallax
  // scrolling to use and a few other things.
  ParseLevelFlags(
    *(levelHeaderData + 39),
    *(levelHeaderData + 40),
    *(levelHeaderData + 41),
    *(levelHeaderData + 42));

  SetMapSize(mapWidth);
}


/** Draw the background for the "enter new high score" screen */
void DrawNewHighScoreEntryBackground(void)
{
  ShowScriptedUI("New_Highscore", "TEXT.MNI");
}


/** Load tile set attributes for the current level
 *
 * Tile set attributes define which tiles are solid, have animation, can be
 * climbed, etc.
 * LoadLevelHeader() must be called before using this function.
 */
void LoadTileSetAttributes(void)
{
  gfxTilesetAttributes = MM_PushChunk(3600, CT_CZONE);
  LoadAssetFilePart(LVL_TILESET_FILENAME(), 0, gfxTilesetAttributes, 3600);
}


/** Load solid (non-transparent) tiles for the current tile set
 *
 * LoadLevelHeader() must be called before using this function.
 */
void LoadUnmaskedTiles(void)
{
  byte far* data = MM_PushChunk(32000, CT_TEMPORARY);

  LoadAssetFilePart(LVL_TILESET_FILENAME(), 3600, data, 32000);
  UploadTileset(data, 8000, 0x4000);

  MM_PopChunk(CT_TEMPORARY);
}


/** Load masked (partially transparent) tiles for the current tile set
 *
 * LoadLevelHeader() must be called before using this function.
 */
void LoadMaskedTiles(void)
{
  gfxMaskedTileData = MM_PushChunk(6400, CT_MASKED_TILES);
  LoadAssetFilePart(LVL_TILESET_FILENAME(), 35600, gfxMaskedTileData, 6400);
}


/** Allocate memory for the current level's music data
 *
 * LoadLevelHeader() must be called before using this function.
 */
void AllocateInGameMusicBuffer(void)
{
  sndInGameMusicBuffer =
    MM_PushChunk(GetAssetFileSize(LVL_MUSIC_FILENAME()), CT_INGAME_MUSIC);
}


/** Test if the given ID specifies that the next actor should be skipped
 *
 * At which difficulty actors appear in a level is specified via dedicated actor
 * types. These marker actors affect the actor right after the marker.
 */
bool pascal CheckDifficultyMarker(word id)
{
  if (
    (id == ACT_META_MEDIUMHARD_ONLY && gmDifficulty == DIFFICULTY_EASY) ||
    (id == ACT_META_HARD_ONLY && gmDifficulty != DIFFICULTY_HARD))
  {
    return true;
  }

  return false;
}


/** Load sprites required by the actors present in the current level
 *
 * LoadLevelHeader() must be called before using this function.
 */
void LoadSpritesForLevel(void)
{
  int i;
  word actorId;

  // levelActorListSize is the number of words, hence we multiply by 2.  Each
  // actor specification is 3 words long, hence we add 6 to i on each
  // iteration.
  for (i = 0; i < levelActorListSize * 2; i += 6)
  {
    actorId = READ_LVL_ACTOR_DESC_ID(i);

    // Skip actors that don't appear in the currently chosen difficulty
    if (CheckDifficultyMarker(actorId))
    {
      i += 6;
      continue;
    }

    LoadSprite(actorId);
    LoadActorExtraSprites(actorId);
  }
}


/** Load the map data (tile grid) for the specified level file */
void pascal LoadMapData(char far* filename)
{
  word headerSize;
  word extraDataSize;
  byte far* compressedExtraData;

  // Load offset to map data
  LoadAssetFilePart(filename, 0, &headerSize, sizeof(word));

  // The map data has a fixed size, different level dimensions only change the
  // interpretation of the data.
  mapData = MM_PushChunk(65500, CT_MAP_DATA);
  LoadAssetFilePart(filename, (dword)headerSize + sizeof(word), mapData, 65500);

  // Load size of the extra map data. See UpdateAndDrawGame in game2.c for more
  // information about the extra map data.
  LoadAssetFilePart(
    filename,
    (dword)headerSize + (sizeof(word) + 65500),
    &extraDataSize,
    sizeof(word));

  compressedExtraData = MM_PushChunk(extraDataSize, CT_TEMPORARY);

  // Load and decompress the extra data
  LoadAssetFilePart(
    filename,
    (dword)headerSize + (sizeof(word) * 2 + 65500),
    compressedExtraData,
    extraDataSize);
  DecompressRLE(compressedExtraData, mapExtraData);

  MM_PopChunk(CT_TEMPORARY);
}


/** Set camera position so that the player is roughly centered on screen
 *
 * Notably, the logic here is not the same as in UpdatePlayer(). Often, the
 * actual camera position at the start of the level will be a bit different due
 * to the player update logic being invoked once before fading in the game.
 */
void CenterViewOnPlayer(void)
{
  gmCameraPosX = plPosX - (VIEWPORT_WIDTH/2 - 1);

  if ((int)gmCameraPosX < 0)
  {
    gmCameraPosX = 0;
  }
  else if (gmCameraPosX > mapWidth - VIEWPORT_WIDTH)
  {
    gmCameraPosX = mapWidth - VIEWPORT_WIDTH;
  }

  gmCameraPosY = plPosY - (VIEWPORT_HEIGHT - 1);

  if ((int)gmCameraPosY < 0)
  {
    gmCameraPosY = 0;
  }
  else if (gmCameraPosY > mapBottom - (VIEWPORT_HEIGHT + 1))
  {
    gmCameraPosY = mapBottom - (VIEWPORT_HEIGHT + 1);
  }
}


/** Spawn actors that appear in the current level */
void SpawnLevelActors(void)
{
  int i;
  int currentDrawIndex;
  word drawIndex;
  word offset;
  word x;
  word y;
  word actorId;

  // The draw index is a means to make certain actors always appear in front of
  // or behind other types of actors, regardless of their position in the actor
  // list (which normally defines the order in which actors are drawn).
  //
  // The way it works is that we do multiple passes over the actor list in the
  // level file, and only spawn the actors during each pass which match the
  // draw index for that pass.
  //
  // Notably, this only works for actors that appear at the start of the level.
  // Any actors that are spawned during gameplay will be placed at wherever a
  // free slot in the actor list can be found, so their draw order is basically
  // random (it's still deterministic but depends on what has happened so far
  // during gameplay, so in practice, it very much appears to be random).
  for (currentDrawIndex = -1; currentDrawIndex < 4; currentDrawIndex++)
  {
    // levelActorListSize is the number of words, hence we multiply by 2.  Each
    // actor specification is 3 words long, hence we add 6 to i on each
    // iteration.
    for (i = 0; i < levelActorListSize * 2; i += 6)
    {
      actorId = READ_LVL_ACTOR_DESC_ID(i);

      // Skip actors that don't appear in the currently chosen difficulty
      if (CheckDifficultyMarker(actorId))
      {
        i += 6;
        continue;
      }

      offset = gfxActorInfoData[actorId];

      drawIndex = AINFO_DRAW_INDEX(offset);

      if (drawIndex == currentDrawIndex)
      {
        x = READ_LVL_ACTOR_DESC_X(i);
        y = READ_LVL_ACTOR_DESC_Y(i);

        if (SpawnActorInSlot(gmNumActors, actorId, x, y))
        {
          gmNumActors++;
        }
      }
    }
  }

  // [NOTE] It would seem a little clearer to call this function at the call
  // site of SpawnLevelActors, instead of burying it in here.
  CenterViewOnPlayer();
}


/** Load the "status icon" tileset containing various fonts and UI elements */
void LoadStatusIcons(void)
{
  byte far* data = MM_PushChunk(GetAssetFileSize("STATUS.MNI"), CT_TEMPORARY);

  LoadAssetFile("STATUS.MNI", data);
  UploadTileset(data, 8000, 0x6000);

  MM_PopChunk(CT_TEMPORARY);
}


/** Take away 1 unit of health from the player, if applicable
 *
 * Checks if the player is cloaked or has mercy invincibility frames.
 * Initiates the player death sequence if health reaches 0.
 */
void DamagePlayer(void)
{
  if (sysTecMode) { return; }

  if (
    !plCloakTimeLeft &&
    !plMercyFramesLeft &&
    plState != PS_DYING)
  {
    plHealth--;
    gmPlayerTookDamage = true;

    if (plHealth > 0 && plHealth < 12)
    {
      plMercyFramesLeft = 50 - gmDifficulty * 10;
      HUD_DrawHealth(plHealth);
      PlaySound(SND_DUKE_PAIN);
    }
    else
    {
      if (plState == PS_USING_SHIP)
      {
        plKilledInShip = true;
      }

      plState = PS_DYING;
      plDeathAnimationStep = 0;
      PlaySound(SND_DUKE_DEATH);
    }
  }
}


/** Redraw the entire HUD
 *
 * Also used to initialize the HUD display at the start of a level. During
 * gameplay, only the parts of the HUD that change are redrawn.
 */
void HUD_RedrawAll(void)
{
  HUD_DrawBackground();
  GiveScore(0);

  if (gmCurrentEpisode < 4) // hide level number during demo playback
  {
    HUD_DrawLevelNumber(gmCurrentLevel);
  }

  HUD_DrawWeapon(plWeapon);
  HUD_DrawHealth(plHealth);
  HUD_DrawAmmo(plAmmo);
  HUD_DrawInventory();

  if (plCollectedLetters & 0x100)
  {
    HUD_DrawLetterIndicator(ACT_LETTER_INDICATOR_N);
  }

  if (plCollectedLetters & 0x200)
  {
    HUD_DrawLetterIndicator(ACT_LETTER_INDICATOR_U);
  }

  if (plCollectedLetters & 0x400)
  {
    HUD_DrawLetterIndicator(ACT_LETTER_INDICATOR_K);
  }

  if (plCollectedLetters & 0x800)
  {
    HUD_DrawLetterIndicator(ACT_LETTER_INDICATOR_E);
  }

  if (plCollectedLetters & 0x1000)
  {
    HUD_DrawLetterIndicator(ACT_LETTER_INDICATOR_M);
  }

  if (gmBossActivated)
  {
    HUD_DrawBossHealthBar(gmBossHealth);
  }
}


#include "demo.c"
#include "player.c"


/** Reset all gameplay-relevant global variables to their default values */
static void ResetGameState(void)
{
  int i;

  gmGameState = GS_RUNNING;
  gmBossActivated = false;
  hudShowingHintMachineMsg = false;
  plBodyExplosionStep = 0;
  plAttachedSpider1 = 0;
  plAttachedSpider2 = 0;
  plAttachedSpider3 = 0;
  hudMessageCharsPrinted = 0;
  hudMessageDelay = 0;
  plOnElevator = false;
  gfxFlashScreen = false;
  plKilledInShip = false;
  bdAddress = 0x4000;
  bdAutoScrollStep = 0;
  gfxCurrentDisplayPage = 1;
  gmRngIndex = 0;
  plAnimationFrame = 0;
  plState = PS_NORMAL;
  plMercyFramesLeft = INITIAL_MERCY_FRAMES;
  gmIsTeleporting = false;
  gmExplodingSectionTicksElapsed = 0;
  plInteractAnimTicks = 0;
  plBlockLookingUp = false;
  gmEarthquakeCountdown = 0;
  gmEarthquakeThreshold = 0;

  ResetEffectsAndPlayerShots();
  ClearParticles();

  if (!gmBeaconActivated)
  {
    gmPlayerTookDamage = false;

    gmNumMovingMapParts = 0;
    for (i = 0; i < MAX_NUM_MOVING_MAP_PARTS; i++)
    {
      gmMovingMapParts[i].type = 0;
    }

    gmRequestUnlockNextDoor = false;
    plAirlockDeathStep = 0;
    gmRequestUnlockNextForceField = false;
    gmWaterAreasPresent = false;
    gmRadarDishesLeft = 0;
    plCollectedLetters = 0;
    plRapidFireTimeLeft = 0;
    gmReactorDestructionStep = 0;
    bdAddressAdjust = 0;
    plCloakTimeLeft = 0;
    gmCamerasDestroyed = gmCamerasInLevel = 0;
    gmWeaponsCollected = gmWeaponsInLevel = 0;
    gmMerchCollected = gmMerchInLevel = 0;
    gmTurretsDestroyed = gmTurretsInLevel = 0;
    plWeapon_hud = 0;
    gmNumActors = 0;
    plHealth = PLAYER_MAX_HEALTH;

    ClearInventory();

    gmOrbsLeft = 0;
    gmBombBoxesLeft = 0;
  }
}


/** Deallocate memory used for level-specific data */
static void UnloadPerLevelData(void)
{
  MM_PopChunks(CT_TEMPORARY);
  MM_PopChunk(CT_MAP_DATA);
  MM_PopChunks(CT_SPRITE);
  MM_PopChunk(CT_INGAME_MUSIC);
}


/** Deallocate memory used for level-specific tile set
 *
 * [NOTE] It's not clear why this isn't combined with UnloadPerLevelData into
 * one. Perhaps at some point in development the tile set could be exchanged
 * while keeping the level data? But in the shipping game, this is never
 * called without also calling UnloadPerLevelData().
 */
static void UnloadTileset(void)
{
  MM_PopChunk(CT_MASKED_TILES);
  MM_PopChunk(CT_CZONE);
}


/** Finish level loading and fade in the screen */
static void StartLevel(void)
{
  // [NOTE] Unnecessary, this already happens in AwaitProgressBarEnd() during
  // LoadLevel().
  uiProgressBarStepDelay = 0;

  SpawnLevelActors();
  CLEAR_SCREEN();
  HUD_RedrawAll();

  // [NOTE] This is never set to anything other than VIEWPORT_HEIGHT, so it's
  // not clear why it's a variable and not just a compile-time constant.
  // Perhaps the viewport height was variable at some point during development?
  mapViewportHeight = VIEWPORT_HEIGHT;

  // Run one frame of the game, this draws the initial image onto the screen
  // which we can then fade in to.
  UpdateAndDrawGame(&WaitAndUpdatePlayer);

  FadeInScreen();

  AdjustMusicForBossLevel();
}


/** Draw background image for the loading screen */
static void DrawLoadingScreenBg(word episode)
{
  switch (episode)
  {
    case 0:
    case 4: // demo
      DrawFullscreenImage("Load1.mni");
      break;

    case 1:
      DrawFullscreenImage("Load2.mni");
      break;

    case 2:
      DrawFullscreenImage("Load3.mni");
      break;

    case 3:
      DrawFullscreenImage("Load4.mni");
      break;
  }
}


/** Load specified level for the current episode */
void pascal LoadLevel(byte level)
{
  char* filename = LEVEL_NAMES[gmCurrentEpisode][level];

  gmCurrentLevel = level;
  plHealth = PLAYER_MAX_HEALTH;
  gmBeaconActivated = false;

  FadeOutScreen();
  UnloadPerLevelData();
  PlayLoadingScreenMusic();
  DrawLoadingScreenBg(gmCurrentEpisode);
  FadeInScreen();

  // Enable the progress bar. The progress bar is updated concurrently to this
  // function by the timer interrupt handler (see TimerInterruptHandler() in
  // music.c).
  //
  // The progress bar is mostly fake. It doesn't represent how much work the
  // game still needs to do in any way. It simply advances continuously. But the
  // game increases the speed at which it advances at various points in the
  // process, to make the progress bar appear more genuine.
  uiProgressBarState = 1;
  uiProgressBarTicksElapsed = 0;

  // Make the progress bar advance at ~4.3 pixels/s
  uiProgressBarStepDelay = 8;

  ResetGameState();
  LoadLevelHeader(filename);
  UnloadTileset();
  LoadTileSetAttributes();

  // Speed it up to 5 pixels/s
  uiProgressBarStepDelay--;

  LoadMaskedTiles();
  LoadUnmaskedTiles();

  // Speed it up to ~5.8 pixels/s
  uiProgressBarStepDelay--;

  LoadBackdrop();

  // Speed it up to 7 pixels/s
  uiProgressBarStepDelay--;

  AllocateInGameMusicBuffer();
  LoadSpritesForLevel();

  // Wait for the progress bar to fill up completely.
  // The decrement makes it move at 8.75 px/s, but AwaitProgressBarEnd()
  // immediately speeds it up to 35 px/s.
  // Also does a fade-out (in AwaitProgressBarEnd).
  uiProgressBarStepDelay--;
  AwaitProgressBarEnd();

  LoadMapData(filename);

  // Create a temporary saved game file with the current state.  This is used to
  // restore weapon, score etc. when restarting the level after a player death,
  // and also when saving the game: Saved games always store the state at the
  // beginning of the level, but the player can use the save function at any
  // time.  With the temporary save file, the game can always retrieve the game
  // state from when the level was started, without needing to keep it in
  // memory.
  WriteSavedGame('T');

  // Disable the progress bar
  uiProgressBarState = 0;

  // This is also going to start the level-specific music, see comment inside
  // that function.
  InitBackdropOffsetTable();

  // Also does a fade-in
  StartLevel();

  if (gmRadarDishesLeft)
  {
    ShowInGameMessage(
      "DUKE, FIND AND DESTROY ALL THE*RADAR DISHES ON THIS LEVEL.");
  }
}


/** Restart level after player death */
static void pascal RestartLevel(byte level)
{
  register char* filename = LEVEL_NAMES[gmCurrentEpisode][level];

  FadeOutScreen();
  ResetGameState();

  if (gmBeaconActivated)
  {
    //
    // Restore from respawn beacon - most of the game state stays intact
    //
    ReadSavedGame('Z');

    plPosX = gmBeaconPosX;
    plPosY = gmBeaconPosY;
    plActorId = ACT_DUKE_R;

    CenterViewOnPlayer();

    HUD_RedrawAll();
    UpdateAndDrawGame(&WaitAndUpdatePlayer);
    FadeInScreen();
  }
  else
  {
    //
    // Player didn't reach a beacon, completely restart the level
    //
    StopMusic();
    MM_PopChunks(CT_TEMPORARY);

    PlayMusic(LVL_MUSIC_FILENAME(), sndInGameMusicBuffer);

    // Reload the map, since it may have changed during gameplay due to
    // destructible walls, falling map parts etc.
    MM_PopChunk(CT_MAP_DATA);
    LoadMapData(filename);

    // Reload state from the beginning of the level - the 'T' saved game file
    // is written in LoadLevel().
    ReadSavedGame('T');
    StartLevel();

    if (gmRadarDishesLeft)
    {
      ShowInGameMessage(
        "DUKE, FIND AND DESTROY ALL THE*RADAR DISHES ON THIS LEVEL.");
    }
  }
}


/** Allocate memory for the backdrop offset table */
void AllocateBackdropOffsetTable(void)
{
  // See InitBackdropOffsetTable().
  bdOffsetTable = MM_PushChunk(8000, CT_COMMON);
}


/** Loads various sprites that are always kept in memory */
void LoadCommonSprites(void)
{
  LoadSpriteRange(ACT_DUKE_L, ACT_DUKE_R);
  LoadSpriteRange(ACT_MUZZLE_FLASH_UP, ACT_MUZZLE_FLASH_RIGHT);
  LoadSpriteRange(ACT_REGULAR_SHOT_HORIZONTAL, ACT_REGULAR_SHOT_VERTICAL);
  LoadSpriteRange(ACT_NUCLEAR_WASTE_CAN_EMPTY, ACT_NUCLEAR_WASTE_CAN_DEBRIS_4);
  LoadSpriteRange(ACT_EXPLOSION_FX_1, ACT_FLAME_FX);
  LoadSprite(ACT_BONUS_GLOBE_SHELL);
  LoadSpriteRange(ACT_BONUS_GLOBE_DEBRIS_1, ACT_BONUS_GLOBE_DEBRIS_2);
  LoadSprite(ACT_DUKE_DEATH_PARTICLES);
  LoadSprite(ACT_SMOKE_CLOUD_FX);
  LoadSpriteRange(ACT_SCORE_NUMBER_FX_100, ACT_SCORE_NUMBER_FX_10000);
  LoadSpriteRange(ACT_WHITE_BOX, ACT_BLUE_FIREBALL_FX);
  LoadSprite(ACT_MENU_FONT_GRAYSCALE);
}


/** Wrap up a game session */
void FinishGameSession(void)
{
  FinishDemoRecording();
  FinishDemoPlayback();

  gfxCurrentDisplayPage = 0;
}


/** Reset player-specific global variables to their default values */
void ResetPlayerState(void)
{
  int i;

  plScore = 0;
  plAmmo = MAX_AMMO;
  plWeapon = WPN_REGULAR;
  plHealth = PLAYER_MAX_HEALTH;
  plRapidFireTimeLeft = 0;

  for (i = 0; i < NUM_TUTORIAL_IDS; i++)
  {
    gmTutorialsShown[i] = false;
  }
}


/** Run the game, returns when player quits or finishes the episode */
void pascal RunInGameLoop(byte startingLevel)
{
  LoadLevel(startingLevel);

  // Show a welcome message on the first level of each episode
  if (!startingLevel)
  {
    // [BUG] If the first level contains radar dishes, the welcome message
    // overrides the radar dish message. That situation never occurs in the
    // original levels shipping with the game, but it can occur in user-made
    // levels.
    ShowInGameMessage("WELCOME TO DUKE NUKEM II!");
  }

  // The logic here is a bit hard to follow, due to the density of the code.
  // Most of this is actually for dealing with the various hot-keys that open
  // in-game menus and handling the results of the latter, implementing cheat
  // codes etc.  Readability could've been improved massively here by moving
  // all of that stuff into a separate function, to make the core logic - the
  // path that's taken on almost all loop iterations - easier to see. Alas,
  // that's not what the original author did, and we can't introduce new
  // functions without changing the machine code that's produced.
  //
  // In lieu of that, here's a commented out version of the loop that's stripped
  // down to the essentials to make the logic easier to see:
#if 0
  do
  {
    if (gmIsTeleporting)
    {
      // Finish a teleport that was started on the previous frame
      gmIsTeleporting = false;
      SetDrawPage(!gfxCurrentDisplayPage);
      UpdateAndDrawGame(&WaitAndUpdatePlayer);
      FadeInScreen();
    }
    else
    {
      // Update and render a single frame of the game
      HUD_UpdateInventoryAnimation();
      UpdateAndDrawGame(&WaitAndUpdatePlayer);
    }

    // Handle player death, level exit and teleporting
    if (gmGameState == GS_PLAYER_DIED)
    {
      RestartLevel(gmCurrentLevel);
    }
    else if (gmGameState == GS_LEVEL_FINISHED)
    {
      // Skip bonus screen during demo playback
      if (gmCurrentEpisode < 4)
      {
        ShowBonusScreen();
      }

      // Go to the next level
      LoadLevel(gmCurrentLevel = gmCurrentLevel + 1);
    }

    if (gmIsTeleporting)
    {
      // Start teleporting, the fade-in happens on the next frame
      FadeOutScreen();
      plPosY = gmTeleportTargetPosY;
      plPosX = gmTeleportTargetPosX + 1;
      CenterViewOnPlayer();
    }

    // Reset any screen shift set by SHAKE_SCREEN during the game update. This
    // is what makes the screen actually shake, by quickly resetting it back
    // after it was shifted.
    SetScreenShift(0);
  }
  while (gmGameState == GS_RUNNING);
#endif

  // Now here's the actual, full loop with all the menu handling code
  do
  {
    if (gmIsTeleporting)
    {
      // Finish a teleport that was started on the previous frame
      gmIsTeleporting = false;
      SetDrawPage(!gfxCurrentDisplayPage);
      UpdateAndDrawGame(&WaitAndUpdatePlayer);
      FadeInScreen();
    }
    else
    {
      HUD_UpdateInventoryAnimation();

      //
      // Handle entering menus and other hot-keys
      //
      if (kbKeyState[SCANCODE_P] || jsButton4)
      {
        if (!sysTecMode)
        {
          ShowScriptedUI("Paused", "TEXT.MNI");
        }
        else
        {
          AwaitInput();
        }

        WaitTicks(5);
      }
      else if (kbKeyState[SCANCODE_M])
      {
        sndMusicEnabled = !sndMusicEnabled;

        if (sndMusicEnabled)
        {
          ShowScriptedUI("Music_On", "TEXT.MNI");
        }
        else
        {
          ResetAdLibMusicChannels();
          ShowScriptedUI("Music_Off", "TEXT.MNI");
        }

        WaitTicks(20);
      }
      else if (kbKeyState[SCANCODE_S])
      {
        sndSoundEnabled = !sndSoundEnabled;

        if (sndSoundEnabled)
        {
          ShowScriptedUI("Sound_On", "TEXT.MNI");
        }
        else
        {
          ShowScriptedUI("Sound_Off", "TEXT.MNI");
        }

        WaitTicks(20);
      }
      else if (kbKeyState[SCANCODE_F3])
      {
        byte menuSelection = ShowScriptedUI("Restore_Game", "OPTIONS.MNI");

        if (menuSelection != 0xFF)
        {
          if (IsSaveSlotEmpty(menuSelection - 1))
          {
            ShowScriptedUI("No_Game_Restore", "OPTIONS.MNI");
          }
          else
          {
            ShowScriptedUI("&Load", "TEXT.MNI");

            // Load the selected saved game
            gmBeaconActivated = false;
            ReadSavedGame('0' + menuSelection);
            FinishDemoRecording();
            LoadLevel(gmCurrentLevel);
            continue;
          }
        }

        FadeOutScreen();
        HUD_RedrawAll();
        UpdateAndDrawGame(&WaitAndUpdatePlayer);
        FadeInScreen();
      }
      else if (kbKeyState[SCANCODE_F2])
      {
        byte menuSelection = ShowScriptedUI("Save_Game", "OPTIONS.MNI");

        if (menuSelection == 0xFF || !RunSaveGameNameEntry(menuSelection - 1))
        {
          // No-op
        }
        else
        {
          ShowScriptedUI("&Save", "TEXT.MNI");

          // This is a very roundabout way of saving the score, weapon etc.
          // from when the level was started, regardless of the current state.
          // The save game menu can be invoked at any time, but loading a save
          // always restores the state from the beginning of the level.  To
          // achieve this, the game writes a temporary save file (called
          // 'NUKEM2.-ST') after loading a level (see LoadLevel()). Now when we
          // want to save the game, we first write out another temporary save
          // file, 'NUKEM2.-SB', in order to backup the current state, then
          // load the state from when the level was started, write out the
          // saved game file, and then reload the backup.
          //
          // A much easier way of achieving the same would be to simply make a
          // copy of the already written 'NUKEM2.-ST' file and name it as
          // desired.
          WriteSavedGame('B');
          ReadSavedGame('T');
          WriteSavedGame('0' + menuSelection);
          ReadSavedGame('B');
        }

        FadeOutScreen();
        HUD_RedrawAll();
        UpdateAndDrawGame(&WaitAndUpdatePlayer);
        FadeInScreen();
      }

      //
      // Handle cheat codes
      //
      if (kbKeyState[SCANCODE_G] && kbKeyState[SCANCODE_O] && kbKeyState[SCANCODE_D])
      {
        ShowScriptedUI("The_Prey", "TEXT.MNI");
        WaitTicks(30);
      }

#ifdef REGISTERED_VERSION
      if (kbKeyState[SCANCODE_E] && kbKeyState[SCANCODE_A] && kbKeyState[SCANCODE_T])
      {
        plScore = 0;
        plHealth = PLAYER_MAX_HEALTH;

        ShowScriptedUI("Full_Health", "TEXT.MNI");
        WaitTicks(30);

        FadeOutScreen();
        HUD_RedrawAll();
        UpdateAndDrawGame(&WaitAndUpdatePlayer);
        FadeInScreen();
      }
      else if (kbKeyState[SCANCODE_N] && kbKeyState[SCANCODE_U] && kbKeyState[SCANCODE_K])
      {
        word i;
        word weapons[] = { 0, 0, 0 };
        byte weaponsFound = 0;

        ShowScriptedUI("Now_Ch", "TEXT.MNI");
        FadeOutScreen();

        gmRadarDishesLeft = 0;

        for (i = 0; i < gmNumActors; i++)
        {
          register ActorState* actor = gmActorStates + i;

          if (actor->id == ACT_RADAR_DISH)
          {
            actor->deleted = true;
          }

          if (weaponsFound < 3 && actor->id == ACT_GREEN_BOX)
          {
            if (actor->var2 == ACT_ROCKET_LAUNCHER)
            {
              weapons[weaponsFound] = i;
              weaponsFound++;
            }
            else if (actor->var2 == ACT_LASER)
            {
              weapons[weaponsFound] = i;
              weaponsFound++;
            }
            else if (actor->var2 == ACT_FLAME_THROWER)
            {
              weapons[weaponsFound] = i;
              weaponsFound++;
            }
          }
          else if (actor->id == ACT_WHITE_BOX && !actor->deleted)
          {
            switch (actor->var2)
            {
              case ACT_BLUE_KEY:
                AddInventoryItem(ACT_BLUE_KEY);
                actor->deleted = true;
                break;

              case ACT_CIRCUIT_CARD:
                AddInventoryItem(ACT_CIRCUIT_CARD);
                actor->deleted = true;
                break;

              case ACT_CLOAKING_DEVICE:
                if (plCloakTimeLeft == 0)
                {
                  AddInventoryItem(ACT_CLOAKING_DEVICE_ICON);
                  plCloakTimeLeft = CLOAK_TIME;
                }
                break;
            }
          }

          if (weaponsFound != 0)
          {
            word pickupHandle = weapons[weaponsFound - 1];
            register ActorState* weaponPickupActor =
              gmActorStates + pickupHandle;

            if (weaponPickupActor->var2 != ACT_FLAME_THROWER)
            {
              plAmmo = MAX_AMMO;
            }
            else
            {
              plAmmo = MAX_AMMO_FLAMETHROWER;
            }

            plWeapon = weaponPickupActor->var3;
          }
        }

        HUD_RedrawAll();
        UpdateAndDrawGame(&WaitAndUpdatePlayer);
        FadeInScreen();
      }
#endif

      //
      // Handle options menu and help screen hot-keys
      //
      else if (kbKeyState[SCANCODE_F1] || kbKeyState[SCANCODE_H])
      {
        if (kbKeyState[SCANCODE_F1])
        {
          ShowOptionsMenu();
        }

        if (kbKeyState[SCANCODE_H])
        {
          ShowScriptedUI("&Instructions", "TEXT.MNI");
        }

        FadeOutScreen();
        HUD_RedrawAll();
        UpdateAndDrawGame(&WaitAndUpdatePlayer);
        FadeInScreen();
      }
      else
      {
        // If none of the above applies, do a regular frame update.
        // This is what happens most of the time, it's just a bit buried in
        // here.
        UpdateAndDrawGame(&WaitAndUpdatePlayer);
      }
    }

    // Handle player death, level exit, and teleporting
    switch (gmGameState)
    {
      case GS_PLAYER_DIED:
        RestartLevel(gmCurrentLevel);
        break;

      case GS_LEVEL_FINISHED:
#ifdef REGISTERED_VERSION
        // If copy protection failed, boot the player from the game after
        // finishing one level. Don't do this if we're playing back a demo
        // (episode 4).
        if (sysCopyProtectionFailed && gmCurrentEpisode < 4)
        {
          // This causes the loop to terminate, throwing the player out of the
          // game back into the main menu, where they will be shown a message
          // indicating failed copy protection due to the CheckCopyProtection()
          // call in RunMainLoop().
          gmGameState = GS_PLAYER_DIED;
        }
        else
#endif
        {
          // Skip bonus screen during demo playback
          if (gmCurrentEpisode < 4)
          {
            ShowBonusScreen();
          }

          LoadLevel(gmCurrentLevel = gmCurrentLevel + 1);
        }
    }

    if (gmIsTeleporting)
    {
      // Start teleporting, the fade-in happens on the next frame
      FadeOutScreen();
      plPosY = gmTeleportTargetPosY;
      plPosX = gmTeleportTargetPosX + 1;
      CenterViewOnPlayer();
    }

    // Reset any screen shift set by SHAKE_SCREEN during the game update. This
    // is what makes the screen actually shake, by quickly resetting it back
    // after it was shifted.
    SetScreenShift(0);
  }
  while (gmGameState == GS_RUNNING && gmGameState != GS_EPISODE_FINISHED);

  StopAllSound();
}


/** Start playing back specified music file, outside of gameplay */
void PlayMenuMusic(char* filename)
{
  StopMusic();
  MM_PopChunk(CT_MENU_MUSIC);

  sndMenuMusicBuffer = MM_PushChunk(GetAssetFileSize(filename), CT_MENU_MUSIC);
  PlayMusic(filename, sndMenuMusicBuffer);
}


/** Show image or series of images advancing the story after each episode */
void ShowEpisodeEndScreen(void)
{
  // Set level to zero so that we don't trigger the special case in PlayMusic()
  // (which is invoked by PlayMenuMusic()). The PlayMenuMusic() call below
  // wouldn't have any effect otherwise.
  gmCurrentLevel = 0;

  PlayMenuMusic("NEVRENDA.IMF");

  // Now set it to 7 in order to trigger the special case in ShowBonusScreen(),
  // which will skip starting the bonus screen song.
  gmCurrentLevel = 7;

  FadeOutScreen();
  WaitTicks(140);

  switch (gmCurrentEpisode)
  {
    case 0:
      DrawFullscreenImage("END1-3.mni");
      FadeInScreen();
      AwaitInput();

      FadeOutScreen();
      DrawFullscreenImage("END1-1.mni");
      FadeInScreen();
      AwaitInput();

      FadeOutScreen();
      DrawFullscreenImage("END1-2.mni");
      FadeInScreen();
      AwaitInput();
      break;

    case 1:
      DrawFullscreenImage("END2-1.mni");
      FadeInScreen();
      AwaitInput();
      break;

    case 2:
      DrawFullscreenImage("END3-1.mni");
      FadeInScreen();
      AwaitInput();
      break;

    case 3:
      DrawFullscreenImage("END4-1.mni");
      FadeInScreen();
      AwaitInput();

      // [BUG] Missing fade-out

      DrawFullscreenImage("END4-3.mni");
      FadeInScreen();
      AwaitInput();
      break;
  }

  // This call is a no-op for the first three episodes
  ShowDuke3dTeaserScreen();

  CLEAR_SCREEN();

  ShowBonusScreen();

#ifdef SHAREWARE
  ShowScriptedUI("Ordering_Info", "ORDERTXT.MNI");
#endif

  gmCurrentLevel = 0;
}


/** Run a "game session"
 *
 * Start at the specified episode & level, and keep going until the player
 * either finishes the episode's last level or quits the game.
 */
void pascal RunGameSession(byte episode, byte level)
{
  gmCurrentEpisode = episode;

  InitDemoRecording();

  RunInGameLoop(level);

  UnloadPerLevelData();
  UnloadTileset();
  FinishGameSession();
}


/** Run the options menu, returns once player exits back to main menu */
void ShowOptionsMenu(void)
{
  word menuSelection;
  word innerMenuSelection;

restart:
  for (;;)
  {
    menuSelection = ShowScriptedUI("My_Options", "OPTIONS.MNI");
    switch (menuSelection)
    {
      case 5:
        for (;;)
        {
          innerMenuSelection = ShowScriptedUI("Key_Config", "OPTIONS.MNI");
          jsCalibrated = false;

          if (innerMenuSelection == 0xFF) { goto restart; }

          RunRebindKeyDialog(innerMenuSelection);
          continue;
        }

      case 6:
        ShowScriptedUI("&Calibrate", "OPTIONS.MNI");
        RunJoystickCalibration();
        break;

      case 7:
        FadeOutScreen();
        SetDrawPage(0);
        CLEAR_SCREEN();

        menuSelection = ShowScriptedUI("Game_Speed", "OPTIONS.MNI");

        if (menuSelection != 0xFF)
        {
          gmSpeedIndex = menuSelection;
        }
        break;

      case 0xFF:
        return;
    }
  }
}


/** Show a debug mode menu with various choices
 *
 * This function is unused in the shipping game, the referenced scripts are
 * still in the game data though. Seems to be part of some debug functionality
 * that was used during development, perhaps also by the beta testers.
 */
bool pascal ShowDebugMenu(byte type)
{
  int menuSelection;

  switch (type)
  {
    case 1:
      if (!sysTecMode)
      {
        ShowScriptedUI("God_Mode_On", "HELP.MNI");
      }
      else
      {
        ShowScriptedUI("God_Mode_Off", "HELP.MNI");
      }

      sysTecMode = !sysTecMode;
      break;

    case 2:
      switch (menuSelection = ShowScriptedUI("Warp", "HELP.MNI"))
      {
        case 0xFF:
          return false;

        default:
          debugLevelToWarpTo = (byte)menuSelection - 5;
          return true;
      }

    case 3:
      menuSelection = ShowScriptedUI("Weapon_Select", "HELP.MNI");

      if (menuSelection == 0xFF)
      {
        // No-op
      }
      else
      {
        plWeapon = menuSelection - 1;
        plAmmo = MAX_AMMO * 4;
        break;
      }

      return false;

    case 4:
      menuSelection = ShowScriptedUI("Skill_Select", "TEXT.MNI");

      if (menuSelection == 0xFF)
      {
        return false;
      }
      else
      {
        gmDifficulty = menuSelection;
        return true;
      }
  }

  return false;
}


/** Show high score list for chosen episode */
void pascal ShowHighScoreList(byte episode)
{
  switch (episode)
  {
    case 1:
      ShowScriptedUI("Volume1", "TEXT.MNI");
      break;

    case 2:
      ShowScriptedUI("Volume2", "TEXT.MNI");
      break;

    case 3:
      ShowScriptedUI("Volume3", "TEXT.MNI");
      break;

    case 4:
      ShowScriptedUI("Volume4", "TEXT.MNI");
      break;
  }

  DrawHighScoreList(episode);
  FadeInScreen();

  AwaitInput();
}


/** Show the apogee logo movie */
void ShowApogeeLogo(void)
{
  FadeOutScreen();
  PlayMenuMusic("FANFAREA.IMF");

  SetVideoMode(0x13);

  PlayVideo("nukem2.f5", VT_APOGEE_LOGO, 255);
  WaitTicks(30);

  SetVideoMode(0xD);
}


void ShowDuke3dTeaserScreen(void)
{
  int i;

  // Only do something if we're in the 4th episode
  if (gmCurrentEpisode < 3) { return; }

  FadeOutScreen();
  SetDrawPage(0);
  SetDisplayPage(0);
  CLEAR_SCREEN();

  LoadSprite(ACT_DUKE_3D_TEASER_TEXT);

  // This loop simultaneously fades in the screen and animates the teaser text
  // moving up.
  for (i = 0; i < 36; i++)
  {
    DrawDuke3dTeaserText(5, 100 - i);

    if (i & 1)
    {
      if (i < 32)
      {
        Duke3dTeaserFadeIn(i);
      }

      WaitTicks(2);
    }
  }

  MM_PopChunks(CT_SPRITE);

  if (AwaitInputOrTimeout(1500) == 0xFE)
  {
    // No-op, but we keep the empty if statement
    // to match the original assembly
  }

  CLEAR_SCREEN();
}


static bool skipStoryInAttractLoop = false;


/** Show the "attact loop" - intro movie, credits, demo, Apogee logo, repeat */
void ShowAttractLoop(void)
{
  for (;;)
  {
    demoPlaybackAborted = false;

    PlayMenuMusic("RANGEA.IMF");

    // Play the intro video. If it was aborted, bail out
    if (ShowIntroVideo()) { return; }

    if (!skipStoryInAttractLoop)
    {
      // We only want to show the story the first time
      skipStoryInAttractLoop = true;
      ShowScriptedUI("&Story", "TEXT.MNI");

      // If the story was aborted, bail out
      if (scriptPageIndex == 0xFF) { return; }
    }

    ShowScriptedUI("&Credits", "TEXT.MNI");
    if (AwaitInputOrTimeout(700) != 0xFE) { return; }

#ifdef SHAREWARE
    ShowScriptedUI("Q_ORDER", "TEXT.MNI");
#endif

    // This delay is for the shareware version's ordering info screen, but it's
    // also in the registered version, making the credits appear for twice as
    // long.
    if (AwaitInputOrTimeout(700) != 0xFE) { return; }

    // Play the demo
    demoIsRecording = false;
    demoIsPlaying = true;
    InitDemoPlayback();
    gmDifficulty = DIFFICULTY_HARD;
    StopMusic();
    MM_PopChunk(CT_MENU_MUSIC);
    ResetPlayerState();
    RunGameSession(4, 0);

    if (demoPlaybackAborted) { return; }

    ShowApogeeLogo();
  }
}


#ifdef REGISTERED_VERSION
/** Check if copy protection failed, and show a message if so */
bool CheckCopyProtection(void)
{
  if (sysCopyProtectionFailed)
  {
    ShowScriptedUI("BAD_GAME", "TEXT.MNI");
    return true;
  }

  return false;
}
#endif


/** Run the menu system and attract loop
 *
 * Thanks to the scripting system, the amount of code in this function is
 * actually fairly little, considering that it implements almost the entire
 * menu system.
 * Keeps running until the player exits the game from the main menu.
 */
static void pascal RunMainLoop(bool skipIntro)
{
  int fd;
  byte menuSelection;

  if (skipIntro == false)
  {
#ifdef REGISTERED_VERSION
    // Show the "anti piracy" message screen
    SetVideoMode(0x13);
    ShowVGAScreen("lcr.mni");
    SetVideoMode(0xD);
#endif

    // Check if the options file exists. If it doesn't, assume that the game
    // has just been launched for the first time and show a "hype" sequence.
    //
    // [NOTE] In the registered version, the call to SetVideoMode() above has
    // erased the status icon tiles that were loaded in InitSubsystems().  Rhe
    // HYPE script doesn't make use of any status icon based functionality like
    // text rendering, which is why this isn't a problem. Not clear if this is
    // intentional, or an oversight.
    fd = OpenFileRW("nukem2.-gt");
    if (fd == -1)
    {
      ShowScriptedUI("HYPE", "TEXT.MNI");
    }
    else
    {
      CloseFile(fd);
    }

    ShowApogeeLogo();

attractLoop:
    ShowAttractLoop();
    skipStoryInAttractLoop = true;

    // Status icons need to be reloaded after a mode switch, since the BIOS
    // clears the video memory.
    LoadStatusIcons();
  }

enterMainMenu:
  gmCurrentEpisode = gmCurrentLevel = 0;
  PlayMenuMusic("DUKEIIA.IMF");

  for (;;)
  {
    switch (ShowScriptedUI("Main_Menu", "TEXT.MNI"))
    {
      case 9: // timed out
        goto attractLoop;
        goto attractLoop;

      case 1: // 'Start A New Game'
selectEpisodeMenu:
        gmCurrentEpisode = menuSelection =
          ShowScriptedUI("Episode_Select", "TEXT.MNI");
        if (menuSelection == 0xFF)
        {
          break;
        }

#ifdef SHAREWARE
        if (gmCurrentEpisode != 1)
        {
          ShowScriptedUI("No_Can_Order", "TEXT.MNI");

          // pre-select 1st episode in menu
          uiMenuSelectionStates[MT_EPISODE_SELECT] = 1;
          goto selectEpisodeMenu;
        }
#endif

        gmCurrentEpisode--;

        menuSelection = ShowScriptedUI("Skill_Select", "TEXT.MNI");
        if (menuSelection == 0xFF)
        {
          goto selectEpisodeMenu;
        }

        gmDifficulty = menuSelection;

        StopMusic();
        MM_PopChunk(CT_MENU_MUSIC);

        ResetPlayerState();
        RunGameSession(gmCurrentEpisode, 0);

        if (gmGameState == GS_EPISODE_FINISHED)
        {
          ShowEpisodeEndScreen();
        }

        TryAddHighScore(gmCurrentEpisode + 1);
        ShowHighScoreList(gmCurrentEpisode + 1);

#ifdef REGISTERED_VERSION
        if (CheckCopyProtection())
        {
          goto enterMainMenu;
        }
#endif

        goto enterMainMenu;

      case 2: // 'Restore A Game'
restoreGameMenu:
        menuSelection = ShowScriptedUI("Restore_Game", "OPTIONS.MNI");

        if (menuSelection != 0xFF)
        {
          if (IsSaveSlotEmpty(menuSelection - 1))
          {
            ShowScriptedUI("No_Game_Restore", "OPTIONS.MNI");
            goto restoreGameMenu;
          }

          ShowScriptedUI("&Load", "TEXT.MNI");
          ReadSavedGame('0' + menuSelection);

          demoIsRecording = false;
          demoIsPlaying = false;

          StopMusic();
          MM_PopChunk(CT_MENU_MUSIC);

          RunGameSession(gmCurrentEpisode, gmCurrentLevel);

          if (gmGameState == GS_EPISODE_FINISHED)
          {
            ShowEpisodeEndScreen();
          }

          TryAddHighScore(gmCurrentEpisode + 1);
          ShowHighScoreList(gmCurrentEpisode + 1);

#ifdef REGISTERED_VERSION
          if (CheckCopyProtection())
          {
            goto enterMainMenu;
          }
#endif

          goto enterMainMenu;
        }

        break;

      case 3: // 'Game Options'
        ShowOptionsMenu();
        break;

      case 4: // 'Ordering Information'
#ifdef SHAREWARE
        ShowScriptedUI("Ordering_Info", "ORDERTXT.MNI");
#else
        ShowScriptedUI("V4ORDER", "TEXT.MNI");
#endif
        break;

      case 5: // 'Instructions And Story'
        menuSelection = ShowScriptedUI("Both_S_I", "TEXT.MNI");

        if (menuSelection == 1)
        {
          ShowScriptedUI("&Instructions", "TEXT.MNI");
        }

        if (menuSelection == 2)
        {
          ShowScriptedUI("&Story", "TEXT.MNI");
        }
        break;

      case 6: // 'High Scores'
selectEpisodeMenuHighScores:
        menuSelection = ShowScriptedUI("Episode_Select", "TEXT.MNI");
        if (menuSelection == 0xFF)
        {
          break;
        }

#ifdef SHAREWARE
        if (menuSelection != 1)
        {
          ShowScriptedUI("No_Can_Order", "TEXT.MNI");

          // pre-select 1st episode in menu
          uiMenuSelectionStates[MT_EPISODE_SELECT] = 1;
          goto selectEpisodeMenuHighScores;
        }
#endif

        ShowHighScoreList(menuSelection);
        break;

      case 7: // 'Credits'
        menuSelection = ShowScriptedUI("&Credits", "TEXT.MNI");
        AwaitInput();
        break;

      case 8: // 'Quit Game'
      case 0xFF: // ESC key
        menuSelection = ShowScriptedUI("Quit_Select", "TEXT.MNI");
        if (menuSelection == 0xFF)
        {
          break;
        }

        if (menuSelection == 1)
        {
          // Quit the game by returning from this loop
          return;
        }
        break;
    }
  }
}


/** Initialize everything
 *
 * Always returns false.
 */
bool Initialize(void)
{
  ReadSaveSlotNames();

  // Initialize the memory manager. This allocates one big block of memory from
  // DOS via the C runtime. After this point, all memory allocations happen
  // through the manager only.
  MM_Init();

  // Load ACTRINFO.MNI so that we can load actor graphics (sprites).
  LoadActorInfo();

  // Initialize everything else
  InitSubsystems();

  // [NOTE] never used
  return false;
}


/** Initialize all systems (except for the memory manager) */
static void InitSubsystems(void)
{
  SB_Init(getenv("BLASTER"));

  ReadOptionsFile();
  LoadSoundEffects();
  InstallTimerInterrupt();

  // [NOTE] This first fade-out happens while still in text mode, which leads to
  // the "DOS prompt turning yellow" effect due to the VGA palette being changed
  // by the fade-out function.  This was maybe not intentional at first but then
  // the developers decided to keep it since it looks kinda cool?
  FadeOutScreen();

  // [NOTE] This first video mode change is unnecessary, since the next thing
  // shown on screen will be either the Apogee logo or the anti-piracy notice,
  // and both of these use a different video mode.  It's likely that development
  // builds of the game featured a way to skip the intro via command line
  // option, in which case this mode switch makes sense. But it could've been
  // removed for the shipping version.  And either way, doing the mode switch in
  // RunMainLoop() would be more appropriate.
  SetVideoMode(0xD);

  LoadCommonSprites();
  InitParticleSystem();
  AllocateBackdropOffsetTable();

  // [NOTE] This is unnecessary, since the loaded status icons will be erased
  // from video memory as soon as we do another video mode switch - which
  // always happens shortly after this function returns, when showing the
  // Apogee logo movie or the anti-piracy notice.
  LoadStatusIcons();

  // Install custom keyboard interrupt handler
  kbSavedIntHandler = _dos_getvect(9);
  _dos_setvect(9, KeyboardHandler);

  // Pretend that the last keyboard event was a key release
  kbLastScancode = 0x80;
}


/** Moves the DOS text mode cursor near the bottom of the screen
 *
 * Used in conjunction with ShowTextScreen().
 */
static void MoveCursorToBottom(void)
{
  _AH = 2;    // Video service 2: Set cursor position
  _BH = 0;    // page
  _DH = 22;   // row
  _DL = 0;    // column
  geninterrupt(0x10); // BIOS interrupt 0x10, video service
}


/** Shut down all systems and exit the game
 *
 * This function cleans everything up and then terminates the program.
 * It's used both for regular exit and to stop in case of severe errors.
 *
 * IMPORTANT: Before calling this, Initialize() must have been called
 * (and finished). This is unfortunately not always guaranteed, see notes
 * in the function body.
 */
void Quit(const char* quitMessage)
{
  // [NOTE] The quitMessage argument is not used within this function.
  // Most likely, there was some alternative code here in development
  // builds of the game, which would print out the quit message.

  // Free all remaining memory
  //
  // [NOTE] The memory is never returned to DOS (missing farfree() call), but it
  // doesn't really matter since the program is about to quit and all memory
  // will be reclaimed by DOS anyway.  But this also means that these two calls
  // are kinda pointless, since nothing makes any use of the memory manager from
  // here on.
  MM_PopChunks(CT_SPRITE);
  MM_PopChunks(CT_COMMON);

  // Restore original keyboard handler
  //
  // [BUG] If Quit is called before saving the keyboard handler, this will get
  // the computer stuck in a non-responsive state, as the keyboard handler will
  // be set to a null pointer.  This could happen if there's an error (e.g.
  // missing file) during initialization.
  _dos_setvect(9, kbSavedIntHandler);

  // Stop any PC speaker sounds
  DN2_outportb(0x61, DN2_inportb(0x61) & 0xFD);

  FadeOutScreen();
  RestoreTimerInterrupt();
  SB_Shutdown();

  // Remove temporary files
  unlink("nukem2.-st");
  unlink("nukem2.-sb");
  unlink("nukem2.-sz");

  // [NOTE] This is the only place where options and save slot names are written
  // to disk. So if the game crashes, the data will be lost. And even though the
  // saved game files themselves are written out sooner, the game will treat
  // them as non-existent if there's no corresponding entry in the save slot
  // name list.
  WriteOptionsFile();
  WriteSaveSlotNames();

  // Switch back to text mode. This also causes the default palette to be
  // restored, which will make the contents of the screen visible again
  // (the screen is still dark at this point due to the fade-out further up).
  SetVideoMode(0x3);

  // Now show the exit text screen and quit
  //
  // [BUG] ShowTextScreen calls OpenAssetFile(), which in turn might call Quit()
  // if it cannot find the requested file. This can cause infinite recursion.
  // One way to easily get into this situation is to run the game without any
  // data files present. The first OpenAssetFile() call the game makes is going
  // to fail, call Quit(), it will get here, then try to find the dos text file,
  // fail, call Quit() again etc.
  //
  // My theory is that in debug builds of the game, there was a
  // printf(quitMessage) here instead of showing the text screen, so this bug
  // might not have been a problem during development.
#ifdef REGISTERED_VERSION
  ShowTextScreen("DOSTEXT2.BIN");
#else
  ShowTextScreen("DOSTEXT.BIN");
#endif

  MoveCursorToBottom();
  _exit(0);
}


/** Show the intro video (Duke Nukem at shooting range) */
bool ShowIntroVideo(void)
{
  int abortedByUser = false;

  SetVideoMode(0x13);
  LoadIntroSoundEffects();

  // The intro video consists of multiple video files,
  // which are played in sequence here. If playback is interrupted
  // by a keypress during any of the individual parts, we stop and
  // indicate it to the caller via the return value.
  if (PlayVideo("nukem2.f2", VT_NEO_LA, 6))
  {
    abortedByUser = true;
  }
  else
  {
    // This seems redundant, but doing a mode switch has the side-effect
    // of clearing the screen and adding a brief delay (on real hardware).
    // It seems that this is deliberately used here as a transition effect
    // between the two scenes.
    SetVideoMode(0x13);

    if (PlayVideo("nukem2.f1", VT_RANGE_1, 10))
    {
      abortedByUser = true;
    }
    else if (PlayVideo("nukem2.f3", VT_RANGE_2, 2))
    {
      abortedByUser = true;
    }
    else if (PlayVideo("nukem2.f4", VT_RANGE_3, 1))
    {
      abortedByUser = true;
    }
  }

  StopAllSound();
  UnloadIntroSoundEffects();
  SetVideoMode(0xD);

  // Status icons need to be reloaded after a mode switch, since the BIOS
  // clears the video memory.
  LoadStatusIcons();

  if (sysTecMode)
  {
    // "tec mode" acts as a god mode, but also triggers some other behavior
    // changes. Apparently it was important during testing to make it appear
    // as if the intro video wasn't aborted by the user, even if it was.
    abortedByUser = false;
  }

  return abortedByUser;
}


/** Prepare copy protection by checking 'file_id.diz' contents
 *
 * This implements a very rudimentary form of copy protection by checking that
 * a file called 'file_id.diz' is present in the game directory and that the
 * contents of the file are as expected.
 *
 * If either of these conditions is not true, the game will switch into a
 * limited mode where it's not possible to play more than a single level at a
 * time. This is caused by sysCopyProtectionFailed getting set to true by
 * this function if it finds anything amiss.
 *
 * The idea with this scheme was to prevent the game's full version from being
 * distributed on BBS servers. file_id.diz files were used in BBS systems to
 * present some information about available downloads, so that users could
 * decide what they wanted to download. Since the registered version's
 * file_id.diz clearly states that the game must not be uploaded to BBSs,
 * someone who wanted to illegally share it would be likely to remove or
 * replace the file in order to make it less obvious that it was pirated
 * software. This would then trip up the game's copy protection system.
 */
#ifdef REGISTERED_VERSION
static void InitCopyProtection(void)
{
  byte buffer[sizeof(EXPECTED_FILE_ID_DIZ)];
  int fd;
  int fileIndex;
  int referenceIndex;

  // First, check if the file exists at all
  fd = OpenFileRW("file_id.diz");
  if (fd == -1)
  {
    sysCopyProtectionFailed = true;
    return;
  }

  // If it does exist, compare it to the reference to see if it's as expected.
  fileIndex = 0;

  CloseFile(fd);

  // [NOTE] We've just opened and closed the file to check if it exists, and now
  // this LoadAssetFile() call is going to open and close it again.  It would be
  // more efficient to do a plain _read() here using the file handle we already
  // have. There's no reason to use LoadAssetFile(), since file_id.diz isn't
  // stored in the group file.
  LoadAssetFile("file_id.diz", buffer);

  for (referenceIndex = 0; referenceIndex < 400; referenceIndex++, fileIndex++)
  {
    if (EXPECTED_FILE_ID_DIZ[referenceIndex] == '*')
    {
      ++referenceIndex;
      fileIndex += 2;
    }

    if (buffer[fileIndex] != EXPECTED_FILE_ID_DIZ[referenceIndex])
    {
      sysCopyProtectionFailed = true;

      // [NOTE] At this point, the loop could be aborted, but it always runs to
      // the end.
    }
  }
}
#endif


int main(int argc, char** argv)
{
  // First, determine how much memory we can allocate from DOS, in order to
  // check if we have enough memory to run the game
  dword availableMem = farcoreleft();

  // We have to load the group file dict here, because the error screen
  // for insufficient memory is stored in the group file (NUKEM2.CMP).
  LoadGroupFileDict();

  // Now check, and bail out if there's not enough memory.
  // Duke 2 doesn't make any use of EMS or XMS, even if available, so having
  // enough conventional memory is important.
  // Thanks to the custom memory manager, the exact memory requirements are
  // known.
  if (availableMem < MM_TOTAL_SIZE)
  {
    // "I'm here to kick ass and chew memory, and I'm all out of memory!"
    ShowTextScreen("NOMEMORY.BIN");
    MoveCursorToBottom();
    _exit(0);
  }

  // We do have enough memory, so let's get things going!
  Initialize();

#ifdef REGISTERED_VERSION
  InitCopyProtection();
#endif

  // This keeps running until the user exits the game from the main menu.
  RunMainLoop(false);

  // Once we're done, clean up everything and exit back to DOS.  This function
  // also shows the text mode ending screen with different content depending on
  // the version of the game (shareware or registered).
  Quit("");
}
