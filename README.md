Lunarchy (fork of CloakV4)
==============

Difference from CloakV4:

- Instead of HUD themes, it's all controlled with HEX colour codes
- CheatHUD is moveable and syncs with 'HUD Color'
- Account Manager - saved accounts move to the top after they are used to log in
- Profiles - save and load module settings
- Global custom fonts - add a `.ttf` to `fonts/`, then select it and its size in
  `Settings` -> `Fonts`; the selection is used by menus, HUDs, chat, formspecs,
  the console, and the cheat UI
- More modules geared for Mineclonia
    - ElytraTakeoff - hold your rockets and double press space
    - ContentPreviewer - shulkerbox, map, enderchest preview when hovering over them
    - CrystalSpam - auto crystal
    - AutoTotem - equips totems automatically
    - AutoArmor - puts on armor automatically
    - AutoWither - builds a wither automatically
        - can also apply nametags too
    - NoArmor - hides player's armor
- Extra Modules
    - ChatPlus - customize the chat position, colour, background and border
    - FPS - shows FPS in the HUD
    - Totems - shows your totem count in the HUD
    - WAILA - shows what block you are looking at
    - Friends - saves friends per server and makes Killaura ignore them
    - LogoutSpots - marks the last observed position of nearby players when they leave
    - DeathMarker - always records the latest death position; the module toggle
      and its `Display` option control whether the waypoint is visible
    - Avoid - teleports 10-15 blocks away when another player enters the
      configured range, preferring a clear destination with safe ground
    - Ignore - hides messages from saved players and supports a priority
      whitelist, whitelist-only mode, Mineclonia `<player>` chat, and `/msg`
      replies; lists can also be managed with `.ignore`
    - BHOP - bunny hop movement
    - FastFall - fall faster
    - HandView - change position and scale of hand
    - NoFire - disables fire effect
    - EquipmentHUD - shows equipment durability
    - Client List - shows clients connected to the server and nearby clients
    - Ping - shows your ping to the server
    - Lag Optimizer - individually disables expensive visuals such as particles,
      ground items, inventory/hand/plant/water/lava animations, fog, clouds,
      post effects, minimap, bossbar, achievements, and animated node tiles
    - InvMove - lets you keep moving when in formspecs
    - Greeter - lets you send welcome or leave messages when clients join
    - Spammer+ - lets you spam from a txt file
- Modules Changed
    - Killaura
        - Mineclonia mace detection by item group or item name
        - Configurable mace target radius and drop height
        - Moves above the selected target for a mace drop, follows the target
          during the drop, and equips the best available torso armor
        - Optional jump/knockback suppression around a mace hit
        - Temporary wall-target noclip uses `PrivBypass` and is cleared when the
          drop ends; it does not enable ordinary noclip permanently
    - Nametags
        - added equipment and blocks away
    - ChatEffects
        - ChatColor can have gradients too
        - Derp, redacted, Reversed and random gradient options
    - Reach
    - Jetpack
    - Flight


<img src="doc/showcase1.png" alt="titlescreen" width="80%" />
<img src="doc/showcase2.png" alt="client" width="80%" />
