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


# Program to convert ROS text into C data.
set(CROS2844 ${CMAKE_CURRENT_SOURCE_DIR}/model2844_ros.txt)
add_executable(cros2844 cros2844.c)
target_include_directories(cros2844 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                ${CMAKE_CURRENT_SOURCE_DIR}/../device )
add_custom_target(model2844_ros.h
       COMMAND cros2844 ${CROS2844} model2844_ros.h
       COMMENT "Building model 2844 ROS data"
       DEPENDS cros2844 ${CROS2844})

target_sources(${PROJECT_NAME} PUBLIC model2844.c model2844_ros.c)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
add_dependencies(${PROJECT_NAME} model2844_ros.h)

if (RUN_TESTS)
add_executable(test2844_test test/ctest_main.c test/disk_test.c ../event.c
               ../logger.c ../conf.c ../device/dasd.c ../device/xlat.c
               ../device/device.c model2844.c model2844_ros.c)
target_link_libraries(test2844_test)
add_dependencies(test2844_test model2844_ros.h)
add_custom_command(TARGET test2844_test
            COMMAND test2844_test
            COMMENT "Test instructions for 2844"
            VERBATIM)
target_include_directories(test2844_test PRIVATE
                                ${SDL2_INCLUDE_DIRS}
                                ${CMAKE_CURRENT_SOURCE_DIR}
                                ${CMAKE_CURRENT_BINARY_DIR}
                                ${CMAKE_CURRENT_SOURCE_DIR}/..
                                ${CMAKE_CURRENT_BINARY_DIR}/..
                                ${CMAKE_CURRENT_SOURCE_DIR}/../test
                                ${CMAKE_CURRENT_SOURCE_DIR}/../device )
add_test(NAME test2844_test COMMAND test2844_test )
endif()

