	core.cheats = {
	["Client"] = {
		["quickmenu"] = { setting = "use_old_menu", order = 0 },
		["client list"] = { setting = "clients", order = 1 },
		["ping"] = { setting = "ping", order = 3 },
		["cheathud"] = { setting = "cheat_hud", order = 6 },
		["hud"] = { setting = "hud.enabled", order = 7 },
		["chatplus"] = { setting = "chatplus.enabled", order = 9 },
		["coords"] = { setting = "coords", order = 10 },
		["fps"] = { setting = "fps", order = 12 },
		["totems"] = { setting = "totems", order = 13 },
		["waila"] = { setting = "waila", order = 14 },
		["equipmenthud"] = { setting = "equipment_hud", order = 15 },
		["lagoptimizer"] = { setting = "lag_optimizer", order = 16 },
	},
	["Render"] = {
		["fullbright"] = "fullbright",
		["brightnight"] = "no_night",
		["xray"] = "xray",
		["entityesp"] = "enable_entity_esp",
		["entitytracers"] = "enable_entity_tracers",
		["playeresp"] = "enable_player_esp",
		["playertracers"] = "enable_player_tracers",
		["nodeesp"] = "enable_node_esp",
		["nodetracers"] = "enable_node_tracers",
		["nodehighlight"] = "nodehighlight",
		["tunnelesp"] = "enable_tunnel_esp",
		["tunneltracers"] = "enable_tunnel_tracers",
	--	["hudbypass"] = "hud_flags_bypass", dont wanna work, will fix later
		["healthesp"] = "enable_health_esp",
		["targethud"] = "enable_combat_target_hud",
		["lefthand"] = "left_hand",
		["contentpreviewer"] = "content_previewer",
		["nametags"] = "nametags",
	["fov"] = "fov_setting",
    },
	["Visuals"] = {
		["nodrowncam"] = "no_drown_cam",
		["nohurtcam"] = "no_hurt_cam",
		["nofire"] = "no_fire",
		["handview"] = "hand_view",
		["noarmor"] = "no_armor",
		["no clouds"] = "no_clouds",
		["no fog"] = "no_fog",
		["clearer water"] = "translucent_liquids",
		["no particles"] = "norender.particles",
	},
	["Player"] = {
		["privbypass"] = "priv_bypass",
		["nofalldamage"] = "prevent_natural_damage",
		["reach"] = "reach",
		["autorespawn"] = "autorespawn",
	--	["luacontrol"] = "lua_control",
		["noforcerotate"] = "no_force_rotate",
    },
	["Movement"] = {
		["bhop"] = { setting = "BHOP", order = 0 },
		["freecam"] = { setting = "freecam", order = 1 },
		["flight"] = "free_move",
		["invmove"] = "invmove",
		["autoforward"] = "continuous_forward",
		["pitchmove"] = "pitch_move",
		["autojump"] = "autojump",
		["noclip"] = "noclip",
		["fastmove"] = "fast_move",
			["jesus"] = "jesus",
			["noslow"] = "no_slow",
			["noslowplus"] = "no_slow_plus",
			["jetpack"] = "jetpack",
		["jump"] = "jump",
		["antislip"] = "antislip",
		["airjump"] = "airjump",
		["spider"] = "spider",
		["autosneak"] = "autosneak",
		["step"] = "step",
    },
	["Combat"] = {
		["antiknockback"] = "antiknockback",
		["attachmentfloat"] = "float_above_parent",
    },
	["Interact"] = {
		["blink"] = "blink",
		["fasthit"] = "spamclick",
		["autohit"] = "autohit",
		["fastplace"] = "fastplace",
		["autoplace"] = "autoplace",
		["autodig"] = "autodig",
		["fastdig"] = "fastdig",
		["instantbreak"] = "instant_break",
		["autotool"] = "autotool",
    },
	["Misc"] = {
		["autostaff"] = "autostaff",
		["antiafk"] = "anti_afk",
		["greeter"] = "greeter",
		["friends"] = { setting = "friends.middle_click", order = 4 },
		["logoutspots"] = { setting = "logoutspots" },
		["deathmarker"] = { setting = "deathmarker" },
		["spammer+"] = "spammer_plus",
    }
}
-----------------------------------------------------------REGISTER CHEATS-----------------------------------------------------------
function core.register_cheat(cheatname, category, func)
	core.cheats[category] = core.cheats[category] or {}
	core.cheats[category][cheatname] = func
end
-----------------------------------------------------------CHEAT SETTINGS-----------------------------------------------------------
core.cheat_settings = {}

