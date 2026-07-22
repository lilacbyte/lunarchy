-- Credits to Lizzy Fleckenstein

local drop_action = InventoryAction("drop")
local betterdrop_known_items = {}
local betterdrop_initialized = false
local betterdrop_warmup = 0

local strip_move_act = InventoryAction("move")
strip_move_act:to("current_player", "craft", 1)
local strip_craft_act = InventoryAction("craft")
strip_craft_act:craft("current_player")
local strip_move_back_act = InventoryAction("move")
strip_move_back_act:from("current_player", "craftresult", 1)

core.register_globalstep(function(dtime)
	local player = core.localplayer
	if not player then
		betterdrop_initialized = false
		betterdrop_known_items = {}
		betterdrop_warmup = 0
		return
	end
	local item = player:get_wielded_item()
	local itemdef = core.get_item_def(item:get_name())
	local wieldindex = player:get_wield_index()
	-- AutoRefill
	if (core.settings:get_bool("autorefill") or core.settings:get_bool("scaffold") or core.settings:get_bool("scaffold_plus")) and itemdef then
		local space = item:get_free_space()
		local i = core.find_additional_items(item:get_name(), wieldindex+1)
		local total_items = core.get_total_items(item:get_name())
		if i and space > 0 then
			local move_act = InventoryAction("move")
			move_act:to("current_player", "main", wieldindex + 1)
			move_act:from("current_player", "main", i)
			move_act:set_count(space)
			move_act:apply()
		end
	end
	-- AutoPlanks (Strip in DF)
	if core.settings:get_bool("autoplanks") then
		if itemdef and itemdef.groups.tree and player:get_control().place then
			strip_move_act:from("current_player", "main", wieldindex)
			strip_move_back_act:to("current_player", "main", wieldindex)
			strip_move_act:apply()
			strip_craft_act:apply()
			strip_move_back_act:apply()
		end
	end
	-- AutoEject
	if core.settings:get_bool("autoeject") then
		local list = (core.settings:get("eject_items") or ""):split(",")
		local inventory = core.get_inventory("current_player")
		for index, stack in pairs(inventory.main) do
			if table.indexof(list, stack:get_name()) ~= -1 then
				drop_action:from("current_player", "main", index)
				drop_action:apply()
			end
		end
	end
	-- BetterDrop
	if core.settings:get_bool("betterdrop") then
		local inventory = core.get_inventory("current_player")
		if inventory and inventory.main then
			local whitelist = parse_item_filter(core.settings:get("betterdrop_whitelist") or "")
			local blacklist = parse_item_filter(core.settings:get("betterdrop_blacklist") or "")
			local restrict_incoming = next(whitelist) ~= nil or core.settings:get_bool("betterdrop_no_incoming")
			if not betterdrop_initialized then
				betterdrop_known_items = {}
				betterdrop_initialized = true
				betterdrop_warmup = 3.0
			end
			if betterdrop_warmup > 0 then
				merge_inventory_names(betterdrop_known_items, inventory)
				betterdrop_warmup = betterdrop_warmup - dtime
				return
			end
			merge_inventory_names(betterdrop_known_items, inventory, {"armor", "offhand", "hand"})

			for index, stack in pairs(inventory.main) do
				local name = stack:get_name()
				if name ~= "" then
					local incoming_disallowed = restrict_incoming and not betterdrop_known_items[name] and not whitelist[name]
					local should_drop = blacklist[name] or incoming_disallowed
					if should_drop then
						betterdrop_drop_slot(index, stack:get_count())
					else
						betterdrop_known_items[name] = true
					end
				end
			end
		end
	else
		betterdrop_initialized = false
		betterdrop_known_items = {}
		betterdrop_warmup = 0
	end
end)

core.register_list_command("eject", "Configure AutoEject", "eject_items")

local function trim_item_name(name)
	return (name or ""):match("^%s*(.-)%s*$")
end

function parse_item_filter(value)
	local out = {}
	for _, item in ipairs((value or ""):split(",")) do
		local name = trim_item_name(item)
		if name ~= "" then
			out[name] = true
		end
	end
	return out
end

function merge_inventory_list_names(names, list)
	for _, stack in pairs(list or {}) do
		local name = stack:get_name()
		if name ~= "" then
			names[name] = true
		end
	end
end

function merge_inventory_names(names, inventory, lists)
	for _, listname in ipairs(lists or {"main", "armor", "offhand", "hand"}) do
		merge_inventory_list_names(names, inventory and inventory[listname])
	end
end

function collect_inventory_names(inventory)
	local names = {}
	merge_inventory_names(names, inventory)
	return names
end

function betterdrop_drop_slot(slot, count)
	local action = InventoryAction("drop")
	if not action then
		return
	end
	action:from("current_player", "main", slot)
	action:set_count(count or 0)
	if action.set_distance then
		action:set_distance(core.settings:get("betterdrop_distance") or 0)
	end
	action:apply()
end

-- Enderchest

function get_itemslot_bg(x, y, w, h)
	local out = ""
	for i = 0, w - 1, 1 do
		for j = 0, h - 1, 1 do
			out = out .."image["..x+i..","..y+j..";1,1;mcl_formspec_itemslot.png]"
		end
	end
	return out
