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

HUD-related code, part 1

The game itself is redrawn every frame (see game2.c), but the HUD is only drawn
fully after loading a level (and when returning to gameplay from an in-game
menu). During gameplay, only parts of the HUD that have changed are redrawn.

Since the game uses two VGA video pages to implement double-buffering, the HUD
needs to be drawn to both pages at once in order to make it appear persistent
while the game is switching between pages.

*******************************************************************************/


/** Draw or redraw player health display in the HUD */
void pascal HUD_DrawHealth(word health)
{
  word i;

  for (i = 0; i < 8; i++)
  {
    SetDrawPage(gfxCurrentDisplayPage);

    if (health - 1 > i)
    {
      // Filled slice
      DrawStatusIcon_1x2(XY_TO_OFFSET(29, 4), i + 25, 22);
    }
    else
    {
      // Empty slice
      DrawStatusIcon_1x2(XY_TO_OFFSET(30, 4), i + 25, 22);
    }

    SetDrawPage(!gfxCurrentDisplayPage);

    if (health - 1 > i)
    {
      // Filled slice
      DrawStatusIcon_1x2(XY_TO_OFFSET(29, 4), i + 25, 22);
    }
    else
    {
      // Empty slice
      DrawStatusIcon_1x2(XY_TO_OFFSET(30, 4), i + 25, 22);
    }
  }
}


/** Update and draw the "low health" animation in the HUD */
void pascal HUD_DrawLowHealthAnimation(word health)
{
  word i;

  if (health > 1)
  {
    return;
  }

  ++hudLowHealthAnimStep;
  if (hudLowHealthAnimStep == 9)
  {
    hudLowHealthAnimStep = 0;
  }

  for (i = 0; i < 8; i++)
  {
    SetDrawPage(gfxCurrentDisplayPage);
    DrawStatusIcon_1x2(
      T2PX(hudLowHealthAnimStep + i) % (9*8) + XY_TO_OFFSET(20, 4),
      i + 25, 22);

    SetDrawPage(!gfxCurrentDisplayPage);
    DrawStatusIcon_1x2(
      T2PX(hudLowHealthAnimStep + i) % (9*8) + XY_TO_OFFSET(20, 4),
      i + 25, 22);
  }
}


/** Draw or redraw ammo display in the HUD */
void pascal HUD_DrawAmmo(word ammo)
{
  if (ammo > MAX_AMMO)
  {
    // [BUG] The flame thrower max ammo is 64, so for the first 32 shots
    // fired with the flame thrower, the ammo display doesn't change.
    // Correct would be to clamp to 64 and divide by 2 in case the
    // current weapon is the flame thrower.
    ammo = MAX_AMMO;
  }

  // Only even values are represented in the display, since it's only
  // 16 pixels tall.
  if (ammo % 2)
  {
    ammo++;
  }

  // Divide by 2 to get a value between 0 and 16, which serves as index into
  // the status icons representing the different ammo counts.
  // Then, multiply by 8 to get a pixel offset.
  //
  // Full ammo is the leftmost icon, empty on the right.
  // ammo = ammo / 2 * 8;
  ammo <<= 2;

  SetDrawPage(gfxCurrentDisplayPage);
  DrawStatusIcon_1x2(XY_TO_OFFSET(16, 23) - ammo, 23, 22);

  SetDrawPage(!gfxCurrentDisplayPage);
  DrawStatusIcon_1x2(XY_TO_OFFSET(16, 23) - ammo, 23, 22);
}


/** Draw or redraw the weapon type indicator in the HUD */
void pascal HUD_DrawWeapon(int weapon)
{
  // See `case ACT_LASER_TURRET` in HandleActorShotCollision (game3.c)
  plWeapon_hud = weapon;

  // Each weapon icon is 4 tiles wide, i.e. 32 pixels (4 * 8).
  // Here, we turn the weapon type into an offset for the icons,
  // which are laid out from left to right in STATUS.MNI.
  //
  // weapon *= 32;
  weapon <<= 5;

  SetDrawPage(gfxCurrentDisplayPage);
  DrawStatusIcon_2x2(weapon + XY_TO_OFFSET(4, 5), 18, 22);
  DrawStatusIcon_2x2(weapon + XY_TO_OFFSET(6, 5), 20, 22);

  SetDrawPage(!gfxCurrentDisplayPage);
  DrawStatusIcon_2x2(weapon + XY_TO_OFFSET(4, 5), 18, 22);
  DrawStatusIcon_2x2(weapon + XY_TO_OFFSET(6, 5), 20, 22);
}


