local function trim(value)
	return (tostring(value or ""):gsub("^%s+", ""):gsub("%s+$", ""))
end

local function plain_chat(message)
	local plain = core.strip_colors and core.strip_colors(message) or message
	plain = plain:gsub("\27%(T@[^)]*%)", "")
	plain = plain:gsub("\27[TEF]", "")
	return trim(plain)
end

-- These are the direct-killer forms emitted by Mineclonia's
-- mcl_death_messages mod. Assist-only messages are intentionally omitted.
local killer_patterns = {
	"^([^ ]+) was killed by ([^ ]+) using ",
	"^([^ ]+) was roasted in dragon breath by ([^ ]+)",
	"^([^ ]+) was shot by a skull from ([^ ]+)",
	"^([^ ]+) was slain by ([^ ]+)",
	"^([^ ]+) was shot by ([^ ]+)",
	"^([^ ]+) was spitballed by ([^ ]+)",
	"^([^ ]+) was fireballed by ([^ ]+)",
	"^([^ ]+) was killed trying to hurt ([^ ]+)",
	"^([^ ]+) tried to hurt ([^ ]+) and died by ",
	"^([^ ]+) was blown up by ([^ ]+)",
	"^([^ ]+) went off with a bang due to a firework fired by ([^ ]+) from ",
	"^([^ ]+) was frozen to death by ([^ ]+)",
	"^([^ ]+) was impaled by ([^ ]+)",
}

local function mineclonia_kill_by(message, local_name)
	-- Mineclonia public chat is formatted as <player> message. Never treat
	-- player-written text or direct messages as a server death message.
	if message:match("^<[^<>]+>%s*") or message:match("^DM%s+") then
		return nil
	end

	for _, pattern in ipairs(killer_patterns) do
		local victim, killer = message:match(pattern)
		if victim and killer and killer:lower() == local_name:lower() and
				victim:lower() ~= local_name:lower() then
			return victim
		end
	end
	return nil
end

local function format_message(template, victim)
	local formatted = template:gsub("{player}", function()
		return victim
	end)
	return formatted
end

local function is_connected_player(name)
	for _, player_name in ipairs(core.get_player_names() or {}) do
		if player_name:lower() == name:lower() then
			return true
		end
	end
	return false
end

core.register_cheat_description("autotoxic", "Misc", "autotoxic",
	"Send a configured message after Mineclonia reports that you killed a player")
core.register_cheat_with_infotext("autotoxic", "Misc", "autotoxic", "")
core.register_cheat_setting("message", "Misc", "autotoxic", "autotoxic.message",
	{type="text", size=40})

core.register_on_receiving_chat_message(function(message)
	if not core.settings:get_bool("autotoxic") then
		return false
	end
	if core.is_mineclonia_server() ~= true then
		return false
	end

	local player = core.localplayer
	local local_name = player and player:get_name() or ""
	if local_name == "" then
		return false
	end

	local victim = mineclonia_kill_by(plain_chat(message), local_name)
	if not victim or not is_connected_player(victim) then
		return false
	end

	local reply = trim(core.settings:get("autotoxic.message"))
	if reply == "" then
		reply = "ez"
	end
	core.send_chat_message(format_message(reply, victim))
	return false
end)