function core.register_cheat_setting(setting_name, parent_category, parent_setting, setting_id, setting_data)
	 --settingname is the formatted setting name, e.g "Assist Mode"
	 --parent_category is the category of the parent setting, e.g "Combat", 
	 --parent_setting is the cheat this setting is for, e.g "autoaim", 
	 --setting_id is the setting string, e.g "autoaim.mode", 
	 --setting_data is the setting table, e.g 
	 --if its a bool,         {type="bool"}
	 --if its an int slider,  {type="slider_int", min=0, max=10, steps=10}
	 --if its a float slider, {type="slider_float", min=0.0, max=10.0, steps=100}
     --if its a text field,   {type="text", size=10}
	 --if its a selectionbox, {type="selectionbox", options={"lock", "assist"}}
	core.cheat_settings[parent_category] = core.cheat_settings[parent_category] or {}
	core.cheat_settings[parent_category][parent_setting] = core.cheat_settings[parent_category][parent_setting] or {}

	core.cheat_settings[parent_category][parent_setting][setting_id] = {
        name = setting_name,
        type = setting_data.type,
        min = setting_data.min,
        max = setting_data.max,
        steps = setting_data.steps,
		size = setting_data.size,
		options = setting_data.options,
		options_from = setting_data.options_from
    }
end
-----------------------------------------------------------CHEAT INFOTEXTS-----------------------------------------------------------
core.infotexts = {}
local infotext_cache = {}


function core.register_cheat_with_infotext(cheatname, category, func, infotext)
	core.infotexts[category] = core.infotexts[category] or {}	
	core.infotexts[category][cheatname] = infotext	
	core.register_cheat(cheatname, category, func)	
end
	
function core.update_infotext(cheatname, category, func, infotext)
	core.infotexts[category] = core.infotexts[category] or {}	
	local cache_key = category .. "\0" .. cheatname
	if infotext_cache[cache_key] == infotext then
		return
	end
	infotext_cache[cache_key] = infotext
	core.infotexts[category][cheatname] = infotext
	core.update_infotexts()
end
-----------------------------------------------------------CHEAT DESCRIPTIONS-----------------------------------------------------------
core.descriptions = {}

function core.register_cheat_with_description(cheatname, category, func, description)
	core.descriptions[category] = core.descriptions[category] or {}
	core.descriptions[category][cheatname] = description
	core.get_description()
end

function core.register_cheat_description(cheatname, category, func, description)
	core.descriptions[category] = core.descriptions[category] or {}
	core.descriptions[category][cheatname] = description
	core.get_description()
end
-----------------------------------------------------------PANIC-----------------------------------------------------------
function core.panic()
	for category_name, category in pairs(minetest.cheats) do
		for cheat_name, cheat in pairs(category) do
			local disable_cheats = minetest.cheats[category_name][cheat_name]
			if type(disable_cheats) == "string" then
				core.settings:set(disable_cheats, "false")
			elseif type(disable_cheats) == "table" and disable_cheats.setting then
				core.settings:set(disable_cheats.setting, "false")
			end
		end
	end
end
core.register_cheat("panic", "Misc", core.panic)
-----------------------------------------------------------TESTS, PRESET VALUES, ETC-----------------------------------------------------------

--Combat
core.register_cheat_description("antiknockback", "Combat", "antiknockback", "Ignore knockback")
core.register_cheat_description("attachmentfloat", "Combat", "float_above_parent", "Puts the camera one node higher when attached to an entity")
	core.register_cheat_description("autototem", "Combat", "autototem", "Automatically puts a totem in your offhand")
	core.register_cheat_description("autoaim", "Combat", "autoaim", "Aims at a specified target")
	core.register_cheat_description("combatlog", "Combat", "be_a_bitch", "Logs off when certain HP is reached")
	core.register_cheat_description("criticals", "Combat", "critical_hits", "Does critical hits in mcl2/mcla")
	core.register_cheat_description("crystalspam", "Combat", "crystalspam", "Puts end crystals under nearby players or entities")
