--Minetest
--Copyright (C) 2023 rubenwardy
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


update_detector = {}


if not core.get_http_api then
	update_detector.get_all = function() return {} end
	update_detector.get_count = function() return 0 end
	return
end


local has_fetched = false
local latest_releases


-- encodes for use as URL parameter or path component
local function urlencode(str)
	return str:gsub("[^%a%d()._~-]", function(char)
		return ("%%%02X"):format(char:byte())
	end)
end
assert(urlencode("sample text?") == "sample%20text%3F")


local function fetch_latest_releases(param)
	local version = core.get_version()
	local base_url = core.settings:get("contentdb_url")
	local url = base_url ..
			"/api/updates/?type=mod&type=game&type=txp&protocol_version=" ..
			core.get_max_supp_proto() .. "&engine_version=" .. param.urlencode(version.string)
	local http = core.get_http_api()
	local response = http.fetch_sync({ url = url })
	if not response.succeeded then
		return
	end

	return core.parse_json(response.data)
end


--- Get a table from package key (author/name) to latest release id
---
--- @param callback function that takes a single argument, table or nil
local function get_latest_releases(callback)
	core.handle_async(
			fetch_latest_releases,
			{ urlencode = urlencode },
			callback)
end


local function has_packages_from_cdb()
	pkgmgr.refresh_globals()
	pkgmgr.update_gamelist()

	for _, content in pairs(pkgmgr.get_all()) do
		if content.author and content.release > 0 then
			return true
		end
	end
	return false
end


--- @returns a new table with all keys lowercase
local function lowercase_keys(tab)
	local ret = {}
	for key, value in pairs(tab) do
		ret[key:lower()] = value
	end
	return ret
end


local function fetch()
	if has_fetched or not has_packages_from_cdb() then
		return
	end

	has_fetched = true

	get_latest_releases(function(releases)
		if not releases then
			has_fetched = false
			return
		end

		latest_releases = lowercase_keys(releases)
		if update_detector.get_count() > 0 then
			local maintab = ui.find_by_name("maintab")
			if not maintab.hidden then
				ui.update()
			end
		end
	end)
end


--- @returns a list of content with an update available
function update_detector.get_all()
	if latest_releases == nil then
		fetch()
		return {}
	end

	pkgmgr.refresh_globals()
	pkgmgr.update_gamelist()

	local ret = {}
	local all_content = pkgmgr.get_all()
	for i = 1, #all_content do
		local content = all_content[i]
		if content.author and content.release > 0 then
			-- The backend will account for aliases in `latest_releases`
			local id = content.author:lower() .. "/"
			if content.type == "game" then
				id = id .. content.id
			else
				id = id .. content.name
			end

			local latest_release = latest_releases[id]
			if latest_release and latest_release > content.release then
				ret[#ret + 1] = content
			end
		end
	end

	return ret
end


--- @return number of packages with updates available
function update_detector.get_count()
	return #update_detector.get_all()
end
