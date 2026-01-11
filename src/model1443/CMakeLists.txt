# Copyright 2021, Richard Cornwell
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

add_library(model1443lib)
target_sources(model1443lib PRIVATE model1443.c)
target_include_directories(model1443lib PRIVATE ${includes})
target_link_libraries(${PROJECT_NAME} PUBLIC model1443lib)
target_sources(${PROJECT_NAME} PUBLIC panel1443.c)

if (RUN_TESTS)
add_executable(model1443_test ../test/ctest_main.c ../test/test_chan.c
               test/model1443_test.c)
target_link_libraries(model1443_test model1443lib)
target_link_libraries(model1443_test devicelib)
target_link_libraries(model1443_test toplib)
if (WIN32)
set_property(TARGET model1443_test APPEND_STRING PROPERTY LINK_FLAGS " /INCREMENTAL:NO")
endif()
add_custom_command(TARGET model1443_test
            COMMAND model1443_test
            COMMENT "Test operation of 1443"
            VERBATIM)
target_include_directories(model1443_test PRIVATE ${includes}
                                                 ${CMAKE_CURRENT_SOURCE_DIR}
                                                 ${CMAKE_CURRENT_SOURCE_DIR}/test
                                                 ${CMAKE_CURRENT_SOURCE_DIR}/../test
                                                 ${CMAKE_CURRENT_SOURCE_DIR}/../device)
add_test(NAME model1443_test COMMAND model1443_test )
endif()