core.register_cheat_description("autowither", "Combat", "autowither", "Completes a wither structure and places the skulls")
core.register_cheat_setting("nametag", "Combat", "autowither", "autowither.nametag", {type="bool"})
core.register_cheat_description("killaura", "Combat", "killaura", "Attacks a specified target. Silent mode is recommended in PVP servers, as it makes Killaura undetectable")
core.register_cheat_description("maceaura", "Combat", "killaura.mace", "Enable Killaura's mace attack mode")
core.register_cheat_description("orbit", "Combat", "orbit", "Moves around a specified target")
core.register_cheat_description("triggerbot", "Combat", "tbot", "Automatically punch when aiming at an entity")
--Interact
core.register_cheat_description("fastdig", "Interact", "fastdig", "No block break cooldown")
core.register_cheat_with_infotext("blink", "Interact", "blink", "0ms")
core.register_cheat_description("blink", "Interact", "blink", "Delay sending of packets until this cheat is disabled.")
core.register_cheat_description("fastplace", "Interact", "fastplace", "No block placement cooldown")
core.register_cheat_description("autodig", "Interact", "autodig", "Player can dig blocks without mouse press")
core.register_cheat_description("autoplace", "Interact", "autoplace", "Auto place blocks")
core.register_cheat_description("instantbreak","Interact", "instant_break", "Instantly break blocks regardless of tool used")
core.register_cheat_description("fasthit", "Interact", "spamclick", "Hit faster while holding")
core.register_cheat_description("autohit","Interact", "autohit", "Auto hit when looking at entity")
core.register_cheat_description("autotool", "Interact", "autotool", "Selects the best tool for an action")
--Inventory
core.register_cheat_description("enderchest", "Misc", minetest.open_enderchest, "Preview enderchest content in mcl/mcla")
core.register_cheat_description("hand", "Misc", minetest.open_handslot, "Open hand formspec in mcl/mcla")
--Misc
core.register_cheat_description("antiafk", "Misc", "anti_afk", "Prevent afk by moving")
core.register_cheat_description("autostaff", "Misc", "autostaff", "Automatically check player privs and assign them as a staff. WARNING: can be detected easily")
core.register_cheat_setting("warn staff", "Misc", "autostaff", "autostaff.warn_staff", {type="bool"})
core.register_cheat_description("autoteam", "Misc", "autoteam", "Sets allied players to your team in ctf. It might require you to run /team in some servers")
core.register_cheat_description("logoutspots", "Misc", "logoutspots", "Displays recorded logout positions as waypoints")
core.register_cheat_setting("Range", "Misc", "logoutspots", "logoutspots.range", {type="slider_int", min=32, max=256, steps=29})
core.register_cheat_setting("Limit", "Misc", "logoutspots", "logoutspots.limit", {type="slider_int", min=1, max=100, steps=100})
core.register_cheat_setting("Scale", "Misc", "logoutspots", "logoutspots.scale", {type="slider_float", min=0.1, max=2.0, steps=20})
core.register_cheat_setting("Display", "Misc", "logoutspots", "logoutspots.display", {type="bool"})
core.register_cheat_description("deathmarker", "Misc", "deathmarker", "Displays your latest recorded death position as a waypoint")
core.register_cheat_setting("Display", "Misc", "deathmarker", "deathmarker.display", {type="bool"})
core.register_cheat_setting("Limit", "Misc", "deathmarker", "deathmarker.limit", {type="slider_int", min=1, max=100, steps=100})
core.register_cheat_description("friends", "Misc", "friends.middle_click", "Toggle a pointed player in the server friend list")
core.register_cheat_setting("Middle click", "Misc", "friends.middle_click", "friends.middle_click", {type="bool"})
core.register_cheat_setting("Friends", "Misc", "friends.middle_click", "friends.list", {type="text", size=80})
core.register_cheat_setting("Ignore", "Misc", "friends.middle_click", "friends.ignore", {type="bool"})
core.register_cheat_description("panic", "Misc", "panic", "Disables all cheats")
core.register_cheat_description("greeter", "Misc", "greeter", "Sends welcome and goodbye messages for joined or leaving clients")
core.register_cheat_description("spammer+", "Misc", "spammer_plus", "Loops through chat messages, including multiline text")
--Movement
core.register_cheat_description("airjump", "Movement", "airjump", "Jump on air")
core.register_cheat_description("antislip", "Movement", "antislip", "Walk on slippery blocks without slipping")
core.register_cheat_description("autoforward", "Movement", "continuous_forward", "Walk forward automatically")
core.register_cheat_description("autojump", "Movement", "autojump", "Jump automatically")
core.register_cheat_description("autosneak", "Movement", "autosneak", "Always sneak")
core.register_cheat_description("fastmove", "Movement", "fast_move", "Toggle fast (req. PrivBypass)")
core.register_cheat_with_infotext("flight", "Movement", "free_move", "")
core.register_cheat_description("flight", "Movement", "free_move", "Toggle flight (req. PrivBypass)")
core.register_cheat_description("freecam", "Movement", "freecam", "Spectator mode")
core.register_cheat_description("invmove", "Movement", "invmove", "Move while a formspec is open")
core.register_cheat_description("jesus", "Movement", "jesus", "Walk on liquids")
core.register_cheat_with_infotext("jetpack", "Movement", "jetpack", "")
core.register_cheat_description("jetpack", "Movement", "jetpack", "AirJump with adjustable speed")
core.register_cheat_with_infotext("jump", "Movement", "jump", "")
core.register_cheat_description("jump", "Movement", "jump", "Adjust normal jump height")
core.register_cheat_description("bhop", "Movement", "BHOP", "Boost movement acceleration while moving")
core.register_cheat_description("detachedcamera", "Movement", "detached_camera", "Detach the camera from the player")
core.register_cheat_description("noslow", "Movement", "noslow", "Sneaking doesn't slow you down")
core.register_cheat_description("noslowplus", "Movement", "no_slow_plus",
	"Prevents cobweb, vine, liquid and node movement slowdown")
