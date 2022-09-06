add_test(NAME hello_world
    COMMAND $<TARGET_FILE:autodo> ${CMAKE_CURRENT_SOURCE_DIR}/hello_world.lua)
