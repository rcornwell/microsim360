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
set(CROS2841 ${CMAKE_CURRENT_SOURCE_DIR}/ros.txt)
add_executable(cros2841 cros2841.c)
target_include_directories(cros2841 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                ${CMAKE_CURRENT_SOURCE_DIR}/../device )
add_custom_target(model2841_ros.h
       COMMAND cros2841 ${CROS2841} model2841_ros.h
       COMMENT "Building model 2841 ROS data"
       DEPENDS cros2841 ${CROS2841})

add_library(model2841lib)
target_sources(model2841lib PRIVATE model2841.c model2841_ros.c)
target_include_directories(model2841lib PRIVATE ${includes})
target_include_directories(model2841lib PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
add_dependencies(model2841lib model2841_ros.h)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(${PROJECT_NAME} PUBLIC panel2841.c)
target_link_libraries(${PROJECT_NAME} PUBLIC model2841lib)

if (RUN_TESTS)
add_executable(test2841_test test/ctest_main.c test/disk_test.c)
target_link_libraries(test2841_test model2841lib)
target_link_libraries(test2841_test devicelib)
target_link_libraries(test2841_test toplib)
#add_dependencies(test2841_test model2841_ros.h)
if (WIN32)
set_property(TARGET test2841_test APPEND_STRING PROPERTY LINK_FLAGS " /INCREMENTAL:NO")
endif()
add_custom_command(TARGET test2841_test
            COMMAND test2841_test
            COMMENT "Test instructions for 2841"
            VERBATIM)
target_include_directories(test2841_test PRIVATE
                                ${includes}
                                ${CMAKE_CURRENT_SOURCE_DIR}
                                ${CMAKE_CURRENT_SOURCE_DIR}/../test)
add_test(NAME test2841_test COMMAND test2841_test )
endif()

