# Copyright 2026, Richard Cornwell
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.


add_library(model2821lib)
target_sources(model2821lib PRIVATE model2821.c)
include_directories(${CMAKE_CURRENT_LIST_DIR})
target_include_directories(model2821lib PRIVATE ${includes})
target_link_libraries(${PROJECT_NAME} PUBLIC model2821lib)

if (RUN_TESTS)
add_executable(model2821_test ../test/ctest_main.c ../test/test_chan.c
               test/model2821_test.c)
target_link_libraries(model2821_test model2821lib)
target_link_libraries(model2821_test devicelib)
target_link_libraries(model2821_test toplib)
if (WIN32)
set_property(TARGET model2821_test APPEND_STRING PROPERTY LINK_FLAGS " /INCREMENTAL:NO")
endif()
add_custom_command(TARGET model2821_test
            COMMAND model2821_test
            COMMENT "Test operation of 2821"
            VERBATIM)
add_custom_target(model2821_test_debug model2821_test)
target_include_directories(model2821_test PRIVATE ${includes}
                                                 ${CMAKE_CURRENT_SOURCE_DIR}
                                                 ${CMAKE_CURRENT_SOURCE_DIR}/test
                                                 ${CMAKE_CURRENT_SOURCE_DIR}/../test
                                                 ${CMAKE_CURRENT_SOURCE_DIR}/../device)
add_test(NAME model2821_test COMMAND model2821_test )
endif()

