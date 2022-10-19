local json = auto.json()

local a = { "a", "s", "d", "f", { a = "hello", b = "world" } }

io.write(json:encode(a))

local str = "{ \"key\": null }"

assert(json:decode(str).key == json.null)

io.write("success\n")
