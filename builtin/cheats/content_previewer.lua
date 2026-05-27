core.register_cheat_description("contentpreviewer", "Render", "content_previewer",
	"Preview ender chests, shulker boxes, or maps in inventory tooltips")
core.register_cheat_with_infotext("contentpreviewer", "Render", "content_previewer", "")

core.register_globalstep(function()
	local enabled = core.settings:get_bool("content_previewer") or core.settings:get_bool("shulker_preview")
	local parts = {}
	if core.settings:get_bool("content_previewer.enderchest") then
		parts[#parts + 1] = "Enderchest"
	end
	if core.settings:get_bool("content_previewer.shulker") then
		parts[#parts + 1] = "Shulker"
	end
	if core.settings:get_bool("content_previewer.maps") then
		parts[#parts + 1] = "Maps"
	end
	local mode = table.concat(parts, ", ")

	if not enabled then
		core.update_infotext("contentpreviewer", "Render", "content_previewer", "")
	else
		core.update_infotext("contentpreviewer", "Render", "content_previewer", "")
	end
end)
