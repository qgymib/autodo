local json = auto.json()

local str = "\\a/mixed//path\\\\for\\test"
local res = { "a", "mixed", "path", "for", "test" }

local str_list = auto.string_split(str, "[/\\\\]")
assert(json:compare(json:encode(str_list), json:encode(res)) == true)
