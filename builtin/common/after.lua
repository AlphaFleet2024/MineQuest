local jobs = {}
local time = 0.0
local time_next = math.huge

minetest.register_globalstep(function(dtime)
	time = time + dtime

	if time < time_next then
		return
	end

	time_next = math.huge

	-- Iterate backwards so that we miss any new timers added by
	-- a timer callback.
	for i = #jobs, 1, -1 do
		local job = jobs[i]
		if time >= job.expire then
			minetest.set_last_run_mod(job.mod_origin)
			job.func(unpack(job.arg))
			local jobs_l = #jobs
			jobs[i] = jobs[jobs_l]
			jobs[jobs_l] = nil
		elseif job.expire < time_next then
			time_next = job.expire
		end
	end
end)

function minetest.after(after, func, ...)
	assert(tonumber(after) and type(func) == "function",
		"Invalid minetest.after invocation")
	local expire = time + after
	jobs[#jobs + 1] = {
		func = func,
		expire = expire,
		arg = {...},
		mod_origin = minetest.get_last_run_mod()
	}
	time_next = math.min(time_next, expire)
end
