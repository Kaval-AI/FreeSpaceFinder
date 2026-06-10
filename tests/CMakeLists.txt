set(TESTS
    tst_filenode
    tst_analyzer
    tst_filemodel
    tst_scanner
)

foreach(test ${TESTS})
    qt_add_executable(${test} ${test}.cpp)
    target_link_libraries(${test} PRIVATE FreeSpaceFinderCore Qt6::Test)
    add_test(NAME ${test} COMMAND ${test})
endforeach()
