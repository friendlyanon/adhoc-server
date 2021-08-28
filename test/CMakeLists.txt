# Parent project does not export its library target, so this CML implicitly
# depends on being added from it, i.e. the testing is done only from the build
# tree and is not feasible from an install location

project(adhoc-serverTests LANGUAGES C)

add_executable(adhoc-server_test source/adhoc-server_test.c)
target_link_libraries(adhoc-server_test PRIVATE adhoc-server_lib)
target_compile_features(adhoc-server_test PRIVATE c_std_11)

add_test(NAME adhoc-server_test COMMAND adhoc-server_test)