core.register_cheat_description("noclip", "Movement", "noclip", "Walk through walls (req. PrivBypass)")
core.register_cheat_description("overrides", "Movement", "overrides", "Movement overrides")
core.register_cheat_description("pitchmove", "Movement", "pitch_move", "While flying, you move where you're pointing")
core.register_cheat_description("spider", "Movement", "spider", "Climb walls")
core.register_cheat_description("step", "Movement", "step", "Climbs the block you're facing")
core.register_cheat_description("velocity", "Movement", "velocity", "Various velocity overrides")
--Player
core.register_cheat_description("autorespawn", "Player", "autorespawn", "Respawn after dying. Singleplayer only")
core.register_cheat_description("nofalldamage", "Player", "prevent_natural_damage", "Receive no fall damage")
core.register_cheat_description("noforcerotate", "Player", "noforcerotate", "Prevent server from changing the player's view direction")
core.register_cheat_description("privbypass", "Player", "priv_bypass", "Bypass fly, noclip, fast and wireframe rendering")
core.register_cheat_with_infotext("reach", "Player", "reach", "")
core.register_cheat_description("reach", "Player", "reach", "Increase reach")
-- core.register_cheat_description("LuaControl", "Player", "luacontrol", "The player moves regardless of the received input")
core.register_cheat_description("quickmenu", "Client", "use_old_menu", "Add a menu for quicker access to cheats")
core.register_cheat_description("client list", "Client", "clients", "Show online and nearby players")
core.register_cheat_setting("mode", "Client", "clients", "clients.mode", {type="selectionbox", options={"online", "nearby", "both"}})
core.register_cheat_setting("nearby distance", "Client", "clients", "nearby_clients.distance", {type="bool"})
core.register_cheat_setting("nearby health", "Client", "clients", "nearby_clients.health", {type="bool"})
core.register_cheat_description("ping", "Client", "ping", "Show client RTT in milliseconds")
core.register_cheat_description("cheathud", "Client", "cheat_hud", "List enabled cheats")
core.register_cheat_description("chatplus", "Client", "chatplus.enabled", "ChatPlus overlay")
core.register_cheat_setting("x offset", "Client", "chatplus.enabled", "chatplus_offset_x", {type="slider_int", min=-500, max=500, steps=200})
core.register_cheat_setting("y offset", "Client", "chatplus.enabled", "chatplus_offset_y", {type="slider_int", min=-500, max=500, steps=200})
core.register_cheat_setting("background", "Client", "chatplus.enabled", "chatplus_background", {type="bool"})
core.register_cheat_setting("background color", "Client", "chatplus.enabled", "chatplus_background_color", {type="text", size=18})
core.register_cheat_setting("background alpha", "Client", "chatplus.enabled", "chatplus_background_alpha", {type="slider_int", min=0, max=255, steps=255})
core.register_cheat_setting("border", "Client", "chatplus.enabled", "chatplus_border", {type="bool"})
core.register_cheat_setting("border color", "Client", "chatplus.enabled", "chatplus_border_color", {type="text", size=18})
core.register_cheat_setting("border alpha", "Client", "chatplus.enabled", "chatplus_border_alpha", {type="slider_int", min=0, max=255, steps=255})
core.register_cheat_setting("padding", "Client", "chatplus.enabled", "chatplus_padding", {type="slider_int", min=0, max=20, steps=20})
core.register_cheat_setting("order", "Client", "cheat_hud", "cheat_hud.order", {type="selectionbox", options={"ascending", "descending"}})
core.register_cheat_setting("by length", "Client", "cheat_hud", "cheat_hud.by_length", {type="bool"})
core.register_cheat_setting("y offset", "Client", "cheat_hud", "cheat_hud.offset", {type="slider_int", min=0, max=200, steps=41})
core.register_cheat_setting("text align", "Client", "cheat_hud", "cheat_hud.align", {type="selectionbox", options={"Left", "Center", "Right"}})
core.register_cheat_description("hud", "Client", "hud.enabled", "Configure HUD colors, backgrounds, and spacing")
core.register_cheat_description("coords", "Client", "coords", "Render coordinates in the bottom left corner")
core.register_cheat_description("fps", "Client", "fps", "Show current frames per second")
core.register_cheat_description("totems", "Client", "totems", "Show total totems in inventory and offhand")
core.register_cheat_description("waila", "Client", "waila", "Show the node or entity you are looking at")
core.register_cheat_description("equipmenthud", "Client", "equipment_hud", "Shows armor and held item durability")
core.register_cheat_description("lagoptimizer", "Client", "lag_optimizer", "Reduce visual lag, first-person motion, inventory animations, and item motion")
--Render
core.register_cheat_description("brightnight", "Render", "no_night", "Always daytime")
core.register_cheat_description("entityesp", "Render", "enable_entity_esp", "See entities through walls")
core.register_cheat_description("entitytracers", "Render", "enable_entity_tracers", "Draw tracers to entities")
core.register_cheat_description("fullbright", "Render", "fullbright", "No darkness")
core.register_cheat_description("left hand", "Render", "left_hand", "Switch to left hand")
core.register_cheat_with_infotext("fov", "Render", "fov_setting", "FOV")
core.register_cheat_description("fov", "Render", "fov_setting", "Have your FOV set to a custom value")
--core.register_cheat_description("HUDBypass", "Render", "hudbypass", "Allows player to toggle hud elements disabled by the game")
core.register_cheat_description("healthesp", "Render", "show_players_hp", "Shows player and entity HP")
core.register_cheat_description("nodeesp", "Render", "enable_node_esp", "See specified nodes through walls")
core.register_cheat_description("nodetracers", "Render", "enable_node_tracers", "Draw tracers to specified nodes")
core.register_cheat_description("nodehighlight", "Render", "nodehighlight", "Style the node you are looking at with the HUD theme")
core.register_cheat_description("playeresp", "Render", "enable_player_esp", "See players through walls")
core.register_cheat_description("playertracers", "Render", "enable_player_tracers", "Draw tracers to players")
core.register_cheat_description("tunnelesp", "Render", "enable_tunnel_esp", "See tunnels through walls")
core.register_cheat_description("tunneltracers", "Render", "enable_tunnel_tracers", "Draw tracers to tunnels")
core.register_cheat_description("xray", "Render", "xray", "Don't render specific nodes")
core.register_cheat_description("targethud", "Render", "enable_combat_target_hud", "Shows best target on a HUD (depends on your combat settings)")
core.register_cheat_description("nametags", "Render", "nametags", "Customize players nametags, equipment, wielded items, and item names. Doesn't work well in CTF")
core.register_cheat_description("lefthand", "Render", "left_hand", "Switch to left hand")
--Visuals
core.register_cheat_description("nodrowncam", "Visuals", "no_drown_cam", "Disables drowning camera effect")
core.register_cheat_description("nohurtcam", "Visuals", "no_hurt_cam", "Disables hurt camera effect")
core.register_cheat_description("nofire", "Visuals", "no_fire", "Disables the burning fire HUD")
core.register_cheat_description("handview", "Visuals", "hand_view", "Adjust the wielded hand position and scale")
core.register_cheat_description("noarmor", "Visuals", "no_armor", "Hide armor on player models and nametags")
core.register_cheat_description("no clouds", "Visuals", "no_clouds", "Disables cloud rendering")
core.register_cheat_description("no fog", "Visuals", "no_fog", "Disables fog rendering")
core.register_cheat_description("clearer water", "Visuals", "translucent_liquids", "Makes liquids clearer")
core.register_cheat_description("no particles", "Visuals", "norender.particles", "Disables particle rendering")
--World
core.register_cheat_description("autotnt", "World", "autotnt", "Puts TNT on the ground")
core.register_cheat_description("blocklava", "World", "blocklava", "Replace lava with the block you're holding")
core.register_cheat_description("blockwater", "World", "blockwater", "Replace water with the block you're holding")
core.register_cheat_description("replace", "World", "replace", "When you break a block it gets replaced by the block you're holding")
core.register_cheat_description("scaffold", "World", "scaffold", "Puts blocks below you")
core.register_cheat_description("scaffoldplus", "World", "scaffoldplus", "Puts even more blocks under you")