end

local function close_cheat_menu()
	if core.close_cheat_menu then
		core.close_cheat_menu()
	end
end

local enderchest_formspec = "size[9,8.75]"..
	"label[0,0;"..core.formspec_escape(core.colorize("#313131", "Ender Chest")).."]"..
	"list[current_player;enderchest;0,0.5;9,3;]"..
	get_itemslot_bg(0,0.5,9,3)..
	"label[0,4.0;"..core.formspec_escape(core.colorize("#313131", "Inventory")).."]"..
	"list[current_player;main;0,4.5;9,3;9]"..
	get_itemslot_bg(0,4.5,9,3)..
	"list[current_player;main;0,7.74;9,1;]"..
	get_itemslot_bg(0,7.74,9,1)..
	"listring[current_player;enderchest]"..
	"listring[current_player;main]"

function core.open_enderchest()
	if not core.require_mineclonia("enderchest") then
		return
	end
	close_cheat_menu()
	core.show_formspec("inventory:enderchest", enderchest_formspec)
end

-- HandSlot

local hand_formspec = "size[9,8.75]"..
	"label[0,0;"..core.formspec_escape(core.colorize("#313131", "Hand")).."]"..
	"list[current_player;hand;0,0.5;1,1;]"..
	get_itemslot_bg(0,0.5,1,1)..
	"label[0,4.0;"..core.formspec_escape(core.colorize("#313131", "Inventory")).."]"..
	"list[current_player;main;0,4.5;9,3;9]"..
	get_itemslot_bg(0,4.5,9,3)..
	"list[current_player;main;0,7.74;9,1;]"..
	get_itemslot_bg(0,7.74,9,1)..
	"listring[current_player;hand]"..
	"listring[current_player;main]"
	
function core.open_handslot()
	if not core.require_mineclonia("hand") then
		return
	end
	close_cheat_menu()
	core.show_formspec("inventory:hand", hand_formspec)
end

local portable_enchanting_formspec = "formspec_version[4]" ..
	"size[11.75,10.425]" ..
	"label[0.375,0.375;" .. core.formspec_escape(core.colorize("#313131", "Enchant")) .. "]" ..
	get_itemslot_bg(1,3.25,1,1) ..
	"list[current_player;enchanting_item;1,3.25;1,1]" ..
	get_itemslot_bg(2.25,3.25,1,1) ..
	"image[2.25,3.25;1,1;mcl_enchanting_lapis_background.png]" ..
	"list[current_player;enchanting_lapis;2.25,3.25;1,1]" ..
	"image[4.125,0.56;7.25,4.1;mcl_enchanting_button_background.png]" ..
	"button[4.125,0.65;7.25,1.3;button_1;]" ..
	"button[4.125,1.95;7.25,1.3;button_2;]" ..
	"button[4.125,3.25;7.25,1.3;button_3;]" ..
	"label[0.375,4.7;" .. core.formspec_escape(core.colorize("#313131", "Inventory")) .. "]" ..
	get_itemslot_bg(0.375,5.1,9,3) ..
	"list[current_player;main;0.375,5.1;9,3;9]" ..
	get_itemslot_bg(0.375,9.05,9,1) ..
	"list[current_player;main;0.375,9.05;9,1;]" ..
	"listring[current_player;enchanting_item]" ..
	"listring[current_player;main]" ..
	"listring[current_player;enchanting_lapis]" ..
	"listring[current_player;main]"

function core.open_enchanting_table()
	if not core.require_mineclonia("enchanting table") then
		return
	end
	close_cheat_menu()
	core.show_formspec("mcl_enchanting:table", portable_enchanting_formspec)
end

core.register_on_formspec_input(function(formname, fields)
	if formname == "mcl_enchanting:table" then
		if fields.button_1 or fields.button_2 or fields.button_3 then
			core.send_inventory_fields("mcl_enchanting:table", fields)
		end
	end
end)

core.register_cheat("autoeject", "Misc", "autoeject")
core.register_cheat("hand", "Misc", core.open_handslot)
core.register_cheat("enderchest", "Misc", core.open_enderchest)
core.register_cheat("enchanting table", "Misc", core.open_enchanting_table)
core.register_cheat("autoplanks", "Misc", "autoplanks")
core.register_cheat("autorefill", "Misc", "autorefill")
core.register_cheat("betterdrop", "Misc", "betterdrop")
core.register_cheat_setting("Distance", "Misc", "betterdrop", "betterdrop_distance", {type="slider_float", min=0.0, max=12.0, steps=120})
core.register_cheat_setting("Whitelist", "Misc", "betterdrop", "betterdrop_whitelist", {type="text", size=18})
core.register_cheat_setting("Antidrop", "Misc", "betterdrop", "betterdrop_antidrop", {type="text", size=18})
core.register_cheat_setting("No Incoming", "Misc", "betterdrop", "betterdrop_no_incoming", {type="bool"})
core.register_cheat_setting("Blacklist", "Misc", "betterdrop", "betterdrop_blacklist", {type="text", size=18})
