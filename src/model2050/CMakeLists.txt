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
                                ${CMAKE_CURRENT_SOURCE_DIR}/..
                                ${CMAKE_CURRENT_SOURCE_DIR}/../device )
add_custom_target(model2050_ros.h
       COMMAND cros2050 ${CMAKE_CURRENT_SOURCE_DIR}/ros.txt model2050_ros.h
       COMMENT "Building model 2050 ROS data"
       DEPENDS cros2050 ${CMAKE_CURRENT_SOURCE_DIR}/ros.txt)

set(model2050_src cpu2050.c model2050_ros.c)
target_sources(${PROJECT_NAME} PUBLIC ${model2050_src} panel2050.c)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/images)
add_dependencies(${PROJECT_NAME} model2050_ros.h)

if (RUN_TESTS)
add_executable(inst2050_test test/ctest_main.c test/model2050_test.c
           ../logger.c ../conf.c ../device/disassem.c ../device/xlat.c
           ../device/device.c ../test/io_test.c ../test/test_device.c ${model2050_src})
if (UNIX)
target_link_libraries(inst2050_test m)
endif()
add_dependencies(inst2050_test model2050_ros.h)
add_custom_command(TARGET inst2050_test
            COMMAND inst2050_test
            COMMENT "Test instructions for 2050"
            VERBATIM)
target_include_directories(inst2050_test PRIVATE
                                ${CMAKE_CURRENT_SOURCE_DIR}
                                ${CMAKE_CURRENT_SOURCE_DIR}/test
                                ${CMAKE_CURRENT_BINARY_DIR}
                                ${CMAKE_CURRENT_SOURCE_DIR}/..
                                ${CMAKE_CURRENT_BINARY_DIR}/..
                                ${CMAKE_CURRENT_SOURCE_DIR}/../test
                                ${CMAKE_CURRENT_SOURCE_DIR}/../device )
add_test(NAME inst2050_test COMMAND inst2050_test )
endif()

