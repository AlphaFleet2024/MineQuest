--[[
Register function to easily register new builtin hud elements
Contains:
  elem_def  the HUD element definion which can be changed with hud_replace_builtin
  event     (optional) for an additional eventname on which update_element is called
            ("hud_changed" and "properties_changed" will always be used.)
  hud_change(id, player)
            (optional) a function to change the element after it has been updated
  show_elem(player, flags, id)
            (optional) a function to decide if the element should be shown to a player
]]--
local registered_elements = {}
local update_events = {}
local function register_builtin_hud_element(name, def)
	registered_elements[name] = def
	if def.event then
		local events = update_events[def.event]
		if events then
			table.insert(events, name)
		else
			update_events[def.event] = {name}
		end
	end
end

-- Stores HUD ids for all players
local hud_ids = {}

-- Updates one element
-- In case the element is already added, it does only call the hud_change function from
-- registered_elements. (To completely update the element call player:hud_remove(id) first.)
local function update_element(player, player_hud_ids, elem_name, flags)
	local def = registered_elements[elem_name]
	local id = player_hud_ids[elem_name]

	if def.show_elem and not def.show_elem(player, flags, id) then
		if id then
			player:hud_remove(id)
		end
		return
	end

	if not id then
		id = player:hud_add(def.elem_def)
		player_hud_ids[elem_name] = id
	end

	if def.hud_change then
		def.hud_change(player, id)
	end
end

-- Updates all elements
local function update_hud(player)
	local flags = player:hud_get_flags()
	local playername = player:get_player_name()
	local player_hud_ids = hud_ids[playername]
	if not player_hud_ids then
		player_hud_ids = {}
		hud_ids[playername] = player_hud_ids
	end
	for elem_name, _ in pairs(registered_elements) do
		update_element(player, player_hud_ids, elem_name, flags)
	end
end

local function player_event_handler(player, eventname)
	assert(player:is_player())

	if eventname == "hud_changed" or eventname == "properties_changed" then
		update_hud(player)
		return true
	end

	-- Custom events
	local to_update = update_events[eventname]
	if to_update then
		local flags = player:hud_get_flags()
		local playername = player:get_player_name()
		local player_hud_ids = hud_ids[playername]
		if not player_hud_ids then
			player_hud_ids = {}
			hud_ids[playername] = player_hud_ids
		end
		for _, elem_name in ipairs(to_update) do
			update_element(player, player_hud_ids, elem_name, flags)
		end
		return true
	end

	return false
end

-- Returns true if successful, otherwise false,
-- but currently the return value is not part of the lua API.
function core.hud_replace_builtin(elem_name, definition)
	if type(definition) ~= "table" or
			(definition.type or definition.hud_elem_type) ~= "statbar" then
		return false
	end

	local registered = registered_elements[elem_name]
	if not registered then
		return false
	end

	registered.elem_def = table.copy(definition)

	for playername, player_hud_ids in pairs(hud_ids) do
		local player = core.get_player_by_name(playername)
		local id = player_hud_ids[elem_name]
		if player and id then
			player:hud_remove(id)
			update_element(player, player_hud_ids, elem_name, player:hud_get_flags())
		end
	end
	return true
end

local function cleanup_builtin_hud(player)
	local name = player:get_player_name()
	if name == "" then
		return
	end
	hud_ids[name] = nil
end


-- Append "update_hud" as late as possible
-- This ensures that the HUD is hidden when the flags are updated in this callback
core.register_on_mods_loaded(function()
	core.register_on_joinplayer(update_hud)
end)
core.register_on_leaveplayer(cleanup_builtin_hud)
core.register_playerevent(player_event_handler)


---- Builtin HUD Elements

--- Healthbar

-- Cache setting
local enable_damage = core.settings:get_bool("enable_damage")

local function scaleToHudMax(player, field)
	-- Scale "hp" or "breath" to the hud maximum dimensions
	local current = player["get_" .. field](player)
	local nominal
	if field == "hp" then -- HUD is called health but field is hp
		nominal = registered_elements.health.elem_def.item
	else
		nominal = registered_elements[field].elem_def.item
	end
	local max_display = math.max(player:get_properties()[field .. "_max"], current)
	return math.ceil(current / max_display * nominal)
end

register_builtin_hud_element("health", {
	elem_def = {
		type = "statbar",
		position = {x = 0.5, y = 1},
		text = "heart.png",
		text2 = "heart_gone.png",
		number = core.PLAYER_MAX_HP_DEFAULT,
		item = core.PLAYER_MAX_HP_DEFAULT,
		direction = 0,
		size = {x = 24, y = 24},
		offset = {x = (-10 * 24) - 25, y = -(48 + 24 + 16)},
	},
	show_elem = function(player, flags)
		return flags.healthbar and enable_damage and player:get_armor_groups().immortal ~= 1
	end,
	event = "health_changed",
	hud_change = function(player, id)
		player:hud_change(id, "number", scaleToHudMax(player, "hp"))
	end,
})

--- Breathbar

register_builtin_hud_element("breath", {
	elem_def = {
		type = "statbar",
		position = {x = 0.5, y = 1},
		text = "bubble.png",
		text2 = "bubble_gone.png",
		number = core.PLAYER_MAX_BREATH_DEFAULT * 2,
		item = core.PLAYER_MAX_BREATH_DEFAULT * 2,
		direction = 0,
		size = {x = 24, y = 24},
		offset = {x = 25, y= -(48 + 24 + 16)},
	},
	show_elem = function(player, flags, id)
		local breath     = player:get_breath()
		local breath_max = player:get_properties().breath_max
		local show_breathbar = flags.breathbar and enable_damage and player:get_armor_groups().immortal ~= 1

		if id then
			if breath == breath_max then
				local player_name = player:get_player_name()
				-- The breathbar stays for some time and then gets removed.
				core.after(1, function(player_name, id)
					local player = core.get_player_by_name(player_name)
					if player then
						player:hud_remove(id)
					end
				end, player_name, id)
				hud_ids[player_name].breath = nil
			end

			return show_breathbar
		else
			-- Only if the element does not already exist the breath amount is considered.
			return show_breathbar and player:get_breath() < player:get_properties().breath_max
		end
	end,
	event = "breath_changed",
	hud_change = function(player, id)
		player:hud_change(id, "number", scaleToHudMax(player, "breath"))
	end,
})
