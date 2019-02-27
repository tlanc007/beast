#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

add_definitions (-DBOOST_BEAST_ALLOW_DEPRECATED)
add_definitions (-DBOOST_BEAST_TESTS)

#hacky
link_directories(/usr/local/boost_1_69_0/lib)
link_libraries(boost_coroutine boost_filesystem fmt)

add_subdirectory (core)
add_subdirectory (experimental)
add_subdirectory (http)
add_subdirectory (websocket)
#add_subdirectory (zlib)

#-------------------------------------------------------------------------------

GroupSources (include/boost/beast beast)
GroupSources (test/extras/include/boost/beast extras)
GroupSources (test/beast "/")

add_executable (tests-beast
    ${BOOST_BEAST_FILES}
    ${EXTRAS_FILES}
    ${TEST_MAIN}
    Jamfile
    core.cpp
    http.cpp
    version.cpp
    websocket.cpp
    zlib.cpp
)

target_link_directories(tests-beast PUBLIC /usr/local/boost_1_69_0/lib)
target_link_libraries(tests-beast boost_coroutine)
set_property(TARGET tests-beast PROPERTY FOLDER "tests")
