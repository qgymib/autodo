local src = "E:\\path\\to\\foo/bar"
local dst = "E:/path/to/foo/bar"

assert(auto.fs_format(src, false) == dst)
