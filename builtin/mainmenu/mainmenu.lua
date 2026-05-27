-- Luanti - Main Menu Dialog
-- Copyright (C) 2024 siliconsniffer
-- SPDX-License-Identifier: LGPL-2.1-or-later

-- Define defaulttexturedir and dump if they aren't globally accessible.
-- For example:
-- local defaulttexturedir = core.get_current_modpath() .. "/textures/"
-- local dump = minetest.debug.dump -- Or your custom dump function

local function main_menu_formspec(this)
	if this.hidden or (this.parent ~= nil and this.parent.hidden) then
		return ""
	end
	local formspec_elements = {
        "formspec_version[8]",
        "size[11.4,9.1]",
        "bgcolor[;neither;]",
        "style_type[*;border=false;textcolor=white;font_size=*1.25;font=bold]",
        "style_type[image_button;border=false;textcolor=white;font_size=*1.7;padding=0;font=bold;bgimg=" .. core.formspec_escape(defaulttexturedir .. "menu_button.png") .. ";bgimg_hovered=" .. core.formspec_escape(defaulttexturedir .. "menu_button_hovered.png") .. "]",
        "image_button[2.1,1.95;7.2,0.78;;local_btn;" .. fgettext("Local") .. "]",
        "image_button[2.1,2.9;7.2,0.78;;online;" .. fgettext("Online") .. "]",
        "image_button[2.1,3.85;3.5,0.78;;accounts;" .. fgettext("Accounts") .. "]",
        "image_button[5.8,3.85;3.5,0.78;;profiles;" .. fgettext("Profiles") .. "]",
        "image_button[2.1,4.8;7.2,0.78;;csm;" .. fgettext("CSMs") .. "]",
        "image_button[2.1,5.75;7.2,0.78;;content;" .. fgettext("Content") .. "]",
        "image_button[2.1,6.7;7.2,0.78;;about;" .. fgettext("About") .. "]",
        "image_button[2.1,7.65;3.5,0.78;;settings;" .. fgettext("Settings") .. "]",
        "image_button[5.8,7.65;3.5,0.78;;quit;" .. fgettext("Quit") .. "]",
    }
	return table.concat(formspec_elements, "")
end

local function main_menu_buttonhandler(this, fields)
	if this.hidden then
		return false
	end
    if fields.local_btn then
        local dlg = create_local_dlg()
		dlg:set_parent(this)
		this:hide()
		dlg:show()
		ui.update()
        return true
    elseif fields.online then
        local dlg = create_online_dlg()
		dlg:set_parent(this)
		this:hide()
		dlg:show()
		ui.update()
        return true
    elseif fields.accounts then
        local dlg = create_account_manager_dlg()
		dlg:set_parent(this)
		this:hide()
		dlg:show()
		ui.update()
        return true
    elseif fields.csm then
        local dlg = create_csm_dlg()
		dlg:set_parent(this)
		this:hide()
		dlg:show()
		ui.update()
        return true
    elseif fields.content then
        local dlg = create_content_dlg()
		dlg:set_parent(this)
		this:hide()
		dlg:show()
		ui.update()
        return true
    elseif fields.profiles then
        local dlg = create_profiles_dlg()
		dlg:set_parent(this)
		this:hide()
		dlg:show()
		ui.update()
        return true
    elseif fields.settings then
        local dlg = create_settings_dlg()
		dlg:set_parent(this)
		this:hide()
		dlg:show()
		ui.update()
        return true
    elseif fields.about then
        local dlg = create_about_dlg()
		dlg:set_parent(this)
		this:hide()
		dlg:show()
		ui.update()
        return true
	elseif fields.quit then
		core.close()
		return true
	elseif fields.try_quit then
		return true
    end
	
    core.log("error", "Main Menu: "..dump(fields))
    return false
end

local function main_menu_eventhandler(this, event)
	if this.hidden then
		return false
	end

	if event == "MenuQuit" then
		core.close()
		return true
	end
end

local function hide_menu(this)
	this.hidden=true
end

--------------------------------------------------------------------------------
local function show_menu(this)
	mm_game_theme.set_engine()
	this.hidden=false
end

local tabview_metatable = {
	handle_buttons            = main_menu_buttonhandler,
	handle_events             = main_menu_eventhandler,
	get_formspec              = main_menu_formspec,
	show                      = show_menu,
	hide                      = hide_menu,
	delete                    = function(self) ui.delete(self) end,
	set_parent                = function(self,parent) self.parent = parent end,
	set_fixed_size			  = function(self,state) self.fixed_size = state end,
}

function create_main_menu(name)--, size, tabheaderpos)
	local self = {}

	self.name     = name
	self.type     = "toplevel"

	setmetatable(self, { __index = tabview_metatable })

	self.fixed_size     = true
	self.hidden         = true

	ui.add(self)
	return self
end