/** Add item to the player's inventory and update HUD */
void pascal AddInventoryItem(word item)
{
  int i = 0;

  // Find first free inventory slot.
  //
  // [UNSAFE] There is no range checking here, so if the player somehow
  // manages to collect 7 items (someone could make a custom level with 7 keys,
  // for example), this would keep scanning unrelated memory until it finds a
  // 0 value, and then overwrite it.
  while (plInventory[i])
  {
    i++;
  }

  plInventory[i] = item;

  // In case a blinking animation for a recently removed item is still ongoing,
  // erase it.
  plInventory[i + 1] = 0;
  hudInventoryBlinkTimeLeft[i] = 0;

  HUD_DrawInventory();
}


/** Remove item from inventory if present, and update HUD
 *
 * Returns true if the item was successfully removed, false if the item wasn't
 * in the inventory.
 *
 * Also starts an animation of the item blinking for a few frames.
 */
bool pascal RemoveFromInventory(word item)
{
  int i;

  // Search item in the inventory. If the player has multiple instances of the
  // same type of item, this will find the first one.
  for (i = 0; plInventory[i] != item; i++)
  {
    if (plInventory[i] == 0)
    {
      // We didn't find the item
      return false;
    }
  }

  // The item isn't removed immediately, but kept in the inventory in order
  // to display the disappearing animation (blinking).
  // The first time this function is called, there will be no animation playing
  // yet, so it just starts one. Once the animation is finished, this function
  // is called again, and this time it actually removes the item for good.
  if (!hudInventoryBlinkTimeLeft[i])
  {
    hudInventoryBlinkTimeLeft[i] = 10;
  }
  else
  {
    if (hudInventoryBlinkTimeLeft[i] == 1)
    {
      hudInventoryBlinkTimeLeft[i]--;
    }

    // Remove the item, and shift following items up by one
    plInventory[i] = 0;
    while (plInventory[i + 1])
    {
      plInventory[i] = plInventory[i + 1];
      hudInventoryBlinkTimeLeft[i] = hudInventoryBlinkTimeLeft[i + 1];
      i++;
    }

    // Erase the slot for the last item, which has already been copied to
    // position i - 1 at this point by the loop above.
    plInventory[i] = 0;
    hudInventoryBlinkTimeLeft[i] = 0;

    HUD_DrawInventory();
  }

  return true;
}


/** Update blinking animation for removed inventory items */
void pascal HUD_UpdateInventoryAnimation(void)
{
  int i = 0;

  // Map inventory index to position in the grid
  //
  // [PERF] Missing `static` causes copy operations here
  const byte X_POS[6] = { 0, 2, 0, 2, 0, 2 };
  const byte Y_POS[6] = { 0, 0, 2, 2, 4, 4 };

  while (plInventory[i])
  {
    if (hudInventoryBlinkTimeLeft[i] > 1)
    {
      // If this animation was just started by a call to RemoveFromInventory(),
      // we erase (overdraw) the item's icon in the HUD, **but only** for the
      // current draw page. Because the icon graphic is still present in the
      // other draw page, the item now blinks on and off as the game flips
      // between the draw pages each frame.
      if (hudInventoryBlinkTimeLeft[i] == 10)
      {
        DrawStatusIcon_2x2(XY_TO_OFFSET(31, 4), X_POS[i] + 34, Y_POS[i] + 3);
      }

      // Advance the animation timer
      hudInventoryBlinkTimeLeft[i]--;
    }
    else
    {
      // Enough time has passed, now stop the animation and completely remove
      // the item. This is also handled by RemoveFromInventory(), which has
      // a special code path for this case. It will also reset the blink timer
      // for this slot back to 0, to indicate that the animation has finished.
      //
      // [NOTE] It seems a bit hacky to reuse the same function for this.
      // Using a dedicated function for actually removing the item and finishing
      // the animation would be a bit clearer.
      // My guess is that the game started out only having the
      // RemoveFromInventory() function and no animation, and the removal code
      // thus made sense to have there. Later, the animation was added in
      // without restructuring the code too much.
      if (hudInventoryBlinkTimeLeft[i] == 1)
      {
        RemoveFromInventory(plInventory[i]);
      }
    }

    i++;
  }
}


/** Remove all items from the player's inventory */
void pascal ClearInventory(void)
{
  // A 0 value is used as indicator to mark the end of the inventory list. By
  // setting the first entry to 0, we effectively mark the whole inventory as
  // emtpy, without the need to reset all the other slots.
  plInventory[0] = 0;
}
