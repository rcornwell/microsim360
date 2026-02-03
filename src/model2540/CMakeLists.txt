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


add_library(model2540lib)
target_sources(model2540lib PRIVATE model2540.c model2540r.c model2540p.c)
include_directories(${CMAKE_CURRENT_LIST_DIR})
target_include_directories(model2540lib PRIVATE ${includes})
target_link_libraries(${PROJECT_NAME} PUBLIC model2540lib)
target_sources(${PROJECT_NAME} PUBLIC panel2540.c)

if (RUN_TESTS)
add_executable(model2540_test ../test/ctest_main.c ../test/test_chan.c
               test/model2540_test.c)
target_link_libraries(model2540_test model2821lib)
target_link_libraries(model2540_test model2540lib)
target_link_libraries(model2540_test devicelib)
target_link_libraries(model2540_test toplib)
if (WIN32)
set_property(TARGET model2540_test APPEND_STRING PROPERTY LINK_FLAGS " /INCREMENTAL:NO")
endif()
#add_custom_command(TARGET model2540_test
#            COMMAND model2540_test -v -d
#            COMMENT "Test operation of 2540"
#            VERBATIM)
target_include_directories(model2540_test PRIVATE ${includes}
                                                 ${CMAKE_CURRENT_SOURCE_DIR}
                                                 ${CMAKE_CURRENT_SOURCE_DIR}/test
                                                 ${CMAKE_CURRENT_SOURCE_DIR}/../test
                                                 ${CMAKE_CURRENT_SOURCE_DIR}/../device)
#add_test(NAME model2540_test COMMAND model2540_test )
endif()

