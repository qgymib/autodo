local pattern = auto.regex("number (\\d+)")
assert(pattern ~= nil)

local data = "This is a test string with number 99"
local match_list = pattern:match(data)
assert(match_list ~= nil)

io.write(auto.json():encode(match_list))
