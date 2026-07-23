local WITHER_SKULL = "mcl_heads:wither_skeleton"
local NAMETAG_ITEM = "mcl_mobitems:nametag"
local TELEPORT_MIN_DISTANCE = 10
local TELEPORT_MAX_DISTANCE = 15

local WITHER_LAYOUTS = {
	x = {
		soul = {
			{x = 0, y = 0, z = 0},
			{x = -1, y = 1, z = 0},
			{x = 0, y = 1, z = 0},
			{x = 1, y = 1, z = 0},
		},
		skull = {
			{x = -1, y = 2, z = 0},
			{x = 0, y = 2, z = 0},
			{x = 1, y = 2, z = 0},
		},
	},
	z = {
		soul = {
			{x = 0, y = 0, z = 0},
			{x = 0, y = 1, z = -1},
			{x = 0, y = 1, z = 0},
			{x = 0, y = 1, z = 1},
		},
		skull = {
			{x = 0, y = 2, z = -1},
			{x = 0, y = 2, z = 0},
			{x = 0, y = 2, z = 1},
		},
	},
}

local autowither_state = {
	cooldown = 0,
	queue = nil,
	disable_timer = 0,
	tag_timer = 0,
	tag_attempts = 0,
	axis = nil,
	base_pos = nil,
	spawn_hint = nil,
}

local function is_soul_block_name(name)
	local def = core.get_item_def(name)
	local groups = def and def.groups or nil
	return groups and (groups.soul_block or 0) > 0 or false
end

local function is_soul_block_stack(stack)
	if not stack or stack:is_empty() then
		return false
	end
	return is_soul_block_name(stack:get_name())
end

local function is_buildable(node)
	if not node then
		return false
	end
	local def = core.get_node_def(node.name)
	return def and def.buildable_to
end

local function get_player_eye_pos(player)
	local props = player and player.get_properties and player:get_properties() or nil
	local eye_height = props and props.eye_height or 1.625
	return vector.offset(player:get_pos(), 0, eye_height, 0)
end

local function is_walkable(pos)
	local node = core.get_node_or_nil(vector.round(pos))
	local def = node and core.get_node_def(node.name) or nil
	return def and def.walkable or false
end

local function is_clear(pos)
	local node = core.get_node_or_nil(vector.round(pos))
	local def = node and core.get_node_def(node.name) or nil
	return def and not def.walkable or false
end

local function safe_destination(pos)
	for y_offset = 2, -2, -1 do
		local candidate = vector.offset(pos, 0, y_offset, 0)
		if is_walkable(vector.offset(candidate, 0, -1, 0))
				and is_clear(candidate)
				and is_clear(vector.offset(candidate, 0, 1, 0)) then
			return candidate
		end
	end
	return nil
end

local function teleport_to_next_wither()
	local player = core.localplayer
	local player_pos = player and player:get_pos() or nil
	if not player_pos then
		return false
	end

	local look = player:get_look_dir() or {x = 1, y = 0, z = 0}
	local horizontal_length = math.sqrt(look.x * look.x + look.z * look.z)
	local dir_x, dir_z
	if horizontal_length > 0.001 then
		dir_x = look.x / horizontal_length
		dir_z = look.z / horizontal_length
	else
		local angle = math.random() * math.pi * 2
		dir_x = math.cos(angle)
		dir_z = math.sin(angle)
	end

	local distance = math.random(TELEPORT_MIN_DISTANCE, TELEPORT_MAX_DISTANCE)
	local direct = {
		x = player_pos.x + dir_x * distance,
		y = player_pos.y,
		z = player_pos.z + dir_z * distance,
	}

	-- Flying keeps the current altitude. On foot, prefer Avoid's safe-floor search.
	local destination = direct
	local airborne = player.is_touching_ground and not player:is_touching_ground()
	if not core.settings:get_bool("free_move") and not airborne then
		destination = safe_destination(direct) or direct
	end

	player:set_pos(destination)
	player:set_velocity({x = 0, y = 0, z = 0})
	return true
end

