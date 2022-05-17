cmake_minimum_required(VERSION 3.5)

project(HPS)

set(CMAKE_C_FLAGS "-std=C99 -O0 -ggdb -gdwarf-2 -g3")
set(CMAKE_CXX_FLAGS "-std=c++11 -O0 -ggdb -gdwarf-2 -g3")

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

link_directories(
    ${PROJECT_SOURCE_DIR}/lib
)

set(LIB_SRC
    src/util.cc
    src/env.cc
    src/log.cc
    src/config.cc
    src/mutex.cc
    src/thread.cc
    src/fiber.cc
    src/scheduler.cc
    src/iomanager.cc
    src/timer.cc
    src/fd_manager.cc
    src/hook.cc
    src/address.cc
    src/socket.cc
    src/bytearray.cc
    src/http/http.cc
    src/http/http_session.cc
    src/http/http_parser.cc
    src/http/httpclient_parser.rl.cc
    src/http/http11_parser.rl.cc
    src/http/http_server.cc
    src/http/servlet.cc
    src/http/http_connection.cc
    src/tcp_server.cc
    src/stream.cc
    src/uri.rl.cc
    src/streams/socket_stream.cc
)

add_library(HPS SHARED ${LIB_SRC})

set(LIBS
    HPS
    dl
    yaml-cpp
    pthread
    ssl
)

add_executable(test test/test.cc)
target_link_libraries(test PUBLIC ${LIBS})

add_executable(test_log test/test_log.cc)
target_link_libraries(test_log PUBLIC ${LIBS})

add_executable(test_config test/test_config.cc)
target_link_libraries(test_config PUBLIC ${LIBS})

add_executable(test_thread test/test_thread.cc)
target_link_libraries(test_thread PUBLIC ${LIBS})

add_executable(test_fiber test/test_fiber.cc)
target_link_libraries(test_fiber PUBLIC ${LIBS})

add_executable(test_scheduler test/test_scheduler.cc)
target_link_libraries(test_scheduler PUBLIC ${LIBS})

add_executable(test_iomanager test/test_iomanager.cc)
target_link_libraries(test_iomanager PUBLIC ${LIBS})

add_executable(test_hook test/test_hook.cc)
target_link_libraries(test_hook PUBLIC ${LIBS})

add_executable(test_address test/test_address.cc)
target_link_libraries(test_address PUBLIC ${LIBS})

add_executable(test_socket test/test_socket.cc)
target_link_libraries(test_socket PUBLIC ${LIBS})

add_executable(test_bytearray test/test_bytearray.cc)
target_link_libraries(test_bytearray PUBLIC ${LIBS})

add_executable(test_http test/test_http.cc)
target_link_libraries(test_http PUBLIC ${LIBS})

add_executable(test_http_parser test/test_http_parser.cc)
target_link_libraries(test_http_parser PUBLIC ${LIBS})

add_executable(test_tcp_server test/test_tcp_server.cc)
target_link_libraries(test_tcp_server PUBLIC ${LIBS})

add_executable(test_http_server test/test_http_server.cc)
target_link_libraries(test_http_server PUBLIC ${LIBS})

add_executable(test_http_connection test/test_http_connection.cc)
target_link_libraries(test_http_connection PUBLIC ${LIBS})

add_executable(test_uri test/test_uri.cc)
target_link_libraries(test_uri PUBLIC ${LIBS})

add_executable(echo_server examples/echo_server.cc)
target_link_libraries(echo_server PUBLIC ${LIBS})

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)