-- Registered metatables, used by the C++ packer
local known_metatables = {}
function core.register_metatable(name, mt)
	assert(type(name) == "string", ("attempt to use %s value as metatable name"):format(type(name)))
	assert(type(mt) == "table", ("attempt to register a %s value as metatable"):format(type(mt)))
	assert(known_metatables[name] == nil or known_metatables[name] == mt,
			("attempt to override metatable %s"):format(name))
	known_metatables[name] = mt
	known_metatables[mt] = name
end
core.known_metatables = known_metatables

core.register_metatable("__builtin:vector", vector.metatable)
