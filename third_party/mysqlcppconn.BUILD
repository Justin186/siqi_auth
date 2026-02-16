# MySQL Connector/C++ system library wrapper
# Wraps the system-installed libmysqlcppconn at /usr

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "mysqlcppconn",
    hdrs = glob([
        "include/mysql_driver.h",
        "include/mysql_connection.h",
        "include/mysql_error.h",
        "include/cppconn/*.h",
    ]),
    includes = ["include"],
    linkopts = ["-lmysqlcppconn"],
)

