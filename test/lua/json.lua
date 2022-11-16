local json = auto.json()

local a = { "a", "b", { a = "hello", b = "world" }, { "test", "string" } }

local a2 = json:decode(json:encode(a))
assert(json:compare(json:encode(a), json:encode(a2)))
