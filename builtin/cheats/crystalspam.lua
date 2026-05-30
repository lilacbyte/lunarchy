local CRYSTAL_ITEM_NAMES = {
	"mcl_end:crystal",
	"mcl_end_crystal:end_crystal",
}

local OBSIDIAN_ITEM_NAMES = {
	"mcl_core:obsidian",
	"default:obsidian",
}

local SUPPORT_BLOCK_NAMES = {
	["mcl_core:obsidian"] = true,
	["mcl_core:crying_obsidian"] = true,
	["mcl_core:bedrock"] = true,
	["default:obsidian"] = true,
}

local CRYSTAL_ENTITY_NAMES = {
	["mcl_end:crystal"] = true,
	["mcl_end_crystal:end_crystal"] = true,
}

core.register_cheat_with_infotext("crystalspam", "Combat", "crystalspam", "")

local spam_state = {
	cooldown = 0,
	obsidian_cooldown = 0,
	placing_node_token = 0,
	target = nil,
	support_pos = nil,
}

local function select_hotbar_item(names)
	local player = core.localplayer
	local inv = core.get_inventory("current_player")
	if not player or not inv or not inv.main then
		return false
	end

	for index, stack in ipairs(inv.main) do
		if index < 10 and not stack:is_empty() then
			for _, item_name in ipairs(names) do
				if stack:get_name() == item_name then
					player:set_wield_index(index)
					return true
				end
			end
		end
	end
	return false
end

local function is_support_block(name)
	return SUPPORT_BLOCK_NAMES[name] or false
end

local function is_buildable(node)
	if not node then
		return false
	end

	local def = core.get_node_def(node.name)
	return def and def.buildable_to
end

local function get_target_mode()
	local mode = core.settings:get("crystalspam.target_mode")
	if mode == "Entities" or mode == "Both" then
		return mode
	end
	return "Players"
end

local function get_infotext_mode()
	if core.settings:get_bool("crystalspam.safe") then
		return "SAFE"
	end
	return ""
end

local function stop_placing_node()
	spam_state.placing_node_token = spam_state.placing_node_token + 1
	core.settings:set_bool("placing_node", false)
end

local function clear_locked_support()
	spam_state.target = nil
	spam_state.support_pos = nil
end

local function get_target_key(target)
	if target and target.get_id then
		return target:get_id()
	end
	return target
end

local function pulse_placing_node()
	spam_state.placing_node_token = spam_state.placing_node_token + 1
	local token = spam_state.placing_node_token
	core.settings:set_bool("placing_node", true)
	core.after(0.1, function()
		if spam_state.placing_node_token == token then
			core.settings:set_bool("placing_node", false)
		end
	end)
end

local function get_nearest_target(max_distance)
	local player = core.localplayer
	if not player then
		return nil
	end

	local player_pos = player:get_pos()
	if not player_pos then
		return nil
	end

	local best_target = nil
	local best_distance = max_distance

	for _, obj in ipairs(core.get_nearby_objects(max_distance) or {}) do
		if obj and not (obj.is_local_player and obj:is_local_player()) then
			if is_valid_target(obj, get_target_mode(), max_distance, player_pos) then
				local obj_pos = obj:get_pos()
				if obj_pos then
					local distance = vector.distance(player_pos, obj_pos)
					if distance <= best_distance then
						best_distance = distance
						best_target = obj
					end
				end
			end
		end
	end

	return best_target
end

local function get_target_search_radius()
	if not core.settings:get_bool("reach") then
		return 8
	end
	local reach_bonus = tonumber(core.settings:get("reach.range")) or 2
	return 8 + reach_bonus
end

local function get_max_place_distance()
	if not core.localplayer then
		return 4.5
	end

	local reach_bonus = 0
	if core.settings:get_bool("reach") then
		reach_bonus = tonumber(core.settings:get("reach.range")) or 2
	end

	return 4.5 + reach_bonus
end

