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

#add_executable(bin2c device/bin2c.c)
#add_custom_target(font1.h
#       COMMAND bin2c base_font ${FONT_SOURCE_DIR}/SourceCodePro-Black.ttf font1.h
#       COMMENT "Building font object"
#       DEPENDS bin2c ${FONT_SOURCE_DIR}/SourceCodePro-Black.ttf)

set(includes ${CMAKE_CURRENT_BINARY_DIR}
             ${CMAKE_CURRENT_SOURCE_DIR}
             ${CMAKE_CURRENT_SOURCE_DIR}/image
             ${CMAKE_CURRENT_SOURCE_DIR}/panel
             ${CMAKE_CURRENT_SOURCE_DIR}/device)

if (RUN_TESTS)
add_subdirectory(test)
add_executable(sim_test test/ctest_main.c test/sim_test.c test/event_test.c)
endif()

add_subdirectory(device)
add_subdirectory(panel)
add_subdirectory(model1052)
add_subdirectory(model1442)
add_subdirectory(model1443)
add_subdirectory(model2415)
add_subdirectory(model2844)
add_subdirectory(model2030)
add_subdirectory(model2050)
#add_subdirectory(model2065)
#add_subdirectory(model2075)
#add_subdirectory(model2841)
add_library(toplib conf.c logger.c event.c layout.c)
target_include_directories(toplib PUBLIC ${includes})
target_include_directories(${PROJECT_NAME} PUBLIC ${includes})
target_sources(${PROJECT_NAME} PUBLIC main.c)
target_link_libraries(${PROJECT_NAME} PUBLIC toplib)
target_link_libraries(${PROJECT_NAME} PUBLIC devicelib)

#add_dependencies(${PROJECT_NAME} font1.h)
if (WIN32)
set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY LINK_FLAGS " /INCREMENTAL:NO")
else()
#target_compile_options(${PROJECT_NAME} PUBLIC -fsanitize=address -fsanitize-recover=address)
endif()

if (RUN_TESTS)

if (WIN32)
set_property(TARGET sim_test APPEND_STRING PROPERTY LINK_FLAGS " /INCREMENTAL:NO")
endif()
target_link_libraries(sim_test PUBLIC toplib)
add_custom_command(TARGET sim_test
            COMMAND sim_test
            COMMENT "Test simulator function"
            VERBATIM)

target_include_directories(sim_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                            ${CMAKE_CURRENT_SOURCE_DIR}/device
                                            ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME sim_test COMMAND sim_test )
endif()



