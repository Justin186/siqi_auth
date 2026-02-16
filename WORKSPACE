workspace(name = "siqi_auth")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# ===========================================================================
# Starlark Libraries
# ===========================================================================

http_archive(
    name = "bazel_skylib",
    sha256 = "c6966ec828da198c5d9adbaa94c05e3a1c7f21bd012a0b29ba8ddbccb2c93b0d",
    urls = [
        "https://github.com/bazelbuild/bazel-skylib/releases/download/1.1.1/bazel-skylib-1.1.1.tar.gz",
        "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.1.1/bazel-skylib-1.1.1.tar.gz",
    ],
)

# ===========================================================================
# C++ Dependencies (required by brpc)
# ===========================================================================

# Protobuf 3.19.1 (with custom BUILD file for brpc compatibility)
http_archive(
    name = "com_google_protobuf",
    build_file = "//third_party:protobuf.BUILD",
    patch_cmds = [
        "sed -i protobuf.bzl -re '4,4d;417,508d'",
    ],
    sha256 = "87407cd28e7a9c95d9f61a098a53cf031109d451a7763e7dd1253abf8b4df422",
    strip_prefix = "protobuf-3.19.1",
    urls = ["https://github.com/protocolbuffers/protobuf/archive/refs/tags/v3.19.1.tar.gz"],
)

# gflags
http_archive(
    name = "com_github_gflags_gflags",
    sha256 = "34af2f15cf7367513b352bdcd2493ab14ce43692d2dcd9dfc499492966c64dcf",
    strip_prefix = "gflags-2.2.2",
    urls = ["https://github.com/gflags/gflags/archive/v2.2.2.tar.gz"],
)

# LevelDB
http_archive(
    name = "com_github_google_leveldb",
    build_file = "//third_party:leveldb.BUILD",
    strip_prefix = "leveldb-a53934a3ae1244679f812d998a4f16f2c7f309a6",
    url = "https://github.com/google/leveldb/archive/a53934a3ae1244679f812d998a4f16f2c7f309a6.tar.gz",
)

# Zlib
http_archive(
    name = "com_github_madler_zlib",
    build_file = "//third_party:zlib.BUILD",
    sha256 = "c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1",
    strip_prefix = "zlib-1.2.11",
    urls = [
        "https://downloads.sourceforge.net/project/libpng/zlib/1.2.11/zlib-1.2.11.tar.gz",
        "https://zlib.net/fossils/zlib-1.2.11.tar.gz",
    ],
)

# OpenSSL (system-installed)
new_local_repository(
    name = "openssl",
    build_file = "//third_party:openssl.BUILD",
    path = "/usr",
)

# ===========================================================================
# bRPC 1.16.0 (downloaded from GitHub)
# ===========================================================================

http_archive(
    name = "com_github_brpc_brpc",
    patch_cmds = [
        # Fix: brpc references protobuf internal subpackage path that doesn't exist
        # in our custom protobuf.BUILD. Replace with the equivalent target.
        "sed -i 's|@com_google_protobuf//src/google/protobuf/compiler:code_generator|@com_google_protobuf//:protoc_lib|g' BUILD.bazel",
    ],
    sha256 = "60f218554527f05ad8fae3cb8f81879d0c7dc72b249cde132049c44b1a73e76d",
    strip_prefix = "brpc-1.16.0",
    urls = ["https://github.com/apache/brpc/archive/refs/tags/1.16.0.tar.gz"],
)

# ===========================================================================
# MySQL Connector/C++ (system-installed)
# ===========================================================================

new_local_repository(
    name = "mysqlcppconn",
    build_file = "//third_party:mysqlcppconn.BUILD",
    path = "/usr",
)