local function get_crystals_near(center, radius)
	local crystals = {}
	if not center then
		return crystals
	end

	for _, obj in ipairs(core.get_objects_inside_radius(center, radius)) do
		local name = obj.get_name and obj:get_name() or nil
		if name and CRYSTAL_ENTITY_NAMES[name] then
			crystals[#crystals + 1] = obj
		end
	end

	return crystals
end

local function get_target_support_candidates(target)
	local target_pos = target and target:get_pos() or nil
	if not target_pos then
		return {}
	end

	local base = vector.round(target_pos)
	local candidates = {}
	local offsets = {
		{x = 0, y = -1, z = 0},
		{x = 1, y = -1, z = 0},
		{x = -1, y = -1, z = 0},
		{x = 0, y = -1, z = 1},
		{x = 0, y = -1, z = -1},
		{x = 1, y = 0, z = 0},
		{x = -1, y = 0, z = 0},
		{x = 0, y = 0, z = 1},
		{x = 0, y = 0, z = -1},
	}

	for _, offset in ipairs(offsets) do
		candidates[#candidates + 1] = vector.add(base, offset)
	end

	return candidates
end

local function punch_crystals_near(target_pos, radius)
	local crystals = get_crystals_near(target_pos, radius or 1.25)
	if #crystals == 0 then
		return false
	end

	local punched = false
	for _, crystal in ipairs(crystals) do
		local id = nil
		if crystal.get_id then
			id = crystal:get_id()
		end
		if id and core.localplayer then
			core.localplayer:punch(id)
		else
			crystal:punch()
		end
		punched = true
	end

	return punched
end

local function punch_crystals_near_later(target_pos, radius)
	if not target_pos then
		return
	end

	local punch_pos = vector.new(target_pos)
	core.after(0.05, function()
		punch_crystals_near(punch_pos, radius or 2.25)
	end)
end

local function clear_dodge_controls()
	return
end

local find_nearby_support_block

find_nearby_support_block = function(center_pos, player_pos, radius)
	if not center_pos then
		return nil
	end

	local search_radius = radius or 1
	local best_pos = nil
	local best_score = math.huge

	for dx = -search_radius, search_radius do
		for dy = -1, 1 do
			for dz = -search_radius, search_radius do
				local node_pos = {
					x = center_pos.x + dx,
					y = center_pos.y + dy,
					z = center_pos.z + dz,
				}
				local node = core.get_node_or_nil(node_pos)
				if node and is_support_block(node.name) then
					local clearance_pos = vector.add(node_pos, {x = 0, y = 1, z = 0})
					local clearance_node = core.get_node_or_nil(clearance_pos)
					local in_range = not player_pos or
						vector.distance(player_pos, node_pos) <= get_max_place_distance()
					if in_range and clearance_node and is_buildable(clearance_node) then
						local score = math.abs(dx) + math.abs(dz) + math.abs(dy) * 2
						if score < best_score then
							best_score = score
							best_pos = node_pos
						end
					end
				end
			end
		end
	end

	return best_pos
end

local function has_nearby_support_block(center_pos, radius)
	return find_nearby_support_block(center_pos, nil, radius) ~= nil
end

local function try_place_spam(target)
	local player = core.localplayer
	local player_pos = player and player:get_pos() or nil
	local target_key = get_target_key(target)
	if spam_state.target ~= target_key then
		clear_locked_support()
		spam_state.target = target_key
	end

	local support_pos = spam_state.support_pos
	if support_pos and player_pos and vector.distance(player_pos, support_pos) > get_max_place_distance() then
		spam_state.support_pos = nil
		support_pos = nil
	end

	if support_pos then
		local support_node = core.get_node_or_nil(support_pos)
		if not support_node or not is_support_block(support_node.name) then
			local target_pos = target and target:get_pos() or nil
			local nearby_support = target_pos and
				find_nearby_support_block(vector.round(target_pos), player_pos, 2) or nil
			if nearby_support then
				support_pos = nearby_support
				spam_state.support_pos = support_pos
			end
		end
	end

	if not support_pos then
		local candidates = get_target_support_candidates(target)
		local fallback_pos = nil
		local target_pos = target and target:get_pos() or nil
		if target_pos then
			support_pos = find_nearby_support_block(vector.round(target_pos), player_pos, 2)
		end
		for _, candidate in ipairs(candidates) do
			if support_pos then
				break
			end
			local clearance_pos = vector.add(candidate, {x = 0, y = 1, z = 0})
			local support_node = core.get_node_or_nil(candidate)
			local clearance_node = core.get_node_or_nil(clearance_pos)
			local in_range = not player_pos or
				vector.distance(player_pos, candidate) <= get_max_place_distance()
			if in_range and clearance_node and is_buildable(clearance_node) then
				if support_node and is_support_block(support_node.name) then
					support_pos = candidate
					break
				end
				if not fallback_pos and support_node and is_buildable(support_node) then
					fallback_pos = candidate
				end
			end
		end
		support_pos = support_pos or fallback_pos
		spam_state.support_pos = support_pos
	end

	if not support_pos then
		return false
	end

	local clearance_pos = vector.add(support_pos, {x = 0, y = 1, z = 0})
	local support_node = core.get_node_or_nil(support_pos)
	local clearance_node = core.get_node_or_nil(clearance_pos)

	if not clearance_node or not is_buildable(clearance_node) then
		stop_placing_node()
		return false
	end

	punch_crystals_near(support_pos, 0.4)

	if support_node and not is_support_block(support_node.name) then
		if is_buildable(support_node) then
			if spam_state.obsidian_cooldown > 0 then
				return false
			end
			if select_hotbar_item(OBSIDIAN_ITEM_NAMES) then
				pulse_placing_node()
				core.place_node(support_pos)
				spam_state.obsidian_cooldown = 0.05
				support_node = core.get_node_or_nil(support_pos)
			else
				stop_placing_node()
				return false
			end
		else
			stop_placing_node()
			core.dig_node(support_pos)
			spam_state.cooldown = 0.25
			return false
		end
	end

	support_node = core.get_node_or_nil(support_pos)
	clearance_node = core.get_node_or_nil(clearance_pos)
	if not support_node or not is_support_block(support_node.name) or not clearance_node or not is_buildable(clearance_node) then
		stop_placing_node()
		return false
	end

	if select_hotbar_item(CRYSTAL_ITEM_NAMES) then
		pulse_placing_node()
		core.place_node(support_pos)
		punch_crystals_near(support_pos, 0.4)
		punch_crystals_near_later(support_pos, 0.4)
		return true
	end

	stop_placing_node()
	return false
end

local function get_safe_hp_threshold()
	return 10
end

core.register_globalstep(function(dtime)
	if not core.settings:get_bool("crystalspam") then
		core.update_infotext("crystalspam", "Combat", "crystalspam", "")
		spam_state.cooldown = 0
		spam_state.obsidian_cooldown = 0
		clear_locked_support()
		stop_placing_node()
		clear_dodge_controls()
		return
	end

	spam_state.cooldown = math.max(spam_state.cooldown - dtime, 0)
	spam_state.obsidian_cooldown = math.max(spam_state.obsidian_cooldown - dtime, 0)
	local player = core.localplayer
	if not player then
		core.update_infotext("crystalspam", "Combat", "crystalspam", "")
		clear_locked_support()
		stop_placing_node()
		return
	end

	local hp = player:get_hp() or 0
	local safe = core.settings:get_bool("crystalspam.safe")

	if safe and hp <= get_safe_hp_threshold() then
		core.update_infotext("crystalspam", "Combat", "crystalspam", get_infotext_mode())
		clear_locked_support()
		stop_placing_node()
		return
	end

	local target = get_nearest_target(get_target_search_radius())
	if not target then
		core.update_infotext("crystalspam", "Combat", "crystalspam", get_infotext_mode())
		clear_locked_support()
		stop_placing_node()
		return
	end

	core.update_infotext("crystalspam", "Combat", "crystalspam", get_infotext_mode())

	if spam_state.target == get_target_key(target) and spam_state.support_pos and
			punch_crystals_near(spam_state.support_pos, 0.4) then
		return
	end

	if spam_state.cooldown > 0 then
		return
	end

	try_place_spam(target)
end)
