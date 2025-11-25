# Copyright 2022, Richard Cornwell
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


add_executable(cros2050 cros2050.c)
target_include_directories(cros2050 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                            ${includes})
add_custom_target(model2050_ros.h
       COMMAND cros2050 ${CMAKE_CURRENT_SOURCE_DIR}/ros.txt model2050_ros.h
       COMMENT "Building model 2050 ROS data"
       DEPENDS cros2050 ${CMAKE_CURRENT_SOURCE_DIR}/ros.txt)

add_library(model2050lib)
target_sources(model2050lib PRIVATE cpu2050.c model2050_ros.c)
target_include_directories(model2050lib PRIVATE ${includes})
target_include_directories(model2050lib PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
add_dependencies(model2050lib model2050_ros.h)
target_sources(${PROJECT_NAME} PUBLIC panel2050.c)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/images)
target_link_libraries(${PROJECT_NAME} PUBLIC model2050lib)

if (RUN_TESTS)
add_executable(inst2050_test ../test/ctest_main.c test/model2050_test.c
               ../test/io_test.c ../test/sel_test.c ../test/mul_io_test.c ../test/test_device.c)
if (WIN32)
set_property(TARGET inst2050_test APPEND_STRING PROPERTY LINK_FLAGS " /INCREMENTAL:NO")
endif()
target_link_libraries(inst2050_test model2050lib)
target_link_libraries(inst2050_test devicelib)
target_link_libraries(inst2050_test toplib)
if (UNIX)
target_link_libraries(inst2050_test m)
endif()
add_custom_command(TARGET inst2050_test
            COMMAND inst2050_test -v -d
            COMMENT "Test instructions for 2050"
            VERBATIM)
target_include_directories(inst2050_test PRIVATE ${includes}
                                                 ${CMAKE_CURRENT_BINARY_DIR}
                                                 ${CMAKE_CURRENT_SOURCE_DIR}
                                                 ${CMAKE_CURRENT_SOURCE_DIR}/test
                                                 ${CMAKE_CURRENT_SOURCE_DIR}/../test)
add_test(NAME inst2050_test COMMAND inst2050_test )
endif()

