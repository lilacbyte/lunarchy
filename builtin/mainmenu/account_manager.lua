-- Luanti
-- SPDX-License-Identifier: LGPL-2.1-or-later

account_manager = {}
login_username = login_username or ""

local account_dir = core.get_user_path() .. DIR_DELIM .. "client"
local account_file = account_dir .. DIR_DELIM .. "accounts.txt"
local selected_name_setting = "account_manager.selected_account"
local legacy_selected_name_setting = "account_manager.selected_username"
local login_username_setting = "login_username"
local login_password_setting = "login_password"

local accounts = {}
local selected_index = 0

local function trim(text)
	return (text or ""):gsub("^%s+", ""):gsub("%s+$", "")
end

local function ensure_storage()
	if core.mkdir then
		core.mkdir(account_dir)
	end
end

local function account_key(account)
	return (account.username or "") .. "|" .. (account.server or "")
end

local function find_account_index_for_server(username, server)
	for index, account in ipairs(accounts) do
		if account.username == username and (account.server or "") == (server or "") then
			return index
		end
	end
	return nil
end

local function find_account_index(username)
	return find_account_index_for_server(username, "")
end

local function apply_selected_account()
	local account = accounts[selected_index]
	if not account then
		return
	end

	core.settings:set(selected_name_setting, account_key(account))
	core.settings:set(legacy_selected_name_setting, account.username)
	core.settings:set("name", account.username)
	cache_settings:set(login_username_setting, account.username)
	cache_settings:set(login_password_setting, account.password)
	login_username = account.username
end

function account_manager.load()
	accounts = {}
	selected_index = 0

	ensure_storage()
	local f = io.open(account_file, "r")
	if f then
		for line in f:lines() do
			line = trim(line)
			if line ~= "" and not line:match("^#") then
				local username, password, server = line:match("^(.-)|([^|]*)|(.*)$")
				if not username then
					username, password = line:match("^(.-)|([^|]*)$")
					server = ""
				end
				if username and password then
					username = trim(username)
					password = trim(password)
					server = trim(server or "")
					if username ~= "" then
						table.insert(accounts, {
							username = username,
							password = password,
							server = server,
						})
					end
				end
			end
		end
		f:close()
	end

	local selected_name = trim(core.settings:get(selected_name_setting) or "")
	if selected_name == "" then
		selected_name = trim(core.settings:get(legacy_selected_name_setting) or "")
	end
	if selected_name ~= "" then
		local selected_username, selected_server = selected_name:match("^(.-)|(.*)$")
		if selected_username then
			selected_index = find_account_index_for_server(trim(selected_username), trim(selected_server)) or 0
		else
			selected_index = find_account_index(selected_name) or 0
		end
	end
	if selected_index == 0 and #accounts > 0 then
		selected_index = 1
	end

	apply_selected_account()
end

function account_manager.save()
	ensure_storage()
	local f = assert(io.open(account_file, "w"))
	for _, account in ipairs(accounts) do
		f:write(account.username, "|", account.password or "", "|", account.server or "", "\n")
	end
	f:close()
end

function account_manager.get_accounts()
	return accounts
end

function account_manager.get_selected_index()
	return selected_index
end

function account_manager.get_selected_account()
	return accounts[selected_index]
end

function account_manager.get_login_defaults()
	local account = account_manager.get_selected_account()
	if account then
		return account.username, account.password
	end

	local cached_username = cache_settings:get(login_username_setting) or core.settings:get("name") or ""
	local cached_password = cache_settings:get(login_password_setting) or ""
	return cached_username, cached_password
end

function account_manager.select_index(index)
	if not index or index < 1 or index > #accounts then
		selected_index = 0
		core.settings:remove(selected_name_setting)
		core.settings:remove(legacy_selected_name_setting)
		return false
	end

	selected_index = index
	apply_selected_account()
	return true
end

function account_manager.select_username(username)
	local index = find_account_index(username)
	if not index then
		return false
	end
	return account_manager.select_index(index)
end

function account_manager.upsert(username, password, server)
	username = trim(username)
	password = trim(password)
	server = trim(server)
	if username == "" then
		return false, "Username is required."
	end

	local account_index = find_account_index_for_server(username, server)
	if account_index then
		accounts[account_index].password = password
		accounts[account_index].server = server
		selected_index = account_index
	else
		table.insert(accounts, {
			username = username,
			password = password,
			server = server,
		})
		selected_index = #accounts
	end

	account_manager.save()
	apply_selected_account()
	return true
end

function account_manager.remove_selected()
	if selected_index < 1 or selected_index > #accounts then
		return false
	end

	table.remove(accounts, selected_index)
	if #accounts == 0 then
		selected_index = 0
		core.settings:remove(selected_name_setting)
		core.settings:remove(legacy_selected_name_setting)
	else
		if selected_index > #accounts then
			selected_index = #accounts
		end
		apply_selected_account()
	end

	account_manager.save()
	return true
end

account_manager.load()
