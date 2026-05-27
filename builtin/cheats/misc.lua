-- Do this first so you can update infontext later
minetest.register_cheat_with_infotext("flagaura", "Misc", "flagaura", "")
minetest.register_cheat_setting("Radius", "Misc", "flagaura", "flagaura.range", {type="slider_float", min=1, max=6, steps=6})
core.register_cheat_description("flagaura", "Misc", "flagaura", "Automatically picks up flags in CTF.")

local heal_cooldown = 0

local function destroy_apple(pos)
    minetest.dig_node(pos)
end

local greeter_initialized = false
local greeter_last_online_players = {}
local greeter_timer = 0
local logoutspots_initialized = false
local logoutspots_last_online_players = {}
local logoutspots_seen_positions = {}
local logoutspots_marker_ids = {}
local logoutspots_markers_by_name = {}
local logoutspots_marker_names = {}
local logoutspots_server_key = nil
local deathmarker_id = nil
local deathmarker_last_alive_pos = nil
local deathmarker_was_dead = false
local deathmarker_owner_key = nil
local spammer_plus_timer = 0
local spammer_plus_index = 1
local spammer_plus_file_warned = false
local avoid_timer = 0
local avoid_server_key = nil
local avoid_editor_value = nil
local avoid_disconnect_pending = false

local function remove_waypoint(id)
    if id and core.localplayer then
        core.localplayer:hud_remove(id)
    end
end

local function clear_waypoint_ids(ids)
    if not ids then
        return
    end

    for _, id in ipairs(ids) do
        remove_waypoint(id)
    end

    for i = #ids, 1, -1 do
        ids[i] = nil
    end
end

local function clear_logoutspot_marker(name)
    local id = logoutspots_markers_by_name[name]
    if id then
        remove_waypoint(id)
        logoutspots_markers_by_name[name] = nil

        for i = #logoutspots_marker_ids, 1, -1 do
            if logoutspots_marker_ids[i] == id then
                table.remove(logoutspots_marker_ids, i)
            end
        end
        for i = #logoutspots_marker_names, 1, -1 do
            if logoutspots_marker_names[i] == name then
                table.remove(logoutspots_marker_names, i)
            end
        end
    end
end

local function clear_logoutspot_markers()
    clear_waypoint_ids(logoutspots_marker_ids)
    logoutspots_markers_by_name = {}
    logoutspots_marker_names = {}
end

local function trim_logoutspot_markers()
    local limit = tonumber(core.settings:get("logoutspots.limit")) or 20
    limit = math.max(1, math.min(100, math.floor(limit)))
    while #logoutspots_marker_names > limit do
        clear_logoutspot_marker(logoutspots_marker_names[1])
    end
end

local function marker_server_key()
    local info = core.get_server_info and core.get_server_info() or nil
    if not info then
        return nil
    end

    local address = info.address or info.ip or ""
    local port = tostring(info.port or "")
    if address == "" and port == "" then
        return nil
    end
    return address .. ":" .. port
end

local function marker_pos_from(pos)
    if not pos then
        return nil
    end

    local rounded = vector.round(pos)
    return {
        x = rounded.x,
        y = rounded.y,
        z = rounded.z,
    }
end

local function logoutspot_scale()
    local scale = tonumber(core.settings:get("logoutspots.scale")) or 1.0
    return math.max(0.1, math.min(2.0, scale))
end

local function add_waypoint_marker(name, pos, color, scale)
    if not core.localplayer or not pos then
        return nil
    end

    local marker_pos = marker_pos_from(pos)
    if not marker_pos then
        return nil
    end

    return core.localplayer:hud_add({
        hud_elem_type = "waypoint",
        name = name,
        text = "m away",
        number = color or 0xff5555,
        world_pos = marker_pos,
        item = 2,
        scale = scale and {x = scale, y = scale} or nil,
    })
end

