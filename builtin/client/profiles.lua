local function trim(text)
	return (text or ""):gsub("^%s+", ""):gsub("%s+$", "")
end

local function load_profiles()
	local parsed = core.settings:get_json("lunarchy.profiles") or {}
	if type(parsed) ~= "table" then
		return {}
	end

	table.sort(parsed, function(a, b)
		return (a.name or ""):lower() < (b.name or ""):lower()
	end)
	return parsed
end

local function save_profiles(profiles)
	core.settings:set_json("lunarchy.profiles", profiles)
	if core.settings.write then
		core.settings:write()
	end
end

local function collect_settings_snapshot()
	local snapshot = {}
	local seen = {}

	local function add_setting(setting_id)
		if not setting_id or setting_id == "" or seen[setting_id] then
			return
		end
		seen[setting_id] = true
		if setting_id:sub(1, 9) == "profiles." then
			return
		end
		local value = core.settings:get(setting_id)
		if value ~= nil then
			snapshot[setting_id] = value
		end
	end

	for _, category in pairs(core.cheats or {}) do
		for _, cheat in pairs(category) do
			if type(cheat) == "string" then
				add_setting(cheat)
			elseif type(cheat) == "table" then
				add_setting(cheat.setting)
			end
		end
	end

	for _, category in pairs(core.cheat_settings or {}) do
		for _, cheat in pairs(category) do
			for setting_id, _ in pairs(cheat) do
				add_setting(setting_id)
			end
		end
	end

	for _, setting_id in ipairs(core.settings:get_names() or {}) do
		if setting_id:sub(1, #"HudElement_") == "HudElement_" then
			add_setting(setting_id)
		end
	end
	return snapshot
end

local function apply_settings_snapshot(snapshot)
	if type(snapshot) ~= "table" then
		return
	end
	for setting_id, value in pairs(snapshot) do
		if value ~= nil then
			core.settings:set(setting_id, tostring(value))
		end
	end
end

local function profile_names(profiles)
	local names = {}
	if #profiles == 0 then
		names[1] = fgettext("No profiles saved")
		return names
	end
	for i, profile in ipairs(profiles) do
		names[i] = trim(profile.name or "")
	end
	return names
end

local function profile_selector_options(profiles)
	local options = profile_names(profiles)
	if #options == 0 then
		options[1] = ""
	end
	return options
end

local function default_profile_name(profiles)
	local used = {}
	for _, profile in ipairs(profiles) do
		used[trim(profile.name or "")] = true
	end

	local index = 1
	while used["Profile " .. index] do
		index = index + 1
	end

	return "Profile " .. index
end

local function find_profile_by_name(profiles, name)
	name = trim(name)
	if name == "" then
		return nil
	end
	for _, profile in ipairs(profiles) do
		if trim(profile.name or "") == name then
			return profile
		end
	end
	return nil
end

local function normalize_profile_name(name)
	name = trim(name)
	if name == fgettext("No profiles saved") then
		return ""
	end
	return name
end

local function apply_profile(profile)
	if not profile then
		return
	end

	apply_settings_snapshot(profile.settings)

	if core.refresh_cheat_settings then
		core.refresh_cheat_settings()
	end
end

local function current_profile_name()
	return normalize_profile_name(core.settings:get("profiles.selected") or "")
end

local function set_current_profile_name(name)
	core.settings:set("profiles.selected", trim(name))
end

local profiles_cache = load_profiles()
local last_applied_profile = ""

local function refresh_profile_selector(skip_refresh)
	profiles_cache = load_profiles()
	local selected = current_profile_name()
	if (#profiles_cache > 0) and not find_profile_by_name(profiles_cache, selected) then
		selected = trim(profiles_cache[1].name or "")
		set_current_profile_name(selected)
	end
	local cheat_setting = core.cheat_settings
		and core.cheat_settings.Client
		and core.cheat_settings.Client["profiles.enabled"]
		and core.cheat_settings.Client["profiles.enabled"]["profiles.selected"]
	if cheat_setting then
		cheat_setting.options = profile_selector_options(profiles_cache)
	end
	if not skip_refresh and core.refresh_cheat_settings then
		core.refresh_cheat_settings()
	end
	return profiles_cache
end

profiles_cache = refresh_profile_selector(true)

local function upsert_profile(profiles, name, allow_overwrite)
	name = trim(name)
	if name == "" then
		name = default_profile_name(profiles)
	end

	for _, profile in ipairs(profiles) do
		if profile.name == name then
			if not allow_overwrite then
				return false, "Profile already exists", true
			end
			profile.settings = collect_settings_snapshot()
			save_profiles(profiles)
			return true, name
		end
	end

	local entry = {
		name = name,
		settings = collect_settings_snapshot(),
	}

	table.insert(profiles, entry)
	table.sort(profiles, function(a, b)
		return (a.name or ""):lower() < (b.name or ""):lower()
	end)
	save_profiles(profiles)
	return true, name
end

core.register_cheat("profiles", "Client", "profiles.enabled")
core.register_cheat_description("profiles", "Client", "profiles.enabled", "Switch between saved cheat profiles")
core.register_cheat_setting("selected profile", "Client", "profiles.enabled", "profiles.selected", {
	type = "selectionbox",
	options = profile_selector_options(profiles_cache),
})
core.register_cheat_setting("save", "Client", "profiles.enabled", "profiles.save", {
	type = "bool",
})

core.register_globalstep(function()
	if not core.settings:get_bool("profiles.enabled") then
		last_applied_profile = ""
		return
	end

	if core.settings:get_bool("profiles.save") then
		local save_name = current_profile_name()
		local ok, saved_name = core.save_selected_profile(save_name)
		if ok and saved_name then
			set_current_profile_name(saved_name)
		end
		core.settings:set_bool("profiles.save", false)
		refresh_profile_selector(true)
	end

	local profiles = profiles_cache or load_profiles()
	local selected_name = current_profile_name()
	if selected_name == last_applied_profile then
		return
	end

	last_applied_profile = selected_name
	local profile = find_profile_by_name(profiles, selected_name)
	if profile then
		apply_profile(profile)
	end
end)

function core.save_selected_profile(name)
	local profiles = refresh_profile_selector()
	local ok, saved_name = upsert_profile(profiles, name, true)
	if ok then
		refresh_profile_selector()
		return true, saved_name
	end
	return false, saved_name
end
