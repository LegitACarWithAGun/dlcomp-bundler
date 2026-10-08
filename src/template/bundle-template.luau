shared.dlcomp_bundler = shared.dlcomp_bundler or {}

local __BUNDLE_NAME = "[[NAME]]"
local __BUNDLE = {
	name = __BUNDLE_NAME,
	modules = {},

	_constructors = {},
}

function __BUNDLE.free_module(module: any)
	if module.destroy then
		module.destroy()
		return
	end

	if module.activator then
		module.activator:Destroy()
	end
	if module.maid then
		module.maid:Destroy()
	end
end
function __BUNDLE._free_module_list(list: { any }, bundle_name: string?)
	for i = #list, 1, -1 do
		local module = list[i]
		if module == nil then
			continue
		end
		local success, err = pcall(__BUNDLE.free_module, list[i])
		if not success then
			warn(`failed to free module (for: {bundle_name}; from: {__BUNDLE_NAME}) {i}:`, err)
		else
			list[i] = nil
		end
	end
end

function __BUNDLE.destroy()
	__BUNDLE._free_module_list(__BUNDLE.modules)
end

function __BUNDLE.require(i)
	local self = __BUNDLE
	if self.modules[i] then
		return self.modules[i]
	end

	local new = self._constructors[i]
	if not new then
		-- throw
		return
	end

	local loaded = new()
	self.modules[i] = loaded
	return loaded
end

-- clean up the previous version of the bundle
(function()
	local previous_instance = shared.dlcomp_bundler[__BUNDLE_NAME]
	if not previous_instance then
		return
	end

	if previous_instance.destroy then
		local success, err = pcall(previous_instance.destroy)
		if not success then
			warn(`failed to destroy old version of bundle {__BUNDLE_NAME}:`, err)
		end
		return
	end

	-- old bundle format, just an array of loaded modules
	if #previous_instance > 0 then
		__BUNDLE._free_module_list(previous_instance)
		return
	end
end)()
shared.dlcomp_bundler[__BUNDLE_NAME] = __BUNDLE

-- constructors for the modules
__BUNDLE._constructors = {
[[CONSTRUCTORS]],
}

-- start the bundle
__BUNDLE.require(1)