--SOME SETTINGS

core.register_cheat_setting("Nodelist", "Render", "xray", "xray.nodes", {type="text", size=10})
core.register_cheat_setting("Nodelist", "Render", "enable_node_esp", "enable_node_esp.nodes", {type="text", size=10})
core.register_cheat_setting("multiplier", "Movement", "step", "step.mult", {type="slider_float", min=1.0, max=3.5, steps=6})
core.register_cheat_setting("range", "Player", "reach", "reach.range",
	{type="slider_float", min=1.0, max=6.0, steps=100})
core.register_cheat_setting("jump", "Movement", "BHOP", "BHOP.jump", {type="bool"})
core.register_cheat_setting("sprint", "Movement", "BHOP", "BHOP.sprint", {type="bool"})
core.register_cheat_setting("speed", "Movement", "BHOP", "BHOP.speed", {type="bool"})
core.register_cheat_setting("disable on damage", "Movement", "freecam", "freecam.disable_on_damage", {type="bool"})
core.register_cheat_setting("flight speed", "Movement", "free_move", "free_move.speed",
	{type="slider_float", min=0.25, max=8.00, steps=155})
core.register_cheat_setting("jetpack speed", "Movement", "jetpack", "jetpack.speed",
	{type="slider_float", min=0.25, max=8.00, steps=155})
