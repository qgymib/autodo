local s_dir = "C:\\public\\test/dir"
local s_file = "file"
local s_path = s_dir .. "/" .. s_file

local dir,file = auto.fs_splitpath(s_path)
assert(dir == s_dir)
assert(file == s_file)

assert(auto.fs_basename(s_path) == s_file)
assert(auto.fs_dirname(s_path) == s_dir)
