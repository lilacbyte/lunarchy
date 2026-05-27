Lunarchy (fork of CloakV4)
==============

The executable and package stem is `lunarchy`. Existing user data remains in the
legacy Minetest-compatible locations (`.minetest`/`Minetest` on desktop and the
`net.minetest.minetest` Android package) so upgrading does not create an empty
profile or hide existing worlds, settings, and client data. Translation catalogs
retain their existing `cloakv4` gettext domain for the same compatibility reason.

Difference from CloakV4:

- Instead of HUD themes, it's all controlled with HEX colour codes
- CheatHUD is moveable and syncs with 'HUD Color'
- Account Manager
- Profiles - save and load module settings
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
    - Luna Stats - player stats HUD, nametags and online client list stats
    - ChatPlus - customize the chat position, colour, background and border
    - FPS - shows FPS in the HUD
    - Totems - shows your totem count in the HUD
    - WAILA - shows what block you are looking at
    - Friends - saves friends per server and makes Killaura ignore them
    - LogoutSpots - marks where players leave
    - DeathMarker - marks where you die
    - Avoid - disconnects when chosen players are online
    - BHOP - bunny hop movement
    - FastFall - fall faster
    - HandView - change position and scale of hand
    - NoFire - disables fire effect
    - EquipmentHUD - shows equipment durability
    - Client List - shows clients connected to the server and nearby clients
    - Ping - shows your ping to the server
    - Lag Optimizer - turn off some of the animations (items on ground, hand animation, etc)
    - InvMove - lets you keep moving when in formspecs
    - Greeter - lets you send welcome or leave messages when clients join
    - Spammer+ - lets you spam from a txt file
- Modules Changed
    - Killaura
        - Mace option and mace jump suppress
    - Nametags
        - added equipment, blocks away, Luna Stats etc
    - ChatEffects
        - ChatColor can have gradients too
        - Derp, redacted, Reversed and random gradient options
    - Reach
    - Jetpack
    - Flight

<img src="doc/showcase1.png" alt="titlescreen" width="80%" />
<img src="doc/showcase2.png" alt="client" width="80%" />
