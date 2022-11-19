local json = auto.json()

-- Path match
if true then
    local src = "\\a/mixed//path\\\\for\\test"
    local dst = { "a", "mixed", "path", "for", "test" }

    local str_list = auto.string_split(src, "[/\\\\]")
    assert(json:compare(json:encode(str_list), json:encode(dst)) == true)
end

-- Line wrap match
if true then
    local src = "macos\rlinux\nwindows\r\nlast_line\n"
    local dst = { "macos", "linux", "windows", "last_line" }

    local l = auto.string_split(src, "\\r|\\n")
    assert(json:compare(json:encode(dst), json:encode(l)) == true)
end

-- Multi characters
if true then
    local src = "hello world to everyone, hello world to the earth."
    local dst = { " to everyone, ", " to the earth." }

    local l = auto.string_split(src, "hello world")
    assert(json:compare(json:encode(dst), json:encode(l)) == true)
end
