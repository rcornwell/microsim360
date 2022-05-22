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


add_executable(cros2065 cros2065.c)
target_include_directories(cros2065 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                ${CMAKE_CURRENT_SOURCE_DIR}/../device )
add_custom_target(model2065_ros.h
       COMMAND cros2065 ${CMAKE_CURRENT_SOURCE_DIR}/ros.txt model2065_ros.h
       COMMENT "Building model 2065 ROS data"
       DEPENDS cros2065 ${CMAKE_CURRENT_SOURCE_DIR}/ros.txt)

set(model2065_src cpu2065.c model2065_ros.c)
target_sources(${PROJECT_NAME} PUBLIC ${model2065_src} )
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/images)
add_dependencies(${PROJECT_NAME} model2065_ros.h)

if (RUN_TESTS)
add_executable(inst2065_test test/ctest_main.c test/model2065_test.c
           ../logger.c ../device/disassem.c ../device/xlat.c ${model2065_src})
target_link_libraries(inst2050_test m)
add_dependencies(inst2065_test model2065_ros.h)
add_custom_command(TARGET inst2065_test
            COMMAND inst2065_test
            COMMENT "Test instructions for 2065"
            VERBATIM)
target_include_directories(inst2065_test PRIVATE
                                ${CMAKE_CURRENT_SOURCE_DIR}
                                ${CMAKE_CURRENT_BINARY_DIR}
                                ${CMAKE_CURRENT_SOURCE_DIR}/..
                                ${CMAKE_CURRENT_BINARY_DIR}/..
                                ${CMAKE_CURRENT_SOURCE_DIR}/../test
                                ${CMAKE_CURRENT_SOURCE_DIR}/../device )
add_test(NAME inst2065_test COMMAND inst2065_test )
endif()