core.register_cheat_setting("multiplier", "Movement", "jump", "jump.multiplier",
	{type="slider_float", min=0.25, max=8.00, steps=155})
core.register_cheat_setting("accent color", "Client", "hud.enabled", "hud.accent_color", {type="text", size=18})
core.register_cheat_setting("text color", "Client", "hud.enabled", "hud.text_color", {type="text", size=18})
core.register_cheat_setting("background color", "Client", "hud.enabled", "hud.background_color", {type="text", size=18})
core.register_cheat_setting("background alpha", "Client", "hud.enabled", "hud.background_alpha", {type="slider_int", min=0, max=255, steps=256})
core.register_cheat_setting("border color", "Client", "hud.enabled", "hud.border_color", {type="text", size=18})
core.register_cheat_setting("border alpha", "Client", "hud.enabled", "hud.border_alpha", {type="slider_int", min=0, max=255, steps=256})
core.register_cheat_setting("padding", "Client", "hud.enabled", "hud.padding", {type="slider_int", min=0, max=20, steps=21})
core.register_cheat_setting("tooltips theme", "Client", "hud.enabled", "hud.tooltips_theme", {type="bool"})
core.register_cheat_setting("font", "Client", "hud.enabled", "font_path_hd",
	{type="selectionbox", options_from="fonts"})
core.register_cheat_setting("font size", "Client", "hud.enabled", "hd_font_size",
	{type="slider_int", min=5, max=72, steps=68})
core.register_cheat_setting("background", "Client", "cheat_hud", "cheat_hud.background", {type="bool"})
core.register_cheat_setting("background color", "Client", "cheat_hud", "hud.background_color", {type="text", size=18})
core.register_cheat_setting("background alpha", "Client", "cheat_hud", "hud.background_alpha", {type="slider_int", min=0, max=255, steps=256})
core.register_cheat_setting("reset", "Client", "cheat_hud", "cheat_hud.reset", {type="bool"})
core.register_cheat_setting("handview", "Visuals", "hand_view", "hand_view", {type="bool"})
core.register_cheat_setting("x offset", "Visuals", "hand_view", "hand_view.x", {type="slider_float", min=-100.0, max=100.0, steps=200})
core.register_cheat_setting("y offset", "Visuals", "hand_view", "hand_view.y", {type="slider_float", min=-100.0, max=100.0, steps=200})
core.register_cheat_setting("z offset", "Visuals", "hand_view", "hand_view.z", {type="slider_float", min=-100.0, max=100.0, steps=200})
core.register_cheat_setting("scale", "Visuals", "hand_view", "hand_view.scale", {type="slider_float", min=0.10, max=3.00, steps=58})
core.register_cheat_setting("color", "Visuals", "hand_view", "hand_view.color", {type="text", size=18})
core.register_cheat_setting("type", "Render", "enable_health_esp", "enable_health_esp.type", {type="selectionbox", options={"Health Bar", "Above Head"}})
core.register_cheat_setting("players only", "Render", "enable_health_esp", "enable_health_esp.players_only", {type="bool"})
core.register_cheat_setting("Solid", "Render", "enable_player_esp", "playeresp.solid", {type="bool"})
core.register_cheat_setting("target highlight", "Render", "enable_combat_target_hud", "enable_combat_target_hud.target_highlight", {type="bool"})
core.register_cheat_setting("target mode", "Combat", "crystalspam", "crystalspam.target_mode", {type="selectionbox", options={"Players", "Entities", "Both"}})
core.register_cheat_setting("target radius", "Combat", "crystalspam", "crystalspam.target_radius",
	{type="slider_int", min=5, max=20, steps=16})
