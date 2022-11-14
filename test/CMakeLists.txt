set(test_list
    coroutine
    sqlite)

foreach(arg IN LISTS test_list)
    add_test(NAME ${arg}
         COMMAND $<TARGET_FILE:autodo> ${CMAKE_CURRENT_SOURCE_DIR}/lua/${arg}.lua)
    set_property(TEST ${arg} PROPERTY
        ENVIRONMENT "PROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}")
endforeach()
