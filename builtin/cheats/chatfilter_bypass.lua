chateffects = {}
local gradient_storage = core.get_mod_storage()
local gradient_setting_name = "gradient_chat"

local function trim(s)
	return (s:gsub("^%s+", ""):gsub("%s+$", ""))
end

local function normalize_color(color)
	local spec = core.colorspec_to_table(color)
	if not spec then
		return nil
	end

	return string.format("#%02X%02X%02X", spec.r, spec.g, spec.b)
end

local function split_lines(text)
	local lines = {}
	local start = 1

	while true do
		local nl = text:find("\n", start, true)
		if not nl then
			lines[#lines + 1] = text:sub(start)
			break
		end

		lines[#lines + 1] = text:sub(start, nl - 1)
		start = nl + 1
	end

	return lines
end

local function utf8_chars(text)
	local chars = {}
	for char in text:gmatch("[\1-\127\194-\244][\128-\191]*") do
		chars[#chars + 1] = char
	end

	if #chars == 0 and text ~= "" then
		for i = 1, #text do
			chars[#chars + 1] = text:sub(i, i)
		end
	end

	return chars
end

local function gradientize_line(text, start_color, end_color)
	local chars = utf8_chars(text)
	local count = #chars
	if count == 0 then
		return text
	end

	local start_spec = core.colorspec_to_table(start_color)
	local end_spec = core.colorspec_to_table(end_color or start_color)
	if not start_spec or not end_spec then
		return text
	end

	local output = {}
	local steps = math.max(count - 1, 1)
	for index, char in ipairs(chars) do
		local t = (index - 1) / steps
		local r = math.floor(start_spec.r + ((end_spec.r - start_spec.r) * t) + 0.5)
		local g = math.floor(start_spec.g + ((end_spec.g - start_spec.g) * t) + 0.5)
		local b = math.floor(start_spec.b + ((end_spec.b - start_spec.b) * t) + 0.5)
		output[#output + 1] = core.get_color_escape_sequence(string.format("#%02X%02X%02X", r, g, b)) .. char
	end

	output[#output + 1] = core.get_color_escape_sequence("#ffffff")
	return table.concat(output)
end

local function lerp_color(start_spec, end_spec, t)
	local r = math.floor(start_spec.r + ((end_spec.r - start_spec.r) * t) + 0.5)
	local g = math.floor(start_spec.g + ((end_spec.g - start_spec.g) * t) + 0.5)
	local b = math.floor(start_spec.b + ((end_spec.b - start_spec.b) * t) + 0.5)
	return string.format("#%02X%02X%02X", r, g, b)
end

local function gradientize_line_stops(text, colors)
	local chars = utf8_chars(text)
	local count = #chars
	local stops = #colors
	if count == 0 or stops == 0 then
		return text
	end
	if stops == 1 then
		return gradientize_line(text, colors[1], colors[1])
	end

	local specs = {}
	for i, color in ipairs(colors) do
		specs[i] = core.colorspec_to_table(color)
		if not specs[i] then
			return text
		end
	end

	local output = {}
	local segments = stops - 1
	local denom = math.max(count - 1, 1)

	for index, char in ipairs(chars) do
		local global_t = (index - 1) / denom
		local scaled = global_t * segments
		local seg_index = math.min(math.floor(scaled) + 1, stops - 1)
		local seg_t = scaled - (seg_index - 1)
		local color = lerp_color(specs[seg_index], specs[seg_index + 1], seg_t)
		output[#output + 1] = core.get_color_escape_sequence(color) .. char
	end

	output[#output + 1] = core.get_color_escape_sequence("#ffffff")
	return table.concat(output)
end

local function gradientize_text(text, start_color, end_color)
	if type(start_color) == "table" then
		return gradientize_line_stops(tostring(text), start_color)
	end

	local lines = split_lines(tostring(text))
	for index, line in ipairs(lines) do
		lines[index] = gradientize_line(line, start_color, end_color)
	end

	return table.concat(lines, "\n")
end

local gradient_state = {
	enabled = false,
	start_color = nil,
	end_color = nil,
}

local function serialize_gradient_state()
	if not gradient_state.enabled or not gradient_state.start_color then
		return ""
	end

	return table.concat({
		gradient_state.start_color,
		gradient_state.end_color or gradient_state.start_color,
	}, "|")
end

local function deserialize_gradient_state(value)
	value = trim(value or "")
	if value == "" then
		return false, nil, nil
	end

	local parts = {}
	for part in value:gmatch("[^|]+") do
		parts[#parts + 1] = trim(part)
	end

	local start_color = normalize_color(parts[1])
	if not start_color then
		return false, nil, nil
	end

	local end_color = normalize_color(parts[2] or parts[1]) or start_color
	return true, start_color, end_color
end

local function save_gradient_state()
	gradient_storage:set_string("gradient_enabled", gradient_state.enabled and "1" or "0")
	gradient_storage:set_string("gradient_start_color", gradient_state.start_color or "")
	gradient_storage:set_string("gradient_end_color", gradient_state.end_color or "")
	core.settings:set(gradient_setting_name, serialize_gradient_state())
	core.settings:write()
end

local function load_gradient_state()
	local setting_enabled, setting_start, setting_end = deserialize_gradient_state(core.settings:get(gradient_setting_name))
	if setting_enabled then
		gradient_state.enabled = true
		gradient_state.start_color = setting_start
		gradient_state.end_color = setting_end
	else
		gradient_state.enabled = gradient_storage:get_string("gradient_enabled") == "1"
		gradient_state.start_color = normalize_color(gradient_storage:get_string("gradient_start_color"))
		gradient_state.end_color = normalize_color(gradient_storage:get_string("gradient_end_color"))
	end

	if gradient_state.enabled and not gradient_state.start_color then
		gradient_state.enabled = false
	end

	if gradient_state.enabled and not gradient_state.end_color then
		gradient_state.end_color = gradient_state.start_color
	end
end

local function parse_gradient_command(param)
	param = trim(param or "")
	if param == "" or param == "off" or param == "reset" then
		return true, nil, nil, nil
	end

	local cleaned = param:gsub(",", " ")
	local tokens = {}
	for token in cleaned:gmatch("%S+") do
		tokens[#tokens + 1] = token
	end

	if #tokens == 0 then
		return true, nil, nil, nil
	end

	local start_color = normalize_color(tokens[1])
	if not start_color then
		return false, "Invalid gradient color: " .. tostring(tokens[1])
	end

	local end_color = start_color
	local text_start = 2
	if tokens[2] and normalize_color(tokens[2]) then
		end_color = normalize_color(tokens[2])
		text_start = 3
	end

	local text = nil
	if #tokens >= text_start then
		text = table.concat(tokens, " ", text_start)
		if text == "" then
			text = nil
		end
	end

	return true, start_color, end_color, text
end

local function parse_chat_color_setting(color)
	color = trim(color or "")
	if color == "" then
		return nil
	end

	local cleaned = color:gsub(",", " ")
	local tokens = {}
	for token in cleaned:gmatch("%S+") do
		tokens[#tokens + 1] = token
	end

	local colors = {}
	for _, token in ipairs(tokens) do
		local parsed = normalize_color(token)
		if parsed then
			colors[#colors + 1] = parsed
		end
	end

	if #colors > 0 then
		return colors
	end

	return nil
end

local function derp_text(text)
	local upper_next = false
	local output = {}

	for _, char in ipairs(utf8_chars(tostring(text))) do
		if char:match("%a") then
			if upper_next then
				output[#output + 1] = string.upper(char)
			else
				output[#output + 1] = string.lower(char)
			end
			upper_next = not upper_next
		else
			output[#output + 1] = char
			if char:match("%s") then
				upper_next = false
			end
		end
	end

	return table.concat(output)
end

local function redact_text(text)
	local output = {}

	for _, char in ipairs(utf8_chars(tostring(text))) do
		if char:match("%s") then
			output[#output + 1] = char
		else
			output[#output + 1] = "█"
		end
	end

	return table.concat(output)
end

local function apply_greentext(message)
	local green = core.get_color_escape_sequence("#789922")
	local reset = core.get_color_escape_sequence("#ffffff")
	local lines = split_lines(tostring(message))

	for i, line in ipairs(lines) do
		if line:match("^>") then
			lines[i] = green .. line .. reset
		end
	end

	return table.concat(lines, "\n")
end

local function apply_saved_gradient(message)
	if not gradient_state.enabled or not gradient_state.start_color then
		return message
	end

	return gradientize_text(message, gradient_state.start_color, gradient_state.end_color or gradient_state.start_color)
end

load_gradient_state()

--[[

 All credit goes to nathan422.

 This function replaces specific ASCII letters with similar-looking letters.
 It is used to bypass chat filters that may block certain words or phrases.

 This file requires frequent updates to keep up with the latest chat filter bypass techniques.
 
--]]
local function replace_ascii_with_similar_text(text)
    local mapping = {
    
			-- Uppercase
		['A'] = 'Α',  -- Greek Alpha
		['B'] = 'Β',  -- Greek Beta
		['C'] = 'С',  -- Cyrillic Es
		--['D'] = 'D',  -- Greek Delta
		['E'] = 'Ε',  -- Greek Epsilon
		--['F'] = 'Ф',  -- Cyrillic Ef
		--['G'] = 'Γ',  -- Greek Gamma
		['H'] = 'Η',  -- Greek Eta
		['I'] = 'Ι',  -- Greek Iota
		['J'] = 'Ј',  -- Cyrillic Je
		['K'] = 'Κ',  -- Greek Kappa
		--['L'] = 'Л',  -- Cyrillic El
		['M'] = 'Μ',  -- Greek Mu
		['N'] = 'Ν',  -- Greek Nu
		['O'] = 'Ο',  -- Greek Omicron
		['P'] = 'Ρ',  -- Greek Rho
		--['Q'] = 'Қ',  -- Cyrillic Ka with descender
		--['R'] = 'Я',  -- Cyrillic Ya
		--['S'] = 'Σ',  -- Greek Sigma
		['T'] = 'Τ',  -- Greek Tau
		--['U'] = 'Ц',  -- Cyrillic Tse
		--['V'] = 'Л',  -- Cyrillic El (inverted V shape)
		--['W'] = 'Ш',  -- Cyrillic Sha
		['X'] = 'Χ',  -- Greek Chi
		['Y'] = 'Υ',  -- Greek Upsilon
		['Z'] = 'Ζ',  -- Greek Zeta

		-- Lowercase
		['a'] = 'α',  -- Greek alpha
		--['b'] = 'β',  -- Greek beta
		['c'] = 'ϲ​',  -- Greek lunate sigma
		--['d'] = 'δ',  -- Greek delta
		--['e'] = 'ε',  -- Greek epsilon
		--['f'] = 'ф',  -- Cyrillic ef
		--['g'] = 'γ',  -- Greek gamma
		--['h'] = 'η',  -- Greek eta
		--['i'] = 'ι',  -- Greek iota
		['i'] = 'i​',  -- i with space
		['j'] = 'ј',  -- Cyrillic je
		['k'] = 'κ',  -- Greek kappa
		--['l'] = 'λ',  -- Greek lamda
		--['m'] = 'μ',  -- Greek mu
		--['n'] = 'ν',  -- Greek nu
		['o'] = 'ο',  -- Greek omicron
		['p'] = 'ρ',  -- Greek rho
		--['q'] = 'қ',  -- Cyrillic ka with descender
		--['r'] = 'я',  -- Cyrillic ya
		--['s'] = 'σ',  -- Greek sigma
		--['t'] = 'τ',  -- Greek tau
		['u'] = 'υ​',  -- Greek upsilon
		['v'] = 'ν',  -- Greek nu
		['w'] = 'ω',  -- Greek omega (closest to “w” curve)
		['x'] = 'χ',  -- Greek chi
		['y'] = 'у',  -- Cyrillic u (better match than Υ)
		--['z'] = 'ζ',  -- Greek zeta
        
    }
    
    return text:gsub(".", function(character)
        return mapping[character] or character
    end)
end

function core.colorize(color, message)
	local lines = tostring(message):split("\n", true)
	local color_code = core.get_color_escape_sequence(color)

	for i, line in ipairs(lines) do
		lines[i] = color_code .. line
	end

	return table.concat(lines, "\n") .. core.get_color_escape_sequence("#ffffff")
end

local function rgb_to_hex(rgb)
	local hexadecimal = "#"

	for key, value in pairs(rgb) do
		local hex = ""

		while(value > 0)do
			local index = math.fmod(value, 16) + 1
			value = math.floor(value / 16)
			hex = string.sub("0123456789ABCDEF", index, index) .. hex
		end

		if(string.len(hex) == 0)then
			hex = "00"
		elseif(string.len(hex) == 1)then
			hex = "0" .. hex
		end

		hexadecimal = hexadecimal .. hex
	end

	return hexadecimal
end

local function color_from_hue(hue)
	local h = hue / 60
	local c = 255
	local x = (1 - math.abs(h % 2 - 1)) * 255

	local i = math.floor(h)
	if i == 0 then
		return rgb_to_hex({c, x, 0})
	elseif i == 1 then
		return rgb_to_hex({x, c, 0})
	elseif i == 2 then
		return rgb_to_hex({0, c, x})
	elseif i == 3 then
		return rgb_to_hex({0, x, c})
	elseif i == 4 then
		return rgb_to_hex({x, 0, c})
	else
		return rgb_to_hex({c, 0, x})
	end
end

function core.rainbow(input)
	local step = 360 / input:len()
	local hue = 0
	local output = ""
	for i = 1, input:len() do
		local char = input:sub(i, i)
		if char:match("%s") then
			output = output .. char
		else
			output = output  .. core.get_color_escape_sequence(color_from_hue(hue)) .. char
		end
		hue = hue + step
	end
	return output
end

function chateffects.send(message)
	if message == "=gradient" or message:sub(1, 10) == "=gradient " then
		local ok, start_color, end_color, text_or_err = parse_gradient_command(message:sub(10))
		if not ok then
			core.display_chat_message("-!- " .. text_or_err)
			return true
		end

		if not start_color then
			gradient_state.enabled = false
			gradient_state.start_color = nil
			gradient_state.end_color = nil
			save_gradient_state()
			core.display_chat_message("[Gradient] disabled")
			return true
		end

		gradient_state.enabled = true
		gradient_state.start_color = start_color
		gradient_state.end_color = end_color or start_color
		save_gradient_state()
		core.display_chat_message("[Gradient] saved " .. gradient_state.start_color .. " -> " .. gradient_state.end_color)

		if text_or_err and text_or_err ~= "" then
			core.send_chat_message(apply_saved_gradient(text_or_err))
		end

		return true
	end

	local starts_with = message:sub(1, 1)
	
	if starts_with == "/" or starts_with == "." then return end

	if not core.settings:get_bool("use_chat_effects") and not gradient_state.enabled then return end

	local reverse = core.settings:get_bool("chat_reverse")
	
	if reverse then
		local msg = ""
		for i = 1, #message do
			msg = message:sub(i, i) .. msg
		end
		message = msg
	end

	if core.settings:get_bool("chat_derp") then
		message = derp_text(message)
	end

	if core.settings:get_bool("chat_redacted") then
		message = redact_text(message)
	end

	if core.settings:get_bool("green_text") then
		message = apply_greentext(message)
	end
	
	local use_chat_color = core.settings:get_bool("use_chat_color")
	local color = core.settings:get("chat_color") or "rainbow"

	if gradient_state.enabled and gradient_state.start_color then
		message = apply_saved_gradient(message)
	elseif use_chat_color then
		local gradient_colors = parse_chat_color_setting(color)
		local msg
		if gradient_colors and #gradient_colors > 1 then
			msg = gradientize_text(message, gradient_colors)
		elseif gradient_colors and #gradient_colors == 1 then
			msg = core.colorize(gradient_colors[1], message)
		elseif color == "rainbow" then
			msg = core.rainbow(message)
		else
			msg = core.colorize(color, message)
		end
		message = msg
	end
	
	local bp_filter = core.settings:get_bool("bypass_filter")
	if bp_filter then
		
		local msg = message

		message = replace_ascii_with_similar_text(msg)		

	end

	core.send_chat_message(message)
	return true


end

core.register_on_sending_chat_message(chateffects.send)

core.register_cheat("chateffects", "Misc", "use_chat_effects")
core.register_cheat_description("chateffects", "Misc", "use_chat_effects", "Custom chat effects, such as color, greentext, reverse text, and more.")
core.register_cheat_setting("Colored", "Misc", "use_chat_effects", "use_chat_color", {type="bool"})
core.register_cheat_setting("Chat Color", "Misc", "use_chat_effects", "chat_color", {type="text", size=18})
core.register_cheat_setting("GreenText", "Misc", "use_chat_effects", "green_text", {type="bool"})
core.register_cheat_setting("Derp", "Misc", "use_chat_effects", "chat_derp", {type="bool"})
core.register_cheat_setting("Redacted", "Misc", "use_chat_effects", "chat_redacted", {type="bool"})
core.register_cheat_setting("Reversed", "Misc", "use_chat_effects", "chat_reverse", {type="bool"})
core.register_cheat_setting("BypassFilter", "Misc", "use_chat_effects", "bypass_filter", {type="bool"})
