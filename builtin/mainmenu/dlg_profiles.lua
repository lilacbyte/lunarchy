local profiles_dir = core.get_user_path() .. DIR_DELIM .. "client"
local profiles_file = profiles_dir .. DIR_DELIM .. "profiles.json"
local save_profiles

local function trim(text)
	return (text or ""):gsub("^%s+", ""):gsub("%s+$", "")
end

local function load_profiles()
	local contents = core.safe_file_read(profiles_file)
	if contents then
		local parsed = core.parse_json(contents, nil, true)
		if type(parsed) == "table" then
			table.sort(parsed, function(a, b)
				return (a.name or ""):lower() < (b.name or ""):lower()
			end)
			return parsed
		end
	end

	local legacy = core.settings:get_json("lunarchy.profiles") or {}
	if type(legacy) == "table" and next(legacy) ~= nil then
		save_profiles(legacy)
		return legacy
	end

	return {}
end

save_profiles = function(profiles)
	assert(core.mkdir(profiles_dir))
	local json = core.write_json(profiles, true) or "[]"
	core.safe_file_write(profiles_file, json)
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

	for _, setting_id in ipairs(core.settings:get_names()) do
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

local function selected_profile(profiles, index)
	index = tonumber(index) or 1
	return profiles[index], index
end

local function profile_label(profile)
	local name = trim(profile.name or "")
	return name
end

local function profiles_text(profiles)
	if #profiles == 0 then
		return fgettext("No profiles saved")
	end

	local names = {}
	for i, profile in ipairs(profiles) do
		names[i] = core.formspec_escape(profile_label(profile))
	end
	return table.concat(names, ",")
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

local function get_formspec(data)
	data.profiles = data.profiles or load_profiles()
	local profiles = data.profiles
	local profile, selected = selected_profile(profiles, data.selected_index)
	data.selected_index = selected

	if data.profile_name == nil then
		data.profile_name = profile and profile.name or ""
	end

	local setting_count = 0
	if profile and profile.settings then
		for _ in pairs(profile.settings) do
			setting_count = setting_count + 1
		end
	end

	return table.concat({
		"formspec_version[8]",
		"size[11.4,9.1]",
		"bgcolor[;neither;]",
		"box[0,0;11.4,9.1;#1f2328]",
		"style_type[*;border=false;textcolor=white;font_size=*1.25;font=bold]",
		"label[0.55,1.12;" .. fgettext("Profiles") .. "]",
		"style_type[image_button;border=false;textcolor=white;font_size=*1.7;padding=0;font=bold;" ..
			"bgimg=" .. core.formspec_escape(defaulttexturedir .. "menu_button.png") .. ";" ..
			"bgimg_hovered=" .. core.formspec_escape(defaulttexturedir .. "menu_button_hovered.png") .. "]",
		"textlist[0.45,1.7;4.35,6.35;profiles;" .. profiles_text(profiles) .. ";" .. selected .. "]",
		"label[5.3,1.7;" .. fgettext("Profile Name") .. ":]",
		"field[5.3,2.05;5.45,0.8;profile_name;;" .. core.formspec_escape(data.profile_name or "") .. "]",
		"label[5.3,3.0;" .. fgettext("Saved settings") .. ": " .. tostring(setting_count) .. "]",
		"image_button[5.3,3.8;5.45,0.75;;add;" .. fgettext("Add") .. "]",
		"image_button[5.3,4.7;5.45,0.75;;select;" .. fgettext("Select") .. "]",
		"image_button[5.3,5.6;5.45,0.75;;remove;" .. fgettext("Remove") .. "]",
		"image_button[5.3,6.5;5.45,0.75;;back;" .. fgettext("Back") .. "]"
	}, "\n")
end

local function upsert_profile(profiles, name)
	name = trim(name)
	if name == "" then
		name = default_profile_name(profiles)
	end

	local snapshot = collect_settings_snapshot()
	for i, profile in ipairs(profiles) do
		if profile.name == name then
			return false, fgettext("Profile already exists")
		end
	end

	table.insert(profiles, {
		name = name,
		settings = snapshot,
	})

	table.sort(profiles, function(a, b)
		return (a.name or ""):lower() < (b.name or ""):lower()
	end)
	save_profiles(profiles)
	return true, name
end

local function buttonhandler(this, fields)
	local profiles = this.data.profiles or load_profiles()
	this.data.profiles = profiles

	if fields.profiles then
		local event = core.explode_textlist_event(fields.profiles)
		if event.type == "CHG" or event.type == "DCL" then
			this.data.selected_index = event.index
			local profile = profiles[event.index]
			if profile then
				this.data.profile_name = profile.name or ""
			end
			return true
		end
	end

	if fields.profile_name then
		this.data.profile_name = fields.profile_name
	end

	if fields.add then
		local ok, saved_name = upsert_profile(profiles, fields.profile_name or this.data.profile_name or "")
		if not ok then
			gamedata.errormessage = saved_name
			return true
		end
		this.data.profiles = load_profiles()
		if core.refresh_cheat_settings then
			core.refresh_cheat_settings()
		end
		for i, profile in ipairs(this.data.profiles) do
			if profile.name == trim(saved_name or fields.profile_name or this.data.profile_name or "") then
				this.data.selected_index = i
				this.data.profile_name = profile.name
				break
			end
		end
		return true
	end

	if fields.select then
		local profile = profiles[tonumber(this.data.selected_index) or 1]
		if profile then
			apply_settings_snapshot(profile.settings)
		end
		return true
	end

	if fields.remove then
		local index = tonumber(this.data.selected_index) or 0
		if index >= 1 and index <= #profiles then
			table.remove(profiles, index)
			save_profiles(profiles)
			this.data.profiles = load_profiles()
			if core.refresh_cheat_settings then
				core.refresh_cheat_settings()
			end
			this.data.selected_index = math.min(index, #this.data.profiles)
			local profile = this.data.profiles[this.data.selected_index]
			this.data.profile_name = profile and profile.name or ""
		end
		return true
	end

	if fields.back then
		this:delete()
		return true
	end
end

local function eventhandler(event)
	if event == "DialogShow" then
		mm_game_theme.set_engine()
		return true
	elseif event == "MenuQuit" then
		local mainmenu = ui.find_by_name("mainmenu")
		local current = ui.find_by_name("dlg_profiles")
		current:delete()
		if mainmenu then
			mainmenu:show()
		end
		ui.update()
		return true
	end
	return false
end

function create_profiles_dlg()
	local dlg = dialog_create("dlg_profiles", get_formspec, buttonhandler, eventhandler)
	return dlg
end