core.register_cheat_setting("safe", "Combat", "crystalspam", "crystalspam.safe", {type="bool"})
core.register_cheat_setting("3x3", "Combat", "crystalspam", "crystalspam.3x3", {type="bool"})
core.register_cheat_setting("hp", "Render", "nametags", "nametags.hp", {type="bool"})
core.register_cheat_setting("status marker", "Render", "nametags", "nametags.status", {type="bool"})
core.register_cheat_setting("distance", "Render", "nametags", "nametags.distance", {type="bool"})
core.register_cheat_setting("self", "Render", "nametags", "nametags.self", {type="bool"})
core.register_cheat_setting("icon scale", "Render", "nametags", "nametags.icon_scale", {type="slider_float", min=0.50, max=4.00, steps=70})
core.register_cheat_setting("item names", "Render", "nametags", "nametags.item_names", {type="bool"})
core.register_cheat_setting("equipment", "Render", "nametags", "nametags.armor", {type="bool"})
core.register_cheat_setting("wielded items", "Render", "nametags", "nametags.wielded_items", {type="bool"})
core.register_cheat_setting("background", "Render", "nametags", "show_nametag_backgrounds", {type="bool"})
core.register_cheat_setting("height", "Render", "nametags", "nametags.height", {type="slider_int", min=1, max=9, steps = 9});
core.register_cheat_setting("field of view", "Render", "fov_setting", "fov.step", {type="slider_int", min=72, max=160, steps = 89});
core.register_cheat_setting("background", "Client", "equipment_hud", "equipment_hud.background", {type="bool"})
core.register_cheat_setting("background", "Client", "coords", "coords.background", {type="bool"})
core.register_cheat_setting("background", "Client", "fps", "fps.background", {type="bool"})
core.register_cheat_setting("background", "Client", "totems", "totems.background", {type="bool"})
core.register_cheat_setting("background", "Client", "waila", "waila.background", {type="bool"})
core.register_cheat_setting("background", "Client", "ping", "ping.background", {type="bool"})
core.register_cheat_setting("background", "Client", "clients", "clients.background", {type="bool"})
core.register_cheat_setting("nether coords", "Client", "coords", "coords.nether_coords", {type="bool"})
core.register_cheat_setting("durability", "Client", "equipment_hud", "equipment_hud.durability_mode", {type="selectionbox", options={"Percent", "Dur/Max", "Both"}})
core.register_cheat_setting("enderchest", "Render", "content_previewer", "content_previewer.enderchest", {type="bool"})
core.register_cheat_setting("shulker", "Render", "content_previewer", "content_previewer.shulker", {type="bool"})
core.register_cheat_setting("maps", "Render", "content_previewer", "content_previewer.maps", {type="bool"})

core.register_cheat_setting("Min Length", "Render", "enable_tunnel_esp", "tunnel_esp_min_length", {type="slider_int", min=1, max=10, steps=10})
core.register_cheat_setting("Max Width", "Render", "enable_tunnel_esp", "tunnel_esp_max_width", {type="slider_int", min=1, max=5, steps=5})
core.register_cheat_setting("Max Height", "Render", "enable_tunnel_esp", "tunnel_esp_max_height", {type="slider_int", min=1, max=5, steps=5})

local update_interval = 0.25
local timer = 0
local blinktime = 0
local friends_sync_server = ""
local friends_sync_value = ""

local function get_friends_json()
	local data = core.settings:get_json("friends") or {}
	if type(data) ~= "table" then
		data = {}
	end
	return data
end

local function get_current_friends_list()
	local server_url = core.get_server_url()
	if not server_url then
		return "", ""
	end

	local friends = get_friends_json()
	local value = friends[server_url]
	if type(value) ~= "string" then
		value = ""
	end

	return server_url, value
end

local function set_current_friends_list(value)
	local server_url = core.get_server_url()
	if not server_url then
		return
	end

	local friends = get_friends_json()
	friends[server_url] = value or ""
	core.settings:set_json("friends", friends)
end

