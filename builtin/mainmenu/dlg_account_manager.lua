local function account_list_text()
	local accounts = account_manager.get_accounts()
	local names = {}
	if #accounts == 0 then
		return fgettext("No accounts saved")
	end
	for i, account in ipairs(accounts) do
		names[i] = account.username
	end
	return table.concat(names, ",")
end

local function get_formspec(dialogdata)
	local selected = tonumber(dialogdata.selected_index) or account_manager.get_selected_index() or 1
	local account = account_manager.get_accounts()[selected]

	if dialogdata.username == nil then
		dialogdata.username = account and account.username or ""
	end
	if dialogdata.password == nil then
		dialogdata.password = ""
	end
	dialogdata.selected_index = selected
	local mapped_server = account and account.server or ""
	if mapped_server == "" then
		mapped_server = fgettext("No server mapped")
	end

	return table.concat({
		"formspec_version[8]",
		"size[11.4,9.1]",
		"bgcolor[;neither;]",
		"box[0,0;11.4,9.1;#1f2328]",
		"style_type[*;border=false;textcolor=white;font_size=*1.25;font=bold]",
		"label[0.55,1.12;" .. fgettext("Account Manager") .. "]",
		"style_type[image_button;border=false;textcolor=white;font_size=*1.7;padding=0;font=bold;" ..
			"bgimg=" .. core.formspec_escape(defaulttexturedir .. "menu_button.png") .. ";" ..
			"bgimg_hovered=" .. core.formspec_escape(defaulttexturedir .. "menu_button_hovered.png") .. "]",
		"textlist[0.45,1.7;4.35,6.35;accounts;" .. account_list_text() .. ";" .. selected .. "]",
		"label[5.3,1.7;" .. fgettext("Username") .. ":]",
		"field[5.3,2.05;5.45,0.8;username;;" .. core.formspec_escape(dialogdata.username or "") .. "]",
		"label[5.3,3.0;" .. fgettext("Server") .. ": " .. core.formspec_escape(mapped_server) .. "]",
		"label[5.3,3.55;" .. fgettext("Password") .. ":]",
		"pwdfield[5.3,3.9;5.45,0.75;password;]",
		"image_button[5.3,4.8;5.45,0.75;;save;" .. fgettext("Add / Update") .. "]",
		"image_button[5.3,5.7;5.45,0.75;;set_default;" .. fgettext("Set Default") .. "]",
		"image_button[5.3,6.6;5.45,0.75;;remove;" .. fgettext("Remove") .. "]",
		"image_button[5.3,7.5;5.45,0.75;;back;" .. fgettext("Back") .. "]"
	}, "\n")
end

local function buttonhandler(this, fields)
	if fields.accounts then
		local event = core.explode_textlist_event(fields.accounts)
		if event.type == "CHG" or event.type == "DCL" then
			this.data.selected_index = event.index
			local account = account_manager.get_accounts()[event.index]
			if account then
				this.data.username = account.username
				this.data.password = ""
			end
			return true
		end
	end

	if fields.username then
		this.data.username = fields.username
	end
	if fields.password then
		this.data.password = fields.password
	end

	if fields.save then
		local ok, err = account_manager.upsert(fields.username or "", fields.password or "")
		if not ok then
			gamedata.errormessage = err or fgettext("Unable to save account.")
		else
			this.data.selected_index = account_manager.get_selected_index()
			this.data.password = ""
		end
		return true
	end

	if fields.set_default then
		local index = tonumber(this.data.selected_index) or account_manager.get_selected_index()
		if account_manager.select_index(index) then
			local account = account_manager.get_selected_account()
			if account then
				this.data.username = account.username
				this.data.password = ""
			end
		end
		return true
	end

	if fields.remove then
		account_manager.remove_selected()
		this.data.selected_index = account_manager.get_selected_index()
		local account = account_manager.get_selected_account()
		if account then
			this.data.username = account.username
			this.data.password = ""
		else
			this.data.username = ""
			this.data.password = ""
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
		local current = ui.find_by_name("dlg_account_manager")
		current:delete()
		if mainmenu then
			mainmenu:show()
		end
		ui.update()
		return true
	end
	return false
end

function create_account_manager_dlg()
	local dlg = dialog_create("dlg_account_manager", get_formspec, buttonhandler, eventhandler)
	return dlg
end
