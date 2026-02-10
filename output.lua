-- DO NOT EDIT; this file has been automatically generated
local modules = {}

-- PATH: ./sample1.lua
-- ID: 1
modules[1] = (function()
	return "arbitrary string"
end)()

-- PATH: ./sample2.lua
-- ID: 0
modules[0] = (function()
	local one = modules[1]
	print(one .. " two!")
end)()