local function reset_cheat_hud()
	core.settings:set_bool("cheat_hud", true)
	core.settings:set_bool("cheat_hud.reset", false)
	core.settings:remove("HudElement_Position1_cheathud")
	core.settings:remove("HudElement_Position2_cheathud")
	core.settings:remove("cheat_hud.offset")
	core.settings:remove("cheat_hud.align")
	core.settings:remove("cheat_hud.position")
	core.settings:remove("cheat_hud.mode")
end

local function make_lua_control(control, jump_override)
	return {
		up = control.up or false,
		down = control.down or false,
		left = control.left or false,
		right = control.right or false,
		jump = jump_override ~= nil and jump_override or (control.jump or false),
		aux1 = control.aux1 or false,
		sneak = control.sneak or false,
		zoom = control.zoom or false,
		dig = control.dig or false,
		place = control.place or false,
	}
end

minetest.register_globalstep(function(dtime)
    timer = timer + dtime

	local server_url, stored_value = get_current_friends_list()
	if server_url ~= "" and server_url ~= friends_sync_server then
		friends_sync_server = server_url
		friends_sync_value = stored_value
		if (core.settings:get("friends.list") or "") ~= stored_value then
			core.settings:set("friends.list", stored_value)
		end
	elseif server_url ~= "" then
		local current_value = core.settings:get("friends.list") or ""
		if current_value ~= friends_sync_value then
			friends_sync_value = current_value
			set_current_friends_list(current_value)
		end
	end

	if core.settings:get_bool("blink") then
		blinktime = blinktime + dtime
	core.update_infotext("blink", "Interact", "blink", math.floor(blinktime * 1000) .. "ms")
		if blinktime > 10 then
			core.settings:set_bool("blink", false)
		end
	else
		blinktime = 0
	end

	if core.settings:get_bool("cheat_hud.reset") then
		reset_cheat_hud()
	end

    if timer >= update_interval then
        timer = 0

        local function format_amount(value)
            local text = string.format("%.2f", tonumber(value) or 0)
            text = text:gsub("0+$", "")
            text = text:gsub("%.$", "")
            return text
        end

        -- Step infotext
        core.update_infotext("step", "Movement", "step",
            core.settings:get_bool("step") and format_amount(core.settings:get("step.mult")) or "")
        core.update_infotext("reach", "Player", "reach",
            core.settings:get_bool("reach") and format_amount(core.settings:get("reach.range")) or "")
        core.update_infotext("flight", "Movement", "free_move",
            core.settings:get_bool("free_move") and format_amount(core.settings:get("free_move.speed")) or "")
        core.update_infotext("jetpack", "Movement", "jetpack",
            core.settings:get_bool("jetpack") and format_amount(core.settings:get("jetpack.speed")) or "")
        core.update_infotext("jump", "Movement", "jump",
            core.settings:get_bool("jump") and format_amount(core.settings:get("jump.multiplier")) or "")

        -- CombatLog infotext
        core.update_infotext("combatlog", "Combat", "combatlog", "Min HP: " .. core.settings:get("combatlog.hp"))

        -- FOV infotext
        if core.settings:get_bool("fov_setting") then
            core.update_infotext("fov", "Render", "fov_setting", core.settings:get("fov.step"))
        else
            core.update_infotext("fov", "Render", "fov_setting", "")
        end

        -- Nametags infotext
        local nametags_enabled = core.settings:get_bool("nametags")
        local nametags_hp = core.settings:get_bool("nametags.hp")
        local nametags_status = core.settings:get_bool("nametags.status")
        local nametags_distance = core.settings:get_bool("nametags.distance")
        if nametags_enabled then
            core.update_infotext("nametags", "Render", "nametags", "")
        end

        -- Friends infotext
        if core.settings:get_bool("friends.ignore") then
            core.update_infotext("friends", "Misc", "friends.middle_click", "ignored")
        else
            core.update_infotext("friends", "Misc", "friends.middle_click", "")
        end

		--Scaffold infotext
		if core.settings:get_bool("scaffold") then
			if core.settings:get("scaffold.mode") == "Silent" then
				core.update_infotext("scaffold", "World", "scaffold", "Silent")
			else
				core.update_infotext("scaffold", "World", "scaffold", "Blatant")
			end
		end
		--ScaffoldPlus infotext
		if core.settings:get_bool("scaffold_plus") then
			if core.settings:get("scaffold_plus.mode") == "Silent" then
				core.update_infotext("scaffoldplus", "World", "scaffold_plus", "Silent")
			else
				core.update_infotext("scaffoldplus", "World", "scaffold_plus", "Blatant")
			end
		end
    end
end)
