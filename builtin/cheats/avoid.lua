local MODULE_KEY = "avoid_movement"
local RANGE_KEY = "avoid_movement.range"
local DEFAULT_RANGE = 6
local MIN_ESCAPE_DISTANCE = 10
local MAX_ESCAPE_DISTANCE = 15
local TELEPORT_COOLDOWN = 0.75

local cooldown = 0

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
		local candidate = {
			x = pos.x,
			y = pos.y + y_offset,
			z = pos.z,
		}
		if is_walkable(vector.add(candidate, {x = 0, y = -1, z = 0}))
				and is_clear(candidate)
				and is_clear(vector.add(candidate, {x = 0, y = 1, z = 0})) then
			return candidate
		end
	end
	return nil
end

local function nearest_player(origin, radius)
	local nearest
	local nearest_distance = radius

	for _, object in ipairs(core.get_nearby_objects(radius) or {}) do
		if object:is_player() and not object:is_local_player() then
			local pos = object:get_pos()
			if pos then
				local distance = vector.distance(origin, pos)
				if distance <= nearest_distance then
					nearest = object
					nearest_distance = distance
				end
			end
		end
	end

	return nearest, nearest_distance
end

local function escape_direction(player_pos, threat_pos)
	local dx = player_pos.x - threat_pos.x
	local dz = player_pos.z - threat_pos.z
	local length = math.sqrt(dx * dx + dz * dz)
	if length > 0.001 then
		return dx / length, dz / length
	end

	local angle = math.random() * math.pi * 2
	return math.cos(angle), math.sin(angle)
end

local function find_escape_position(player_pos, threat_pos)
	local dir_x, dir_z = escape_direction(player_pos, threat_pos)
	local base_angle = math.atan2(dir_z, dir_x)
	local distance = math.random(MIN_ESCAPE_DISTANCE, MAX_ESCAPE_DISTANCE)
	local angle_offsets = {0, 22.5, -22.5, 45, -45, 67.5, -67.5, 90, -90}
	local direct = {
		x = threat_pos.x + dir_x * distance,
		y = player_pos.y,
		z = threat_pos.z + dir_z * distance,
	}

	for _, offset in ipairs(angle_offsets) do
		local angle = base_angle + math.rad(offset)
		local candidate = safe_destination({
			x = threat_pos.x + math.cos(angle) * distance,
			y = player_pos.y,
			z = threat_pos.z + math.sin(angle) * distance,
		})
		if candidate then
			return candidate
		end
	end

	-- Match Mace's direct set_pos fallback if no loaded safe floor is available.
	return direct
end

core.register_globalstep(function(dtime)
	cooldown = math.max(0, cooldown - dtime)
	if not core.settings:get_bool(MODULE_KEY) then
		core.update_infotext("avoid", "Movement", MODULE_KEY, "")
		return
	end

	local player = core.localplayer
	local player_pos = player and player:get_pos() or nil
	if not player_pos then
		return
	end

	local range = tonumber(core.settings:get(RANGE_KEY)) or DEFAULT_RANGE
	range = math.max(1, math.min(20, range))
	local threat, threat_distance = nearest_player(player_pos, range)
	if not threat then
		core.update_infotext("avoid", "Movement", MODULE_KEY, "")
		return
	end

	local name = threat:get_name() or "player"
	core.update_infotext("avoid", "Movement", MODULE_KEY,
		("%s (%.1fm)"):format(name, threat_distance))
	if cooldown > 0 then
		return
	end

	local threat_pos = threat:get_pos()
	local destination = threat_pos and find_escape_position(player_pos, threat_pos) or nil
	if not destination then
		core.update_infotext("avoid", "Movement", MODULE_KEY, "No safe escape")
		cooldown = TELEPORT_COOLDOWN
		return
	end

	-- Match Killaura Mace's client movement path, but move away from the target.
	player:set_pos(destination)
	player:set_velocity({x = 0, y = 0, z = 0})
	cooldown = TELEPORT_COOLDOWN
end)

core.register_cheat_with_infotext("avoid", "Movement", MODULE_KEY, "")
core.register_cheat_description("avoid", "Movement", MODULE_KEY,
	"Teleports 10-15 blocks away when another player enters the configured range.")
core.register_cheat_setting("Range", "Movement", MODULE_KEY, RANGE_KEY,
	{type = "slider_int", min = 1, max = 20, steps = 20})
