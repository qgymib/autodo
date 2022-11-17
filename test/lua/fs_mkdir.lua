local test_dir = os.getenv("CMAKE_CURRENT_BINARY_DIR") .. "/fs_mkdir/d1/d2"

auto.fs_mkdir(test_dir, true)
assert(auto.fs_isdir(test_dir) == true)
assert(auto.fs_isfile(test_dir) == false)

auto.fs_delete(test_dir)
assert(auto.fs_isdir(test_dir) == false)
assert(auto.fs_isfile(test_dir) == false)
