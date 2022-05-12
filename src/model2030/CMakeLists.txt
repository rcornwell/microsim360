# copyright 2021, richard cornwell
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
add_executable(cros2030 cros2030.c)
target_include_directories(cros2030 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                ${CMAKE_CURRENT_SOURCE_DIR}/../device )
add_custom_target(model2030_ros.h
       COMMAND cros2030 ${CMAKE_CURRENT_SOURCE_DIR}/ccros2030.txt model2030_ros.h
       COMMENT "Building model 2030 ROS data"
       DEPENDS cros2030 ${CMAKE_CURRENT_SOURCE_DIR}/ccros2030.txt)

set(model2030_src cpu2030.c model1050.c model2030_ros.c)
target_sources(${PROJECT_NAME} PUBLIC ${model2030_src} panel2030.c)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
add_dependencies(${PROJECT_NAME} model2030_ros.h)

if (RUN_TESTS)
add_executable(inst2030_test test/ctest_main.c test/model2030_test.c
           ../logger.c ../device/disassem.c ../device/xlat.c ${model2030_src})
target_link_libraries(inst2030_test m)
add_dependencies(inst2030_test model2030_ros.h)
add_custom_command(TARGET inst2030_test
            COMMAND inst2030_test
            COMMENT "Test instructions for 2030"
            VERBATIM)
target_include_directories(inst2030_test PRIVATE
                                ${SDL2_INCLUDE_DIRS}
                                ${CMAKE_CURRENT_SOURCE_DIR}
                                ${CMAKE_CURRENT_BINARY_DIR}
                                ${CMAKE_CURRENT_SOURCE_DIR}/..
                                ${CMAKE_CURRENT_SOURCE_DIR}/../device )
add_test(NAME inst2030_test COMMAND inst2030_test )
endif()