local function choose_axis(player)
	local look = player and player.get_look_dir and player:get_look_dir() or nil
	if not look then
		return "x"
	end

	if math.abs(look.x) >= math.abs(look.z) then
		return "x"
	end
	return "z"
end

local function get_target_base(player)
	local pointed = player and player.get_pointed_thing and player:get_pointed_thing() or nil
	if pointed and pointed.type == "node" then
		if pointed.under then
			local under_node = core.get_node_or_nil(pointed.under)
			if under_node and is_soul_block_name(under_node.name) then
				return vector.copy(pointed.under)
			end
		end

		if pointed.above then
			local above_node = core.get_node_or_nil(pointed.above)
			if above_node and is_soul_block_name(above_node.name) then
				return vector.copy(pointed.above)
			end
		end
	end

	local player = core.localplayer
	if not player then
		return nil
	end

	return vector.round(player:get_pos())
end

local function queue_layout(base_pos, axis)
	local layout = WITHER_LAYOUTS[axis]
	if not layout then
		return nil
	end

	local queue = {}

	for _, offset in ipairs(layout.soul) do
		local pos = vector.add(base_pos, offset)
		local node = core.get_node_or_nil(pos)
		if not node then
			return nil
		end
		if not is_soul_block_name(node.name) then
			if not is_buildable(node) then
				return nil
			end
			queue[#queue + 1] = { kind = "soul", pos = pos }
		end
	end

	for _, offset in ipairs(layout.skull) do
		local pos = vector.add(base_pos, offset)
		local support_pos = vector.offset(pos, 0, -1, 0)
		local node = core.get_node_or_nil(pos)
		local support = core.get_node_or_nil(support_pos)
		if not node or not support then
			return nil
		end
		if node.name ~= WITHER_SKULL then
			if not is_buildable(node) then
				return nil
			end
			if not is_soul_block_name(support.name) and not is_buildable(support) then
				return nil
			end
			queue[#queue + 1] = { kind = "skull", pos = pos, support_pos = support_pos }
		end
	end

	return queue
end

local function switch_to_group_item()
	local inv = core.get_inventory("current_player")
	if not inv or not inv.main then
		return false
	end

	for index, stack in ipairs(inv.main) do
		if is_soul_block_stack(stack) then
			core.localplayer:set_wield_index(index)
			return true
		end
	end

	return false
end

local function switch_to_wither_skull()
	return core.switch_to_item(WITHER_SKULL)
end

local function switch_to_nametag()
	return core.switch_to_item(NAMETAG_ITEM)
end

local function place_soul_at(pos)
	ws.aim(pos)
	local pointed_thing = {
		type = "node",
		under = pos,
		above = pos,
	}
	core.interact("place", pointed_thing)
end

local function place_skull_at(pos, support_pos)
	ws.aim(support_pos)
	local pointed_thing = {
		type = "node",
		under = support_pos,
		above = pos,
	}
	core.interact("place", pointed_thing)
end

local function find_spawned_wither(center)
	if not center then
		return nil
	end

	local best
	local best_distance = 8
	for _, obj in ipairs(core.get_objects_inside_radius(center, 8)) do
		if obj and obj.get_name and obj:get_name() == "mobs_mc:wither" then
			local pos = obj.get_pos and obj:get_pos() or nil
			if pos then
				local distance = vector.distance(center, pos)
				if distance <= best_distance then
					best_distance = distance
					best = obj
				end
			end
		end
	end

	return best
end

local function try_nametag_wither()
	local player = core.localplayer
	if not player then
		return false
	end

	local wither = find_spawned_wither(autowither_state.spawn_hint or autowither_state.base_pos or player:get_pos())
	if not wither then
		return false
	end

	if not switch_to_nametag() then
		return false
	end

	local wielded = player:get_wielded_item()
	local wield_name = wielded and wielded:get_name() or ""
	if wield_name ~= NAMETAG_ITEM then
		return false
	end

	local meta = wielded:get_meta()
	local current_name = meta:get_string("name")
	if current_name == "" then
		return false
	end

	-- Use the already-held named tag as-is; Mineclonia consumes it on right-click.
	local success = pcall(function()
		wither:rightclick()
	end)
	return success
end

local function start_session()
	local player = core.localplayer
	if not player then
		return false
	end

	local base_pos = get_target_base(player)
	if not base_pos then
		return false
	end

	local axis = choose_axis(player)
	local queue = queue_layout(base_pos, axis)
	if not queue or #queue == 0 then
		return false
	end

	autowither_state.queue = queue
	autowither_state.axis = axis
	autowither_state.base_pos = base_pos
	autowither_state.spawn_hint = nil
	return false
end

local function step_session(dtime)
	if autowither_state.disable_timer > 0 then
		autowither_state.disable_timer = math.max(autowither_state.disable_timer - dtime, 0)
		if autowither_state.disable_timer == 0 then
			local keep_spamming = core.settings:get_bool("autowither.teleport")
			if keep_spamming then
				teleport_to_next_wither()
			else
				core.settings:set_bool("autowither", false)
			end
			core.settings:set_bool("placing_node", false)
			autowither_state.queue = nil
			autowither_state.tag_timer = 0
			autowither_state.tag_attempts = 0
			autowither_state.axis = nil
			autowither_state.base_pos = nil
			autowither_state.spawn_hint = nil
			if keep_spamming then
				autowither_state.cooldown = 0.35
			end
		end
		return
	end

	if autowither_state.tag_timer > 0 then
		autowither_state.tag_timer = math.max(autowither_state.tag_timer - dtime, 0)
		if autowither_state.tag_timer == 0 then
			if try_nametag_wither() then
				autowither_state.disable_timer = 0.15
				autowither_state.tag_attempts = 0
			elseif autowither_state.tag_attempts < 4 then
				autowither_state.tag_attempts = autowither_state.tag_attempts + 1
				autowither_state.tag_timer = 0.25
			else
				autowither_state.disable_timer = 0.1
				autowither_state.tag_attempts = 0
			end
		end
		return
	end

	if not autowither_state.queue or #autowither_state.queue == 0 then
		autowither_state.queue = nil
		return
	end

	local entry = table.remove(autowither_state.queue, 1)
	if entry.kind == "soul" then
		if not switch_to_group_item() then
			core.settings:set_bool("autowither", false)
			autowither_state.queue = nil
			return
		end
		place_soul_at(entry.pos)
		return
	end

	if entry.kind == "skull" then
		if not switch_to_wither_skull() then
			core.settings:set_bool("autowither", false)
			autowither_state.queue = nil
			return
		end
		place_skull_at(entry.pos, entry.support_pos)
		if not autowither_state.queue or #autowither_state.queue == 0 then
			autowither_state.spawn_hint = vector.copy(entry.pos)
			if core.settings:get_bool("autowither.nametag") then
				autowither_state.tag_attempts = 0
				autowither_state.tag_timer = 0.45
			else
				autowither_state.disable_timer = 0.15
			end
		end
	end
end

core.register_globalstep(function(dtime)
	if not core.settings:get_bool("autowither") then
		autowither_state.cooldown = 0
		autowither_state.queue = nil
		autowither_state.tag_timer = 0
		autowither_state.tag_attempts = 0
		autowither_state.axis = nil
		autowither_state.base_pos = nil
		autowither_state.spawn_hint = nil
		autowither_state.disable_timer = 0
		core.settings:set_bool("placing_node", false)
		return
	end

	autowither_state.cooldown = math.max(autowither_state.cooldown - dtime, 0)
	if autowither_state.cooldown > 0 then
		return
	end

	if not autowither_state.queue then
		if not start_session() then
			return
		end
	end

	step_session(dtime)
	if autowither_state.queue then
		autowither_state.cooldown = 0.2
	end
end)

core.register_cheat_with_infotext("autowither", "Combat", "autowither", "-")
core.register_cheat_description("autowither", "Combat", "autowither", "Automatically completes a wither structure and places the skulls.")
