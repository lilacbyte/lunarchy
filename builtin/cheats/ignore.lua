local IGNORE_SETTING = "ignore.players"
local WHITELIST_SETTING = "ignore.whitelist"
local WHITELIST_ONLY_SETTING = "ignore.whitelist_only"
local MINECLONIA_SETTING = "ignore.mineclonia"

local function trim(value)
	return (tostring(value or ""):gsub("^%s+", ""):gsub("%s+$", ""))
end

local function clean_name(value)
	local name = trim(value)
	if #name >= 2 then
		local first = name:sub(1, 1)
		local last = name:sub(-1)
		if (first == "'" and last == "'") or (first == '"' and last == '"') then
			name = trim(name:sub(2, -2))
		end
	end
	return name
end

local function read_names(setting)
	local names = {}
	for entry in (core.settings:get(setting) or ""):gmatch("[^,]+") do
		local name = clean_name(entry)
		if name ~= "" then
			names[#names + 1] = name
		end
	end
	return names
end

local function name_index(names, wanted)
	wanted = wanted:lower()
	for index, name in ipairs(names) do
		if name:lower() == wanted then
			return index
		end
	end
	return nil
end

local function name_set(setting)
	local set = {}
	for _, name in ipairs(read_names(setting)) do
		set[name:lower()] = true
	end
	return set
end

local function save_names(setting, names)
	core.settings:set(setting, table.concat(names, ","))
	if core.settings.write then
		core.settings:write()
	end
end

local function set_whitelist_only(enabled)
	core.settings:set_bool(WHITELIST_ONLY_SETTING, enabled)
	if core.settings.write then
		core.settings:write()
	end
end

local function edit_list(setting, label, action, raw_name)
	local names = read_names(setting)

	if action == "list" then
		if #names == 0 then
			return true, label .. " is empty."
		end
		return true, label .. ": " .. table.concat(names, ", ")
	elseif action == "clear" then
		save_names(setting, {})
		return true, "Cleared " .. label:lower() .. "."
	end

	local name = clean_name(raw_name)
	if name == "" then
		return false, "Missing player name."
	end

	local index = name_index(names, name)
	if action == "add" then
		if index then
			return false, name .. " is already in " .. label:lower() .. "."
		end
		names[#names + 1] = name
		save_names(setting, names)
		return true, "Added " .. name .. " to " .. label:lower() .. "."
	elseif action == "del" or action == "delete" or action == "remove" then
		if not index then
			return false, name .. " is not in " .. label:lower() .. "."
		end
		table.remove(names, index)
		save_names(setting, names)
		return true, "Removed " .. name .. " from " .. label:lower() .. "."
	end

	return false, "Invalid action. Use add, del, list, or clear."
end

core.register_chatcommand("ignore", {
	params = "add <player> | del <player> | list | clear | whitelist <add/del/list/clear/on/off> [player]",
	description = "Hide chat messages from selected players",
	func = function(param)
		local action, rest = trim(param):match("^(%S+)%s*(.*)$")
		action = action and action:lower() or ""

		if action == "whitelist" then
			local whitelist_action, name = trim(rest):match("^(%S+)%s*(.*)$")
			whitelist_action = whitelist_action and whitelist_action:lower() or ""
			if whitelist_action == "on" or whitelist_action == "off" then
				local enabled = whitelist_action == "on"
				set_whitelist_only(enabled)
				return true, "Whitelist-only mode " .. (enabled and "enabled." or "disabled.")
			end
			return edit_list(WHITELIST_SETTING, "Whitelist", whitelist_action, name)
		end

		return edit_list(IGNORE_SETTING, "Ignore list", action, rest)
	end,
})

local function plain_chat(message)
	local plain = core.strip_colors and core.strip_colors(message) or message
	-- Translation/control escapes may wrap otherwise ordinary chat messages.
	plain = plain:gsub("\27%(T@[^)]*%)", "")
	plain = plain:gsub("\27[TEF]", "")
	return trim(plain)
end

local function extract_mineclonia_sender(message)
	-- Public chat comes from chat_message_format: <@name> @message.
	local name = clean_name(message:match("^<([^<>]+)>%s*") or "")
	if name == "" then
		-- Mineclonia uses Luanti's /msg response: DM from @name: @message.
		name = clean_name(message:match("^DM%s+from%s+([^:]+):%s*") or "")
	end
	name = name:gsub("^@", "")
	return name ~= "" and name:lower() or nil
end

local function contains_player_name(message, wanted)
	local lower_message = message:lower()
	local lower_name = wanted:lower()
	local start = 1
	while true do
		local first, last = lower_message:find(lower_name, start, true)
		if not first then
			return false
		end
		local before = first > 1 and lower_message:sub(first - 1, first - 1) or ""
		local after = last < #lower_message and lower_message:sub(last + 1, last + 1) or ""
		if not before:match("[%w_-]") and not after:match("[%w_-]") then
			return true
		end
		start = last + 1
	end
end

core.register_on_receiving_chat_message(function(message)
	if not core.settings:get_bool("ignore") then
		return false
	end

	local ignored = name_set(IGNORE_SETTING)
	local whitelist = name_set(WHITELIST_SETTING)
	local plain = plain_chat(message)

	if core.settings:get_bool(MINECLONIA_SETTING) then
		local sender = extract_mineclonia_sender(plain)
		if not sender or whitelist[sender] then
			return false
		end
		if ignored[sender] then
			return true
		end
		return core.settings:get_bool(WHITELIST_ONLY_SETTING) == true
	end

	-- Legacy mode suppresses the entire received line when an ignored player's
	-- exact name occurs anywhere in it. Whitelist matches take priority.
	for name in pairs(whitelist) do
		if contains_player_name(plain, name) then
			return false
		end
	end
	for name in pairs(ignored) do
		if contains_player_name(plain, name) then
			return true
		end
	end
	return core.settings:get_bool(WHITELIST_ONLY_SETTING) == true
end)

core.register_cheat("ignore", "Misc", "ignore")
core.register_cheat_description("ignore", "Misc", "ignore",
	"Hide chat messages from ignored players; whitelisted players always remain visible")
core.register_cheat_setting("Ignored players", "Misc", "ignore", IGNORE_SETTING,
	{type = "text", size = 80, order = 1})
core.register_cheat_setting("Whitelist", "Misc", "ignore", WHITELIST_SETTING,
	{type = "text", size = 80, order = 2})
core.register_cheat_setting("Mineclonia", "Misc", "ignore", MINECLONIA_SETTING,
	{type = "bool", order = 3})
core.register_cheat_setting("Whitelist only", "Misc", "ignore", WHITELIST_ONLY_SETTING,
	{type = "bool", order = 4})
