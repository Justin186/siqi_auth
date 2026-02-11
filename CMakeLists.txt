cmake_minimum_required(VERSION 3.10)
project(siqi_permission_system)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 查找依赖
find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(BRPC REQUIRED brpc)

# Manual find for MySQL Connector/C++ (legacy)
find_path(MYSQL_INCLUDE_DIR mysql_driver.h)
find_library(MYSQL_LIBRARY mysqlcppconn)

if (NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_LIBRARY)
    message(FATAL_ERROR "MySQL Connector/C++ (libmysqlcppconn) not found")
endif()

# 生成protobuf代码
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS proto/auth.proto)

# 可执行文件：权限服务器
add_executable(auth_server 
    src/server_main.cpp
    src/auth_service_impl.cpp
    src/permission_dao.cpp
    ${PROTO_SRCS}
)

target_include_directories(auth_server PRIVATE
    ${PROTOBUF_INCLUDE_DIR}
    ${BRPC_INCLUDE_DIRS}
    ${MYSQL_INCLUDE_DIR}
    include
)

target_link_libraries(auth_server
    ${PROTOBUF_LIBRARY}
    ${BRPC_LIBRARIES}
    ${MYSQL_LIBRARY}
    pthread
    dl
    z
    ssl
    crypto
    leveldb
    gflags
)

# 可执行文件：测试客户端
add_executable(test_client
    src/client_example.cpp
    ${PROTO_SRCS}
)

target_include_directories(test_client PRIVATE
    ${PROTOBUF_INCLUDE_DIR}
    ${BRPC_INCLUDE_DIRS}
    include
)

target_link_libraries(test_client
    ${PROTOBUF_LIBRARY}
    ${BRPC_LIBRARIES}
    pthread
    dl
    z
    ssl
    crypto
    leveldb
    gflags
)