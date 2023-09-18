--Minetest
--Copyright (C) 2014 sapier
--Copyright (C) 2018 rubenwardy <rw@rubenwardy.com>
--
--This program is free software; you can redistribute it and/or modify
--it under the terms of the GNU Lesser General Public License as published by
--the Free Software Foundation; either version 2.1 of the License, or
--(at your option) any later version.
--
--This program is distributed in the hope that it will be useful,
--but WITHOUT ANY WARRANTY; without even the implied warranty of
--MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--GNU Lesser General Public License for more details.
--
--You should have received a copy of the GNU Lesser General Public License along
--with this program; if not, write to the Free Software Foundation, Inc.,
--51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

local packages_raw, packages

local function on_change(type)
	if type ~= "ENTER" then
		return
	end

	if not pkgmgr.global_mods then
		pkgmgr.refresh_globals()
	end
	if not pkgmgr.games then
		pkgmgr.update_gamelist()
	end

	packages_raw = {}
	table.insert_all(packages_raw, pkgmgr.games)
	table.insert_all(packages_raw, pkgmgr.get_texture_packs())
	table.insert_all(packages_raw, pkgmgr.global_mods:get_list())

	local function get_data()
		return packages_raw
	end

	local function is_equal(element, uid) --uid match
		return (element.type == "game" and element.id == uid) or
				element.name == uid
	end

	packages = filterlist.create(get_data, pkgmgr.compare_package,
			is_equal, nil, {})
end

local function get_formspec(tabview, name, tabdata)
	if not tabdata.selected_pkg then
		tabdata.selected_pkg = 1
	end

	local use_technical_names = core.settings:get_bool("show_technical_names")

	local retval = {
		"label[0.4,0.4;", fgettext("Installed Packages:"), "]",
		"tablecolumns[color;tree;text]",
		"table[0.4,0.8;6.3,4.8;pkglist;",
		pkgmgr.render_packagelist(packages, use_technical_names),
		";", tabdata.selected_pkg, "]",

		"button[0.4,5.8;6.3,0.9;btn_contentdb;", fgettext("Browse online content"), "]"
	}

	local selected_pkg
	if filterlist.size(packages) >= tabdata.selected_pkg then
		selected_pkg = packages:get_list()[tabdata.selected_pkg]
	end

	if selected_pkg then
		-- Check for screenshot being available
		local screenshotfilename = selected_pkg.path .. DIR_DELIM .. "screenshot.png"
		local screenshotfile, error = io.open(screenshotfilename, "r")

		local modscreenshot
		if not error then
			screenshotfile:close()
			modscreenshot = screenshotfilename
		else
			modscreenshot = defaulttexturedir .. "no_screenshot.png"
		end

		local info = core.get_content_info(selected_pkg.path)
		local desc = fgettext("No package description available")
		if info.description and info.description:trim() ~= "" then
			desc = core.formspec_escape(info.description)
		end

		local title_and_name
		if selected_pkg.type == "game" then
			title_and_name = selected_pkg.name
		else
			title_and_name = (selected_pkg.title or selected_pkg.name) .. "\n" ..
				core.colorize("#BFBFBF", selected_pkg.name)
		end

		table.insert_all(retval, {
			"image[7.1,0.2;3,2;", core.formspec_escape(modscreenshot), "]",
			"label[10.5,1;", core.formspec_escape(title_and_name), "]",
			"box[7.1,2.4;8,3.1;#000]"
		})

		if selected_pkg.is_modpack then
			table.insert_all(retval, {
				"button[11.1,5.8;4,0.9;btn_mod_mgr_rename_modpack;",
				fgettext("Rename"), "]"
			})
		elseif selected_pkg.type == "mod" then
			-- Show dependencies for mods
			desc = desc .. "\n\n"
			local toadd_hard = table.concat(info.depends or {}, "\n")
			local toadd_soft = table.concat(info.optional_depends or {}, "\n")
			if toadd_hard == "" and toadd_soft == "" then
				desc = desc .. fgettext("No dependencies.")
			else
				if toadd_hard ~= "" then
					desc = desc ..fgettext("Dependencies:") ..
						"\n" .. toadd_hard
				end
				if toadd_soft ~= "" then
					if toadd_hard ~= "" then
						desc = desc .. "\n\n"
					end
					desc = desc .. fgettext("Optional dependencies:") ..
						"\n" .. toadd_soft
				end
			end
		elseif selected_pkg.type == "txp" then
			if selected_pkg.enabled then
				table.insert_all(retval, {
					"button[11.1,5.8;4,0.9;btn_mod_mgr_disable_txp;",
					fgettext("Disable Texture Pack"), "]"
				})
			else
				table.insert_all(retval, {
					"button[11.1,5.8;4,0.9;btn_mod_mgr_use_txp;",
					fgettext("Use Texture Pack"), "]"
				})
			end
		end

		table.insert_all(retval, {"textarea[7.1,2.4;8,3.1;;;", desc, "]"})

		if core.may_modify_path(selected_pkg.path) then
			table.insert_all(retval, {
				"button[7.1,5.8;4,0.9;btn_mod_mgr_delete_mod;",
				fgettext("Uninstall Package"), "]"
			})
		end
	end

	return table.concat(retval)
end

local function handle_doubleclick(pkg)
	if pkg.type == "txp" then
		if core.settings:get("texture_path") == pkg.path then
			core.settings:set("texture_path", "")
		else
			core.settings:set("texture_path", pkg.path)
		end
		packages = nil

		mm_game_theme.init()
		mm_game_theme.reset()
	end
end

local function handle_buttons(tabview, fields, tabname, tabdata)

	if fields.pkglist then
		local event = core.explode_table_event(fields.pkglist)
		tabdata.selected_pkg = event.row
		if event.type == "DCL" then
			handle_doubleclick(packages:get_list()[tabdata.selected_pkg])
		end
		return true
	end

	if fields.btn_contentdb then
		local dlg = create_store_dlg()
		dlg:set_parent(tabview)
		tabview:hide()
		dlg:show()
		packages = nil
		return true
	end

	if fields.btn_mod_mgr_rename_modpack then
		local mod = packages:get_list()[tabdata.selected_pkg]
		local dlg_renamemp = create_rename_modpack_dlg(mod)
		dlg_renamemp:set_parent(tabview)
		tabview:hide()
		dlg_renamemp:show()
		packages = nil
		return true
	end

	if fields.btn_mod_mgr_delete_mod then
		local mod = packages:get_list()[tabdata.selected_pkg]
		local dlg_delmod = create_delete_content_dlg(mod)
		dlg_delmod:set_parent(tabview)
		tabview:hide()
		dlg_delmod:show()
		packages = nil
		return true
	end

	if fields.btn_mod_mgr_use_txp or fields.btn_mod_mgr_disable_txp then
		local txp_path = ""
		if fields.btn_mod_mgr_use_txp then
			txp_path = packages:get_list()[tabdata.selected_pkg].path
		end

		core.settings:set("texture_path", txp_path)
		packages = nil

		mm_game_theme.init()
		mm_game_theme.reset()
		return true
	end

	return false
end

return {
	name = "content",
	caption = fgettext("Content"),
	cbf_formspec = get_formspec,
	cbf_button_handler = handle_buttons,
	on_change = on_change
}
