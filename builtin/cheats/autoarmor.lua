local armor_order = {
	{ index = 2, group = "armor_head" },
	{ index = 3, group = "armor_torso" },
	{ index = 4, group = "armor_legs" },
	{ index = 5, group = "armor_feet" },
}

local autoarmor_timer = 0
local autoarmor_busy = false

local function normalize_itemname(name)
	return name
end

local function armor_score(stack)
	if not stack or stack:is_empty() then
		return -1
	end

	local itemname = normalize_itemname(stack:get_name())
	local def = core.get_item_def(itemname)
	local groups = def and def.groups or nil
	if not groups or (groups.armor or 0) == 0 then
		return -1
	end

	local points = tonumber(groups.mcl_armor_points) or 0
	local toughness = tonumber(groups.mcl_armor_toughness) or 0
	local max_uses = math.max(1, (tonumber(groups.mcl_armor_uses) or 1) - 1)
	local remaining_uses = math.floor(max_uses * (65535 - stack:get_wear()) / 65535)

	-- Prefer protection first, then toughness, then remaining durability.
	return points * 100000000 + toughness * 1000000 + remaining_uses
end

local function find_best_piece(inv, slot)
	local current = inv.armor and inv.armor[slot.index] or ItemStack("")
	if slot.group == "armor_torso" and not current:is_empty() then
		local current_def = core.get_item_def(normalize_itemname(current:get_name()))
		local current_groups = current_def and current_def.groups or nil
		if current_groups and (current_groups.elytra or 0) > 0 then
			return nil
		end
	end

	local best_score = armor_score(current)
	local best_index

	for i, stack in ipairs(inv.main or {}) do
		if not stack:is_empty() then
			local itemname = normalize_itemname(stack:get_name())
			local def = core.get_item_def(itemname)
			local groups = def and def.groups or nil
			if groups and (groups[slot.group] or 0) > 0 then
				local score = armor_score(stack)
				if score > best_score then
					best_score = score
					best_index = i
				end
			end
		end
	end

	return best_index
end

local function equip_best_armor()
	local player = core.localplayer
	if not player then
		return false
	end

	local inv = core.get_inventory("current_player")
	if not inv or not inv.main or not inv.armor then
		return false
	end

	for _, slot in ipairs(armor_order) do
		local best_index = find_best_piece(inv, slot)
		if best_index then
			local action = InventoryAction("move")
			action:from("current_player", "main", best_index)
			action:to("current_player", "armor", slot.index)
			action:set_count(1)
			action:apply()
			return true
		end
	end

	return false
end

core.register_globalstep(function(dtime)
	if not core.settings:get_bool("autoarmor") then
		return
	end

	if autoarmor_busy then
		return
	end

	autoarmor_timer = autoarmor_timer + dtime
	if autoarmor_timer < 0.2 then
		return
	end
	autoarmor_timer = 0

	if not core.localplayer then
		return
	end

	autoarmor_busy = true
	local ok, err = pcall(equip_best_armor)
	autoarmor_busy = false

	if not ok then
		core.log("error", "[autoarmor] failed: " .. tostring(err))
	end
end)

core.register_cheat_with_infotext("autoarmor", "Combat", "autoarmor", "")
core.register_cheat_description("autoarmor", "Combat", "autoarmor", "Automatically equips the best armor in your inventory.")
