#
# Tests configuration
#

include_directories($<TARGET_PROPERTY:meta,INTERFACE_INCLUDE_DIRECTORIES>)
add_compile_options($<TARGET_PROPERTY:meta,INTERFACE_COMPILE_OPTIONS>)

add_executable(meta_test odr.cpp meta.cpp)
set_target_properties(meta_test PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(meta_test PRIVATE meta GTest::Main Threads::Threads)
target_compile_definitions(meta_test PRIVATE $<TARGET_PROPERTY:meta,INTERFACE_COMPILE_DEFINITIONS>)
target_compile_features(meta_test PRIVATE $<TARGET_PROPERTY:meta,INTERFACE_COMPILE_FEATURES>)
target_compile_options(meta_test PRIVATE $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-pedantic -Wall>)
target_compile_options(meta_test PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/EHsc>)
add_test(NAME meta_test COMMAND meta_test)
