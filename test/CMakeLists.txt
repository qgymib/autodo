set(test_list
    coroutine)

foreach(arg IN LISTS test_list)
    add_test(NAME ${arg}
         COMMAND $<TARGET_FILE:autodo> ${CMAKE_CURRENT_SOURCE_DIR}/lua/${arg}.lua)
endforeach()
