local CRYSTAL_ENTITY_NAMES = {
	["mcl_end:crystal"] = true,
	["mcl_end_crystal:end_crystal"] = true,
}

local avoid_cooldown = 0

local function safe_object_call(object, method)
	if not object then
		return nil
	end
	local ok, value = pcall(function()
		local fn = object[method]
		return fn and fn(object) or nil
	end)
	return ok and value or nil
end

local function is_clear(pos)
	local node = core.get_node_or_nil(vector.round(pos))
	local def = node and core.get_node_def(node.name) or nil
	return def and not def.walkable or false
end

local function find_escape_pos(crystal_pos, player_pos, direction, distance)
	for extra = 0, 4 do
		local candidate = {
			x = crystal_pos.x + direction * (distance + extra),
			y = player_pos.y,
			z = player_pos.z,
		}
		if is_clear(candidate) and is_clear(vector.offset(candidate, 0, 1, 0)) then
			return candidate
		end
	end

	return {
		x = crystal_pos.x + direction * distance,
		y = player_pos.y,
		z = player_pos.z,
	}
end

local function escape_from_crystal(player, player_pos, crystal_pos)
	local escape_distance = tonumber(core.settings:get("crystalavoid.escape_distance")) or 9
	escape_distance = math.max(7, math.min(16, escape_distance))
	local direction = player_pos.x < crystal_pos.x and -1 or 1
	local destination = find_escape_pos(crystal_pos, player_pos, direction, escape_distance)
	local velocity = {x = 0, y = 0, z = 0}

	if core.send_player_pos_packet then
		core.send_player_pos_packet(destination, velocity)
	else
		player:set_pos(destination)
		player:set_velocity(velocity)
	end

	avoid_cooldown = 0.25
end

core.register_cheat_with_infotext("crystalavoid", "Combat", "crystalavoid", "")

core.register_globalstep(function(dtime)
	if not core.settings:get_bool("crystalavoid") then
		avoid_cooldown = 0
		core.update_infotext("crystalavoid", "Combat", "crystalavoid", "")
		return
	end

	avoid_cooldown = math.max(avoid_cooldown - dtime, 0)
	if avoid_cooldown > 0 then
		return
	end

	local player = core.localplayer
	local player_pos = player and player:get_pos() or nil
	if not player_pos then
		return
	end

	local trigger_radius = tonumber(core.settings:get("crystalavoid.trigger_radius")) or 6
	trigger_radius = math.max(2, math.min(12, trigger_radius))
	local nearest_pos
	local nearest_distance = trigger_radius

	for _, object in ipairs(core.get_nearby_objects(trigger_radius) or {}) do
		local name = safe_object_call(object, "get_name")
		if CRYSTAL_ENTITY_NAMES[name] then
			local pos = safe_object_call(object, "get_pos")
			if pos then
				local distance = vector.distance(player_pos, pos)
				if distance <= nearest_distance then
					nearest_distance = distance
					nearest_pos = pos
				end
			end
		end
	end

	if nearest_pos then
		escape_from_crystal(player, player_pos, nearest_pos)
		core.update_infotext("crystalavoid", "Combat", "crystalavoid",
			("%.1f"):format(nearest_distance))
	else
		core.update_infotext("crystalavoid", "Combat", "crystalavoid", "")
	end
end)
