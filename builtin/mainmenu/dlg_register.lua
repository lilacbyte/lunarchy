-- Luanti
-- SPDX-License-Identifier: LGPL-2.1-or-later

local function generate_password(length)
	local chars = "abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ23456789"
	local result = {}
	for i = 1, length do
		local idx = math.random(#chars)
		result[i] = chars:sub(idx, idx)
	end
	return table.concat(result)
end

local function register_formspec(dialogdata)
	local title = fgettext("Joining $1", dialogdata.server and dialogdata.server.name or dialogdata.address)
	local buttons_y = 6.65
	if dialogdata.error then
		buttons_y = buttons_y + 0.8
	end

	local retval = {
		"formspec_version[4]",
		"size[8,", tostring(buttons_y + 1.175), "]",
		"set_focus[", (dialogdata.name ~= "" and "password" or "name"), "]",
		"label[0.375,0.8;", title, "]",
		"field[0.375,1.575;7.25,0.8;name;", core.formspec_escape(fgettext("Name")), ";",
				core.formspec_escape(dialogdata.name), "]",
		"pwdfield[0.375,2.875;7.25,0.8;password;", core.formspec_escape(fgettext("Password")), "]",
		"pwdfield[0.375,4.175;7.25,0.8;password_2;", core.formspec_escape(fgettext("Confirm Password")), "]",
		"checkbox[0.375,5.45;random_password;", core.formspec_escape(fgettext("Random password")), ";",
			dump(dialogdata.random_password == true), "]",
		"checkbox[0.375,5.95;save_to_account_manager;", core.formspec_escape(fgettext("Save to account manager")), ";",
			dump(dialogdata.save_to_account_manager == true), "]"
	}

	if dialogdata.error then
		table.insert_all(retval, {
			"box[0.375,", tostring(buttons_y - 0.9), ";7.25,0.6;darkred]",
			"label[0.625,", tostring(buttons_y - 0.6), ";", core.formspec_escape(dialogdata.error), "]",
		})
	end

	table.insert_all(retval, {
		"container[0.375,", tostring(buttons_y), "]",
		"button[0,0;2.5,0.8;dlg_register_confirm;", fgettext("Register"), "]",
		"button[4.75,0;2.5,0.8;dlg_register_cancel;", fgettext("Cancel"), "]",
		"container_end[]",
	})

	return table.concat(retval, "")
end

--------------------------------------------------------------------------------
local function register_buttonhandler(this, fields)
	this.data.name = fields.name
	this.data.error = nil
	if fields.random_password then
		this.data.random_password = fields.random_password == "true"
	end
	if fields.save_to_account_manager then
		this.data.save_to_account_manager = fields.save_to_account_manager == "true"
	end

	if fields.dlg_register_confirm or fields.key_enter then
		if fields.name == "" then
			this.data.error = fgettext("Missing name")
			return true
		end

		local random_password = fields.random_password == "true" or this.data.random_password == true
		local password = fields.password or ""
		local confirm = fields.password_2 or ""

		if random_password then
			password = generate_password(96)
		elseif password == "" then
			this.data.error = fgettext("Password is required")
			return true
		elseif confirm == "" then
			this.data.error = fgettext("Confirm your password")
			return true
		end

		if not random_password and password ~= confirm then
			this.data.error = fgettext("Passwords do not match")
			return true
		end

		gamedata.playername = fields.name
		gamedata.password   = password
		gamedata.address    = this.data.address
		gamedata.port       = this.data.port
		gamedata.allow_login_or_register = "register"
		gamedata.selected_world = 0

		assert(gamedata.address and gamedata.port)

		local server = this.data.server
		if server then
			serverlistmgr.add_favorite(server)
			gamedata.servername        = server.name
			gamedata.serverdescription = server.description
		else
			gamedata.servername        = ""
			gamedata.serverdescription = ""

			serverlistmgr.add_favorite({
				address = gamedata.address,
				port = gamedata.port,
			})
		end

		core.settings:set("name", fields.name)
		core.settings:set("address",     gamedata.address)
		core.settings:set("remote_port", gamedata.port)

		if fields.save_to_account_manager == "true" or this.data.save_to_account_manager == true then
			core.settings:set("account_manager.pending_register_save", "true")
		else
			core.settings:set("account_manager.pending_register_save", "false")
		end

		core.start()
	end

	if fields["dlg_register_cancel"] then
		this:delete()
		return true
	end

	return false
end

--------------------------------------------------------------------------------
function create_register_dialog(address, port, server)
	assert(address)
	assert(type(port) == "number")

	local retval = dialog_create("dlg_register",
			register_formspec,
			register_buttonhandler,
			nil)
	retval.data.address = address
	retval.data.port = port
	retval.data.server = server
	retval.data.name = core.settings:get("name") or ""
	retval.data.random_password = false
	retval.data.save_to_account_manager = false
	return retval
end
