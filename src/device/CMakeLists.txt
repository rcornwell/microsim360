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

add_library(devicelib STATIC card.c disassem.c device.c tape.c xlat.c dasd.c cpu.c)
target_include_directories(devicelib PRIVATE ${includes})
#                                            ${SDL2_INCLUDE_DIRS}
#                                            ${SDL2_IMAGE_INCLUDE_DIRS}
#                                            ${SDL2_TTF_INCLUDE_DIRS})

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
#target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(${PROJECT_NAME} PUBLIC devicelib)

if (RUN_TESTS)
add_executable(device_test)
if (WIN32)
set_property(TARGET device_test APPEND_STRING PROPERTY LINK_FLAGS " /INCREMENTAL:NO")
endif()
target_sources(device_test PUBLIC ../test/ctest_main.c test/device_test.c test/card_test.c test/tape_test.c)
target_link_libraries(device_test PUBLIC devicelib)
target_link_libraries(device_test PUBLIC toplib)
target_include_directories(device_test PUBLIC ${includes})
add_custom_command(TARGET device_test
            COMMAND device_test
            COMMENT "Test device emulators"
            VERBATIM)
add_test(NAME device_test COMMAND device_test )
endif()