local function sync_logoutspots()
    local server_key = marker_server_key()
    if logoutspots_server_key ~= server_key then
        logoutspots_last_online_players = {}
        logoutspots_seen_positions = {}
        clear_logoutspot_markers()
        logoutspots_initialized = false
        logoutspots_server_key = server_key
    end

    local enabled = core.settings:get_bool("logoutspots")
    if not enabled then
        logoutspots_last_online_players = {}
        logoutspots_seen_positions = {}
        clear_logoutspot_markers()
        logoutspots_initialized = false
        return
    end

    local current_set = {}
    local nearby_set = {}
    local local_name = core.localplayer and core.localplayer:get_name() or nil
    local range = tonumber(core.settings:get("logoutspots.range")) or 132
    local scale = logoutspot_scale()

    for _, id in ipairs(logoutspots_marker_ids) do
        core.localplayer:hud_change(id, "scale", {x = scale, y = scale})
    end

    for _, name in ipairs(core.get_player_names() or {}) do
        if name ~= local_name then
            current_set[name] = true
        end
    end

    if core.localplayer and next(current_set) then
        for _, obj in ipairs(core.get_nearby_objects(range) or {}) do
            if obj and obj.is_player and obj:is_player() then
                local name = obj.get_name and obj:get_name() or nil
                if name and current_set[name] then
                    nearby_set[name] = true
                    local pos = obj:get_pos()
                    if pos then
                        logoutspots_seen_positions[name] = marker_pos_from(pos)
                    end
                end
            end
        end
    end

    for name, _ in pairs(current_set) do
        clear_logoutspot_marker(name)
    end
    trim_logoutspot_markers()

    if not logoutspots_initialized then
        logoutspots_last_online_players = current_set
        logoutspots_initialized = true
        return
    end

    for name, _ in pairs(logoutspots_seen_positions) do
        if current_set[name] and not nearby_set[name] then
            logoutspots_seen_positions[name] = nil
        end
    end

    for name, _ in pairs(logoutspots_last_online_players) do
        if not current_set[name] then
            local pos = logoutspots_seen_positions[name]
            if pos then
                clear_logoutspot_marker(name)
                local id = add_waypoint_marker("Logout: " .. name, pos, 0xff8c00, scale)
                if id then
                    logoutspots_markers_by_name[name] = id
                    table.insert(logoutspots_marker_ids, id)
                    table.insert(logoutspots_marker_names, name)
                    trim_logoutspot_markers()
                end
            end
            logoutspots_seen_positions[name] = nil
        end
    end

    logoutspots_last_online_players = current_set
end

local function clear_deathmarker()
    remove_waypoint(deathmarker_id)
    deathmarker_id = nil
end

local function set_deathmarker(pos)
    if not core.settings:get_bool("deathmarker") then
        clear_deathmarker()
        return
    end

    clear_deathmarker()
    deathmarker_id = add_waypoint_marker("Last death", pos, 0xff5555)
end

