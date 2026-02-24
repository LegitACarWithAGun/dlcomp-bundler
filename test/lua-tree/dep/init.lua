-- conformance with https://rfcs.luau.org/abstract-module-paths-and-init-dot-luau.html
local sub = require("./dep/sub")
return sub .. " | dep/"
