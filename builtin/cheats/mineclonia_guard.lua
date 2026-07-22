local mineclonia_only = {
	{setting = "autotoxic", name = "autotoxic", category = "Misc"},
	{setting = "crystalspam", name = "crystalspam", category = "Combat"},
	{setting = "autowither", name = "autowither", category = "Combat"},
	{setting = "autototem", name = "autototem", category = "Combat"},
	{setting = "autoarmor", name = "autoarmor", category = "Combat"},
	{setting = "critical_hits", name = "criticals", category = "Combat"},
	{setting = "elytra_takeoff", name = "elytratakeoff", category = "Movement"},
	{setting = "autocraft", name = "autocraft", category = "Bots"},
	{setting = "autocraft_menu", name = "autocraft_menu", category = "Bots"},
	{setting = "autotnt", name = "autotnt", category = "World"},
	{setting = "content_previewer", name = "contentpreviewer", category = "Render"},
	{setting = "no_fire", name = "nofire", category = "Visuals"},
	{setting = "totems", name = "totems", category = "Client"},
}

local warned = {}
local timer = 0

core.register_globalstep(function(dtime)
	timer = timer + dtime
	if timer < 0.25 then
		return
	end
	timer = 0

	local compatible = core.is_mineclonia_server()
	if compatible == nil then
		return
	end

	for _, feature in ipairs(mineclonia_only) do
		if core.settings:get_bool(feature.setting) then
			if compatible then
				warned[feature.setting] = nil
			else
				core.settings:set_bool(feature.setting, false)
				core.update_infotext(feature.name, feature.category,
					feature.setting, "Mineclonia only")
				if not warned[feature.setting] then
					core.display_chat_message(feature.name .. " is Mineclonia only.")
					warned[feature.setting] = true
				end
			end
		else
			warned[feature.setting] = nil
		end
	end
end)
