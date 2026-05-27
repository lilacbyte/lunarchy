local DOUBLE_JUMP_WINDOW_US = 100000
local action_busy = false

local jump_state = {
	pressed = false,
	count = 0,
	last_jump = 0,
}

local function item_group(name, group)
	local def = core.get_item_def(name)
	local groups = def and def.groups or nil
	return (groups and tonumber(groups[group])) or 0
end

local function is_firework_rocket(itemname)
	return itemname == "mcl_bows:rocket"
		or itemname:find("^mcl_fireworks:rocket_") ~= nil
end

local function has_elytra_equipped(inv)
	local chest = inv.armor and inv.armor[3]
	return chest and item_group(chest:get_name(), "elytra") > 0
end

local function find_elytra_index(inv)
	for index, stack in ipairs(inv.main or {}) do
		if item_group(stack:get_name(), "elytra") > 0 then
			return index
		end
	end
	return nil
end

local function equip_elytra(inv)
	if has_elytra_equipped(inv) then
		return true
	end

	local elytra_index = find_elytra_index(inv)
	if not elytra_index then
		return false
	end

	local action = InventoryAction("move")
	action:from("current_player", "main", elytra_index)
	action:to("current_player", "armor", 3)
	action:set_count(1)
	action:apply()
	return true
end

local function trigger_takeoff(player, inv)
	if action_busy then
		return
	end

	action_busy = true
	local ok, err = pcall(function()
		if equip_elytra(inv) then
			core.after(0.05, function()
				if not core.settings:get_bool("elytra_takeoff") then
					return
				end
				local current = core.localplayer
				if not current then
					return
				end
				local current_wield = current:get_wielded_item()
				local current_name = current_wield and current_wield:get_name() or ""
				if not is_firework_rocket(current_name) then
					return
				end
				core.interact("activate", {type = "nothing"})
			end)
		end
	end)
	action_busy = false

	if not ok then
		core.log("error", "[ElytraTakeoff] failed: " .. tostring(err))
	end
end

core.register_globalstep(function(dtime)
	if not core.settings:get_bool("elytra_takeoff") then
		jump_state.pressed = false
		jump_state.count = 0
		jump_state.last_jump = 0
		return
	end

	local player = core.localplayer
	if not player then
		return
	end

	local inv = core.get_inventory("current_player")
	if not inv or not inv.main or not inv.armor then
		jump_state.pressed = false
		jump_state.count = 0
		jump_state.last_jump = 0
		return
	end

	local wielded = player:get_wielded_item()
	local wield_name = wielded and wielded:get_name() or ""
	if not is_firework_rocket(wield_name) then
		jump_state.pressed = false
		jump_state.count = 0
		jump_state.last_jump = 0
		return
	end

	local velocity = player:get_velocity()
	if not velocity then
		return
	end

	if velocity.y == 0 then
		jump_state.pressed = false
		jump_state.count = 0
		jump_state.last_jump = 0
		return
	end

	local control = player:get_control()
	local jumping = control.jump or false
	local now = core.get_us_time()

	if jumping and not jump_state.pressed
		and (jump_state.last_jump == 0 or now - jump_state.last_jump > DOUBLE_JUMP_WINDOW_US) then
		jump_state.count = jump_state.count + 1
		jump_state.last_jump = now
	end

	if jump_state.count >= 2 then
		trigger_takeoff(player, inv)
		jump_state.count = 0
		jump_state.last_jump = 0
	end

	jump_state.pressed = jumping
end)

core.register_cheat("elytratakeoff", "Movement", "elytra_takeoff")
core.register_cheat_description("elytratakeoff", "Movement", "elytra_takeoff",
	"Double-jump while holding a firework rocket to equip an elytra and launch.")
