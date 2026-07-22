local lag_optimizer_state = {
	was_enabled = nil,
	saved = {},
}

local forced_settings = {
	{ option = "lag_optimizer.no_inventory_animations", target = "inventory_items_animations", value = false },
	{ option = "lag_optimizer.no_view_bobbing", target = "nobob", value = true },
	{ option = "lag_optimizer.fast_chunks", target = "smooth_lighting", value = false },
	{ option = "lag_optimizer.fast_chunks", target = "performance_tradeoffs", value = true },
	{ option = "lag_optimizer.no_water_animation", target = "enable_waving_water", value = false },
	{ option = "lag_optimizer.low_fx", target = "enable_post_processing", value = false },
	{ option = "lag_optimizer.low_fx", target = "enable_bloom", value = false },
	{ option = "lag_optimizer.low_fx", target = "enable_volumetric_lighting", value = false },
	{ option = "lag_optimizer.low_fx", target = "enable_auto_exposure", value = false },
	{ option = "lag_optimizer.no_minimap", target = "enable_minimap", value = false },
}

local function save_target(setting)
	if lag_optimizer_state.saved[setting] == nil then
		lag_optimizer_state.saved[setting] = core.settings:get_bool(setting)
	end
end

local function restore_target(setting)
	local old_value = lag_optimizer_state.saved[setting]
	if old_value ~= nil then
		core.settings:set_bool(setting, old_value)
		lag_optimizer_state.saved[setting] = nil
	end
end

local function restore_all_targets()
	for setting, old_value in pairs(lag_optimizer_state.saved) do
		core.settings:set_bool(setting, old_value)
		lag_optimizer_state.saved[setting] = nil
	end
end

local function apply_forced_settings()
	for _, entry in ipairs(forced_settings) do
		save_target(entry.target)
		if core.settings:get_bool(entry.option) then
			if core.settings:get_bool(entry.target) ~= entry.value then
				core.settings:set_bool(entry.target, entry.value)
			end
		else
			restore_target(entry.target)
		end
	end
end

core.register_cheat_description("lagoptimizer", "Client", "lag_optimizer",
	"Reduce visual lag, particles, clouds, post effects, first-person motion, inventory animations, and item motion")
core.register_cheat_with_infotext("lagoptimizer", "Client", "lag_optimizer", "")
core.register_cheat_setting("No Inventory Animations", "Client", "lag_optimizer",
	"lag_optimizer.no_inventory_animations", {type="bool"})
core.register_cheat_setting("No Item Spin", "Client", "lag_optimizer", "lag_optimizer.no_item_spin", {type="bool"})
core.register_cheat_setting("No Hand Animation", "Client", "lag_optimizer",
	"lag_optimizer.no_hand_animation", {type="bool"})
core.register_cheat_setting("No Ground Items", "Client", "lag_optimizer", "lag_optimizer.no_ground_items", {type="bool"})
core.register_cheat_setting("No View Bob", "Client", "lag_optimizer", "lag_optimizer.no_view_bobbing", {type="bool"})
core.register_cheat_setting("No Break Particles", "Client", "lag_optimizer", "lag_optimizer.no_break_particles", {type="bool"})
core.register_cheat_setting("Fast Chunks", "Client", "lag_optimizer",
	"lag_optimizer.fast_chunks", {type="bool"})
core.register_cheat_setting("No Plant Animation", "Client", "lag_optimizer",
	"lag_optimizer.no_plant_animation", {type="bool"})
core.register_cheat_setting("No Water Animation", "Client", "lag_optimizer", "lag_optimizer.no_water_animation", {type="bool"})
core.register_cheat_setting("No Lava Animation", "Client", "lag_optimizer", "lag_optimizer.no_lava_animation", {type="bool"})
core.register_cheat_setting("Low FX", "Client", "lag_optimizer", "lag_optimizer.low_fx", {type="bool"})
core.register_cheat_setting("No Minimap", "Client", "lag_optimizer", "lag_optimizer.no_minimap", {type="bool"})
core.register_cheat_setting("No Achievement Overlay", "Client", "lag_optimizer",
	"lag_optimizer.no_achievement_overlay", {type="bool"})
core.register_cheat_setting("No Bossbar", "Client", "lag_optimizer", "lag_optimizer.no_bossbar", {type="bool"})
core.register_cheat_setting("Suppress Tiles", "Client", "lag_optimizer",
	"lag_optimizer.suppress_tiles", {type="bool"})

core.register_globalstep(function(dtime)
	local ok, err = pcall(function()
		local enabled = core.settings:get_bool("lag_optimizer")

		if enabled ~= lag_optimizer_state.was_enabled then
			lag_optimizer_state.was_enabled = enabled
			if enabled then
				apply_forced_settings()
				core.update_infotext("lagoptimizer", "Client", "lag_optimizer", "")
			else
				restore_all_targets()
				core.update_infotext("lagoptimizer", "Client", "lag_optimizer", "")
			end
		elseif enabled then
			apply_forced_settings()
		end
	end)

	if not ok then
		core.log("error", "LagOptimizer error: " .. tostring(err))
		core.settings:set_bool("lag_optimizer", false)
		restore_all_targets()
		lag_optimizer_state.was_enabled = nil
	end
end)