local function normalize_message_lines(value)
    value = tostring(value or "")
    value = value:gsub("\r\n", "\n"):gsub("\\n", "\n")

    local lines = {}
    for _, line in ipairs(string.split(value, "\n", true)) do
        if line ~= nil and line ~= "" then
            lines[#lines + 1] = line
        end
    end

    return lines
end

local function get_spammer_plus_messages()
    local file_path = core.settings:get("spammer_plus.file_path") or ""
    if file_path ~= "" and not spammer_plus_file_warned then
        spammer_plus_file_warned = true
        minetest.log("warning", "Spammer+: file path input is unsupported in client Lua; using the Messages field instead.")
    end

    local raw_messages = core.settings:get("spammer_plus.messages") or "message"
    local messages = normalize_message_lines(raw_messages)
    if #messages == 0 then
        messages = {"message"}
    end
    return messages
end

local function normalize_avoid_list(value)
    local names = {}
    local targets = {}
    for entry in tostring(value or ""):gmatch("[^,]+") do
        local name = entry:match("^%s*(.-)%s*$")
        if name ~= "" then
            local key = name:lower()
            if not targets[key] then
                targets[key] = true
                names[#names + 1] = name
            end
        end
    end
    return table.concat(names, ","), targets
end

local function sync_avoid()
    local server_key = core.get_server_url and core.get_server_url() or nil
    if not server_key then
        avoid_server_key = nil
        avoid_editor_value = nil
        avoid_disconnect_pending = false
        return
    end

    local data = core.settings:get_json("avoid.players_by_server") or {}
    if avoid_server_key ~= server_key then
        avoid_server_key = server_key
        avoid_editor_value = data[server_key] or ""
        core.settings:set("avoid.players", avoid_editor_value)
        avoid_disconnect_pending = false
    end

    local edited = core.settings:get("avoid.players") or ""
    local normalized, targets = normalize_avoid_list(edited)
    if normalized ~= avoid_editor_value then
        avoid_editor_value = normalized
        data[server_key] = normalized
        core.settings:set_json("avoid.players_by_server", data)
        if edited ~= normalized then
            core.settings:set("avoid.players", normalized)
        end
    end

    if not core.settings:get_bool("avoid") or avoid_disconnect_pending or next(targets) == nil then
        return
    end

    local local_name = core.localplayer and core.localplayer:get_name():lower() or ""
    for _, name in ipairs(core.get_player_names() or {}) do
        local key = name:lower()
        if key ~= local_name and targets[key] then
            avoid_disconnect_pending = true
            core.display_chat_message("Avoid: disconnecting because " .. name .. " is online.")
            core.disconnect()
            return
        end
    end
end

local function format_player_message(template, name)
    template = template or ""
    template = template:gsub("%%player%%", function()
        return name
    end)
    template = template:gsub("%%name%%", function()
        return name
    end)
    template = template:gsub("%$1", function()
        return name
    end)
    return template
end

local function apply_color_tags(message)
    message = tostring(message or "")
    local used_color = false

    message = message:gsub("<#([%x]+)>", function(color)
        used_color = true
        return core.get_color_escape_sequence("#" .. color)
    end)

    if used_color then
        message = message .. core.get_color_escape_sequence("#ffffff")
    end

    return message
end

local function send_greeter_message(setting_name, fallback, name)
    local template = core.settings:get(setting_name)
    if template == nil or template == "" then
        template = fallback
    end
    minetest.send_chat_message(apply_color_tags(format_player_message(template, name)))
end

local message_sent_combat_target_hud = false
local message_sent_coords

local timer = 0

local function getOtherColors(excludedColor)
    local teamColors = {"red", "blue", "green", "orange", "purple"}
    local otherColors = {}
    for _, color in ipairs(teamColors) do
        if color ~= excludedColor then
            table.insert(otherColors, color)
        end
    end

    return otherColors
end

local function findFlags()
	local player = minetest.localplayer
    if not player then return end
    local tcolor = player:get_teamcolor()
    if not tcolor then return end
    local availableColors = getOtherColors(tcolor)
    local nodeNames = {}

    -- Add the flag nodes for each available color
    for _, color in ipairs(availableColors) do
        table.insert(nodeNames, "ctf_modebase:flag_top_" .. color)
    end
    local foundNodes = {}
    for _, nodeName in ipairs(nodeNames) do
        local nodes = minetest.find_nodes_near(player:get_pos(), tonumber(minetest.settings:get("flagaura.range")), {nodeName})
        for _, pos in ipairs(nodes) do
            table.insert(foundNodes, pos) 
        end
    end

    return foundNodes
end

core.register_globalstep(function(dtime)

    -- AutoHeal
	if core.localplayer then
		if heal_cooldown > 0 then
			heal_cooldown = heal_cooldown - dtime
		end

		if heal_cooldown <= 0 and core.localplayer:get_hp() < tonumber(core.settings:get("auto_heal.hp")) and core.settings:get_bool("auto_heal") then
			local current_wield_index = core.localplayer:get_wield_index() + 1
			local food_index = nil

			for index, stack in ipairs(core.get_inventory("current_player").main) do
				if stack and stack:get_name() ~= "" then
					for group_name, _ in pairs(core.get_item_def(stack:get_name()).groups) do
						if group_name:sub(1, 5) == "food_" then
							food_index = index
							break
						end
					end
				end
				if food_index then break end
			end

			if food_index then
				core.localplayer:set_wield_index(food_index)
				heal_cooldown = tonumber(core.settings:get("auto_heal.cooldown"))

				core.after(tonumber(core.settings:get("auto_heal.delay")), function()
					core.interact("use", {type="nothing"})
					core.localplayer:set_wield_index(current_wield_index)
				end)
			end
		end
	end
    local player = minetest.localplayer
    if not player then
        return
    end
    local owner_key = tostring(marker_server_key() or "") .. "|" .. player:get_name()
    if deathmarker_owner_key ~= owner_key then
        clear_deathmarker()
        deathmarker_last_alive_pos = nil
        deathmarker_was_dead = false
        deathmarker_owner_key = owner_key
    end
    if not core.settings:get_bool("deathmarker") then
        clear_deathmarker()
        deathmarker_last_alive_pos = nil
        deathmarker_was_dead = false
    else
        local hp = player:get_hp() or 0
        if hp > 0 then
            local pos = player:get_pos()
            if pos then
                deathmarker_last_alive_pos = marker_pos_from(pos)
            end
            deathmarker_was_dead = false
        elseif not deathmarker_was_dead then
            set_deathmarker(deathmarker_last_alive_pos or player:get_pos())
            deathmarker_was_dead = true
        end
    end
    --AppleAura
    if minetest.settings:get_bool("appleaura") then
        local player_pos = player:get_pos()
        local apple_nodes = minetest.find_nodes_near(player_pos, tonumber(minetest.settings:get("appleaura.range")), {"default:apple"}) -- Find apples within specified range

        if apple_nodes then
            for _, apple_pos in ipairs(apple_nodes) do
                destroy_apple(apple_pos)
            end
        end
    end
    --Spammer+
    if core.settings:get_bool("spammer_plus") then
        spammer_plus_timer = spammer_plus_timer + dtime
        local cooldown = tonumber(core.settings:get("spammer_plus.cooldown")) or 5
        cooldown = math.max(cooldown, 0.1)

        if spammer_plus_timer >= cooldown then
            spammer_plus_timer = spammer_plus_timer - cooldown

            local messages = get_spammer_plus_messages()
            if #messages > 0 then
                if spammer_plus_index > #messages then
                    spammer_plus_index = 1
                end
                minetest.send_chat_message(messages[spammer_plus_index])
                spammer_plus_index = spammer_plus_index + 1
            end
        end
    else
        spammer_plus_timer = 0
        spammer_plus_index = 1
    end

    --Greeter
    greeter_timer = greeter_timer + dtime
    if greeter_timer >= 1 then
        greeter_timer = 0

        local current_players = core.get_player_names() or {}
        local current_set = {}
        local local_name = core.localplayer and core.localplayer:get_name() or nil
        for _, name in ipairs(current_players) do
            if name ~= local_name then
                current_set[name] = true
            end
        end

		if not greeter_initialized then
			greeter_last_online_players = current_set
			greeter_initialized = true
		else
			local greeter_enabled = core.settings:get_bool("greeter")
			local welcome_enabled = core.settings:get_bool("greeter.welcome")
			local goodbye_enabled = core.settings:get_bool("greeter.goodbye")

			if greeter_enabled then
				for name, _ in pairs(current_set) do
					if welcome_enabled and not greeter_last_online_players[name] then
							send_greeter_message(
								"greeter.welcome_message",
								"welcome, <%player%> :^)",
								name
							)
					end
				end

				for name, _ in pairs(greeter_last_online_players) do
					if goodbye_enabled and not current_set[name] then
						send_greeter_message(
							"greeter.goodbye_message",
							"goodbye, <%player%> :^(",
							name
						)
                    end
                end
			else
				greeter_initialized = false
            end
        end

        greeter_last_online_players = current_set
        sync_logoutspots()
    end
    --Avoid
    avoid_timer = avoid_timer + dtime
    if avoid_timer >= 0.2 then
        avoid_timer = 0
        sync_avoid()
    end
    --HUD elements advice thing
    if minetest.settings:get_bool("enable_combat_target_hud") and minetest.settings:get_bool("hud_elements_advice") then
        if not message_sent_combat_target_hud then
            local message = minetest.colorize("#3250af", "[Advice]: To modify this HUD element's (and some others) position and size, open the Click GUI (F8 by default), press 'Edit HUD' button and then you can modify them. You can hide this message with the command .hide_hud_elements_advice")
            ws.dcm(message)
            message_sent_combat_target_hud = true
        end
    else
		message_sent_combat_target_hud = false
	end
    --Anti AFK
    timer = timer + dtime

    if core.settings:get_bool("anti_afk") then
        if timer >= 0 and timer < 0.25 then
            Strata.clear_controls()
            Strata.set_controls({left = true})
        elseif timer >= 0.25 and timer < 0.5 then
            Strata.clear_controls()
            Strata.set_controls({up = true})
        elseif timer >= 0.5 and timer < 0.75 then
            Strata.clear_controls()
            Strata.set_controls({right = true})
        elseif timer >= 0.75 and timer < 1 then
            Strata.clear_controls()
            Strata.set_controls({down = true})
        elseif timer >= 1 then
            Strata.clear_controls()
            timer = 0
        end
    end
    --FlagAura
    if not minetest.settings:get_bool("flagaura") then return end
    if core.get_server_game() == "not_initialized" then return end
    if core.get_server_game() ~= "capturetheflag" then
        core.update_infotext("flagaura", "Misc", "flagaura", "Invalid Game")
        return
    end
	local positions = findFlags()
    if not positions then return end
	for _, pos in ipairs(positions) do
    	minetest.dig_node(pos)
	end
end)
--AutoHeal
core.register_cheat_with_infotext("autoheal", "Misc", "auto_heal", "CTF")
core.register_cheat_description("autoheal", "Misc", "auto_heal", "Automatically eat food if health is below a set value.")

core.register_cheat_setting("Delay", "Misc", "auto_heal", "auto_heal.delay", {type="slider_float", min=0.0, max=1.5, steps=16})
core.register_cheat_setting("Cooldown", "Misc", "auto_heal", "auto_heal.cooldown", {type="slider_float", min=0.0, max=1.5, steps=16})
core.register_cheat_setting("HP", "Misc", "auto_heal", "auto_heal.hp", {type="slider_int", min=1, max=30, steps=30})
--AppleAura
minetest.register_cheat_with_infotext("appleaura", "Misc", "appleaura", "CTF")
minetest.register_cheat_setting("Radius", "Misc", "appleaura", "appleaura.range", {type="slider_float", min=1, max=6, steps=6})
core.register_cheat_description("appleaura", "Misc", "appleaura", "Automatically digs all apples within a specific radius.")
--Greeter
minetest.register_cheat("greeter", "Misc", "greeter")
core.register_cheat_setting("Welcome", "Misc", "greeter", "greeter.welcome", {type="bool", order=1})
core.register_cheat_setting("Welcome Message", "Misc", "greeter", "greeter.welcome_message", {type="text", size=80, order=2})
core.register_cheat_setting("Goodbye", "Misc", "greeter", "greeter.goodbye", {type="bool", order=3})
core.register_cheat_setting("Goodbye Message", "Misc", "greeter", "greeter.goodbye_message", {type="text", size=80, order=4})
--Spammer+
minetest.register_cheat("spammer+", "Misc", "spammer_plus")
core.register_cheat_setting("Cooldown", "Misc", "spammer_plus", "spammer_plus.cooldown", {type="slider_int", min=1, max=50, steps=50})
core.register_cheat_setting("Messages", "Misc", "spammer_plus", "spammer_plus.messages", {type="text", size=120})

core.register_on_death(function()
    local pos = deathmarker_last_alive_pos
    if not pos and core.localplayer then
        pos = core.localplayer:get_pos()
    end
    set_deathmarker(pos)
    deathmarker_was_dead = true
end)

core.register_on_shutdown(function()
    clear_deathmarker()
    clear_logoutspot_markers()
end)


--Commands

minetest.register_chatcommand("hide_hud_elements_advice", {
    func = function()
        core.settings:set_bool("hud_elements_advice", false)
    end,
})

-- I don't know why you'd wanna use this
minetest.register_chatcommand("show_hud_elements_advice", {
    func = function()
        core.settings:set_bool("hud_elements_advice", true)
    end,
})
minetest.register_chatcommand("lenny", {
    func = function()
        core.send_chat_message("( ͡° ͜ʖ ͡°)")
    end,
})
