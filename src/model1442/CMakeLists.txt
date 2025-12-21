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


add_library(model1442lib)
target_sources(model1442lib PRIVATE model1442.c)
target_include_directories(model1442lib PRIVATE ${includes})
target_link_libraries(${PROJECT_NAME} PUBLIC model1442lib)
target_sources(${PROJECT_NAME} PUBLIC panel1442.c)

if (RUN_TESTS)
add_executable(model1442_test ../test/ctest_main.c ../test/test_chan.c
               test/model1442_test.c)
target_link_libraries(model1442_test model1442lib)
target_link_libraries(model1442_test devicelib)
target_link_libraries(model1442_test toplib)
if (WIN32)
set_property(TARGET model1442_test APPEND_STRING PROPERTY LINK_FLAGS " /INCREMENTAL:NO")
endif()
add_custom_command(TARGET model1442_test
            COMMAND model1442_test
            COMMENT "Test operation of 1442"
            VERBATIM)
target_include_directories(model1442_test PRIVATE ${includes}
                                                 ${CMAKE_CURRENT_SOURCE_DIR}
                                                 ${CMAKE_CURRENT_SOURCE_DIR}/test
                                                 ${CMAKE_CURRENT_SOURCE_DIR}/../test
                                                 ${CMAKE_CURRENT_SOURCE_DIR}/../device)
add_test(NAME model1442_test COMMAND model1442_test )
endif()

