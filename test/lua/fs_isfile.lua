local path = os.getenv("PROJECT_SOURCE_DIR") .. "/test/lua"
local cnt = 0

for _, p in auto.fs_listdir(path) do
    io.write(p .. "\n")
    assert(auto.fs_isfile(p))
    cnt = cnt + 1
end

assert(cnt >= 1)
