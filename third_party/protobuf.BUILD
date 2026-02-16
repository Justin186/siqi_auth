# Protobuf BUILD file for brpc compatibility
# Adapted from brpc example/build_with_bazel/protobuf.BUILD
# Based on protobuf 3.19.1 source structure

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", native_cc_proto_library = "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_lang_toolchain", "proto_library")
load(":compiler_config_setting.bzl", "create_compiler_config_setting")
load(
    ":protobuf.bzl",
    "adapt_proto_library",
)

licenses(["notice"])

exports_files(["LICENSE"])

################################################################################
# ZLIB configuration
################################################################################

ZLIB_DEPS = ["@com_github_madler_zlib//:zlib"]

################################################################################
# Protobuf Runtime Library
################################################################################

MSVC_COPTS = [
    "/wd4018",
    "/wd4065",
    "/wd4146",
    "/wd4244",
    "/wd4251",
    "/wd4267",
    "/wd4305",
    "/wd4307",
    "/wd4309",
    "/wd4334",
    "/wd4355",
    "/wd4506",
    "/wd4800",
    "/wd4996",
]

COPTS = select({
    ":msvc": MSVC_COPTS,
    "//conditions:default": [
        "-DHAVE_ZLIB",
        "-Wmissing-field-initializers",
        "-Woverloaded-virtual",
        "-Wno-sign-compare",
    ],
})

create_compiler_config_setting(
    name = "msvc",
    value = "msvc-cl",
    visibility = [
        "//:__subpackages__",
    ],
)

LINK_OPTS = select({
    ":msvc": [
        "-ignore:4221",
    ],
    "//conditions:default": [
        "-lpthread",
        "-lm",
    ],
})

cc_library(
    name = "protobuf_lite",
    srcs = [
        "src/google/protobuf/any_lite.cc",
        "src/google/protobuf/arena.cc",
        "src/google/protobuf/arenastring.cc",
        "src/google/protobuf/extension_set.cc",
        "src/google/protobuf/generated_enum_util.cc",
        "src/google/protobuf/generated_message_table_driven_lite.cc",
        "src/google/protobuf/generated_message_tctable_lite.cc",
        "src/google/protobuf/generated_message_util.cc",
        "src/google/protobuf/implicit_weak_message.cc",
        "src/google/protobuf/inlined_string_field.cc",
        "src/google/protobuf/io/coded_stream.cc",
        "src/google/protobuf/io/io_win32.cc",
        "src/google/protobuf/io/strtod.cc",
        "src/google/protobuf/io/zero_copy_stream.cc",
        "src/google/protobuf/io/zero_copy_stream_impl.cc",
        "src/google/protobuf/io/zero_copy_stream_impl_lite.cc",
        "src/google/protobuf/map.cc",
        "src/google/protobuf/message_lite.cc",
        "src/google/protobuf/parse_context.cc",
        "src/google/protobuf/repeated_field.cc",
        "src/google/protobuf/repeated_ptr_field.cc",
        "src/google/protobuf/stubs/bytestream.cc",
        "src/google/protobuf/stubs/common.cc",
        "src/google/protobuf/stubs/int128.cc",
        "src/google/protobuf/stubs/status.cc",
        "src/google/protobuf/stubs/statusor.cc",
        "src/google/protobuf/stubs/stringpiece.cc",
        "src/google/protobuf/stubs/stringprintf.cc",
        "src/google/protobuf/stubs/structurally_valid.cc",
        "src/google/protobuf/stubs/strutil.cc",
        "src/google/protobuf/stubs/time.cc",
        "src/google/protobuf/wire_format_lite.cc",
    ],
    hdrs = glob([
        "src/google/protobuf/**/*.h",
        "src/google/protobuf/**/*.inc",
    ]),
    copts = COPTS,
    includes = ["src/"],
    linkopts = LINK_OPTS,
    visibility = ["//visibility:public"],
)

PROTOBUF_DEPS = select({
    ":msvc": [],
    "//conditions:default": ZLIB_DEPS,
})

cc_library(
    name = "protobuf",
    srcs = [
        "src/google/protobuf/any.cc",
        "src/google/protobuf/any.pb.cc",
        "src/google/protobuf/api.pb.cc",
        "src/google/protobuf/compiler/importer.cc",
        "src/google/protobuf/compiler/parser.cc",
        "src/google/protobuf/descriptor.cc",
        "src/google/protobuf/descriptor.pb.cc",
        "src/google/protobuf/descriptor_database.cc",
        "src/google/protobuf/duration.pb.cc",
        "src/google/protobuf/dynamic_message.cc",
        "src/google/protobuf/empty.pb.cc",
        "src/google/protobuf/extension_set_heavy.cc",
        "src/google/protobuf/field_mask.pb.cc",
        "src/google/protobuf/generated_message_bases.cc",
        "src/google/protobuf/generated_message_reflection.cc",
        "src/google/protobuf/generated_message_table_driven.cc",
        "src/google/protobuf/generated_message_tctable_full.cc",
        "src/google/protobuf/io/gzip_stream.cc",
        "src/google/protobuf/io/printer.cc",
        "src/google/protobuf/io/tokenizer.cc",
        "src/google/protobuf/map_field.cc",
        "src/google/protobuf/message.cc",
        "src/google/protobuf/reflection_ops.cc",
        "src/google/protobuf/service.cc",
        "src/google/protobuf/source_context.pb.cc",
        "src/google/protobuf/struct.pb.cc",
        "src/google/protobuf/stubs/substitute.cc",
        "src/google/protobuf/text_format.cc",
        "src/google/protobuf/timestamp.pb.cc",
        "src/google/protobuf/type.pb.cc",
        "src/google/protobuf/unknown_field_set.cc",
        "src/google/protobuf/util/delimited_message_util.cc",
        "src/google/protobuf/util/field_comparator.cc",
        "src/google/protobuf/util/field_mask_util.cc",
        "src/google/protobuf/util/internal/datapiece.cc",
        "src/google/protobuf/util/internal/default_value_objectwriter.cc",
        "src/google/protobuf/util/internal/error_listener.cc",
        "src/google/protobuf/util/internal/field_mask_utility.cc",
        "src/google/protobuf/util/internal/json_escaping.cc",
        "src/google/protobuf/util/internal/json_objectwriter.cc",
        "src/google/protobuf/util/internal/json_stream_parser.cc",
        "src/google/protobuf/util/internal/object_writer.cc",
        "src/google/protobuf/util/internal/proto_writer.cc",
        "src/google/protobuf/util/internal/protostream_objectsource.cc",
        "src/google/protobuf/util/internal/protostream_objectwriter.cc",
        "src/google/protobuf/util/internal/type_info.cc",
        "src/google/protobuf/util/internal/utility.cc",
        "src/google/protobuf/util/json_util.cc",
        "src/google/protobuf/util/message_differencer.cc",
        "src/google/protobuf/util/time_util.cc",
        "src/google/protobuf/util/type_resolver_util.cc",
        "src/google/protobuf/wire_format.cc",
        "src/google/protobuf/wrappers.pb.cc",
    ],
    hdrs = glob([
        "src/**/*.h",
        "src/**/*.inc",
    ]),
    copts = COPTS,
    includes = ["src/"],
    linkopts = LINK_OPTS,
    visibility = ["//visibility:public"],
    deps = [":protobuf_lite"] + PROTOBUF_DEPS,
)

cc_library(
    name = "protobuf_headers",
    hdrs = glob([
        "src/**/*.h",
        "src/**/*.inc",
    ]),
    includes = ["src/"],
    visibility = ["//visibility:public"],
)

WELL_KNOWN_PROTO_MAP = {
    "any": ("src/google/protobuf/any.proto", []),
    "api": (
        "src/google/protobuf/api.proto",
        [
            "source_context",
            "type",
        ],
    ),
    "compiler_plugin": (
        "src/google/protobuf/compiler/plugin.proto",
        ["descriptor"],
    ),
    "descriptor": ("src/google/protobuf/descriptor.proto", []),
    "duration": ("src/google/protobuf/duration.proto", []),
    "empty": ("src/google/protobuf/empty.proto", []),
    "field_mask": ("src/google/protobuf/field_mask.proto", []),
    "source_context": ("src/google/protobuf/source_context.proto", []),
    "struct": ("src/google/protobuf/struct.proto", []),
    "timestamp": ("src/google/protobuf/timestamp.proto", []),
    "type": (
        "src/google/protobuf/type.proto",
        [
            "any",
            "source_context",
        ],
    ),
    "wrappers": ("src/google/protobuf/wrappers.proto", []),
}

WELL_KNOWN_PROTOS = [value[0] for value in WELL_KNOWN_PROTO_MAP.values()]

LITE_WELL_KNOWN_PROTO_MAP = {
    "any": ("src/google/protobuf/any.proto", []),
    "api": (
        "src/google/protobuf/api.proto",
        [
            "source_context",
            "type",
        ],
    ),
    "duration": ("src/google/protobuf/duration.proto", []),
    "empty": ("src/google/protobuf/empty.proto", []),
    "field_mask": ("src/google/protobuf/field_mask.proto", []),
    "source_context": ("src/google/protobuf/source_context.proto", []),
    "struct": ("src/google/protobuf/struct.proto", []),
    "timestamp": ("src/google/protobuf/timestamp.proto", []),
    "type": (
        "src/google/protobuf/type.proto",
        [
            "any",
            "source_context",
        ],
    ),
    "wrappers": ("src/google/protobuf/wrappers.proto", []),
}

LITE_WELL_KNOWN_PROTOS = [value[0] for value in LITE_WELL_KNOWN_PROTO_MAP.values()]

filegroup(
    name = "well_known_protos",
    srcs = WELL_KNOWN_PROTOS,
    visibility = ["//visibility:public"],
)

filegroup(
    name = "lite_well_known_protos",
    srcs = LITE_WELL_KNOWN_PROTOS,
    visibility = ["//visibility:public"],
)

adapt_proto_library(
    name = "cc_wkt_protos_genproto",
    visibility = ["//visibility:public"],
    deps = [proto + "_proto" for proto in WELL_KNOWN_PROTO_MAP.keys()],
)

cc_library(
    name = "cc_wkt_protos",
    deprecation = "Only for backward compatibility. Do not use.",
    visibility = ["//visibility:public"],
)

[proto_library(
    name = proto[0] + "_proto",
    srcs = [proto[1][0]],
    strip_import_prefix = "src",
    visibility = ["//visibility:public"],
    deps = [dep + "_proto" for dep in proto[1][1]],
) for proto in WELL_KNOWN_PROTO_MAP.items()]

[native_cc_proto_library(
    name = proto + "_cc_proto",
    visibility = ["//visibility:private"],
    deps = [proto + "_proto"],
) for proto in WELL_KNOWN_PROTO_MAP.keys()]

################################################################################
# Protocol Buffers Compiler
################################################################################

cc_library(
    name = "protoc_lib",
    srcs = [
        "src/google/protobuf/compiler/code_generator.cc",
        "src/google/protobuf/compiler/command_line_interface.cc",
        "src/google/protobuf/compiler/cpp/cpp_enum.cc",
        "src/google/protobuf/compiler/cpp/cpp_enum_field.cc",
        "src/google/protobuf/compiler/cpp/cpp_extension.cc",
        "src/google/protobuf/compiler/cpp/cpp_field.cc",
        "src/google/protobuf/compiler/cpp/cpp_file.cc",
        "src/google/protobuf/compiler/cpp/cpp_generator.cc",
        "src/google/protobuf/compiler/cpp/cpp_helpers.cc",
        "src/google/protobuf/compiler/cpp/cpp_map_field.cc",
        "src/google/protobuf/compiler/cpp/cpp_message.cc",
        "src/google/protobuf/compiler/cpp/cpp_message_field.cc",
        "src/google/protobuf/compiler/cpp/cpp_padding_optimizer.cc",
        "src/google/protobuf/compiler/cpp/cpp_parse_function_generator.cc",
        "src/google/protobuf/compiler/cpp/cpp_primitive_field.cc",
        "src/google/protobuf/compiler/cpp/cpp_service.cc",
        "src/google/protobuf/compiler/cpp/cpp_string_field.cc",
        "src/google/protobuf/compiler/csharp/csharp_doc_comment.cc",
        "src/google/protobuf/compiler/csharp/csharp_enum.cc",
        "src/google/protobuf/compiler/csharp/csharp_enum_field.cc",
        "src/google/protobuf/compiler/csharp/csharp_field_base.cc",
        "src/google/protobuf/compiler/csharp/csharp_generator.cc",
        "src/google/protobuf/compiler/csharp/csharp_helpers.cc",
        "src/google/protobuf/compiler/csharp/csharp_map_field.cc",
        "src/google/protobuf/compiler/csharp/csharp_message.cc",
        "src/google/protobuf/compiler/csharp/csharp_message_field.cc",
        "src/google/protobuf/compiler/csharp/csharp_primitive_field.cc",
        "src/google/protobuf/compiler/csharp/csharp_reflection_class.cc",
        "src/google/protobuf/compiler/csharp/csharp_repeated_enum_field.cc",
        "src/google/protobuf/compiler/csharp/csharp_repeated_message_field.cc",
        "src/google/protobuf/compiler/csharp/csharp_repeated_primitive_field.cc",
        "src/google/protobuf/compiler/csharp/csharp_source_generator_base.cc",
        "src/google/protobuf/compiler/csharp/csharp_wrapper_field.cc",
        "src/google/protobuf/compiler/java/java_context.cc",
        "src/google/protobuf/compiler/java/java_doc_comment.cc",
        "src/google/protobuf/compiler/java/java_enum.cc",
        "src/google/protobuf/compiler/java/java_enum_field.cc",
        "src/google/protobuf/compiler/java/java_enum_field_lite.cc",
        "src/google/protobuf/compiler/java/java_enum_lite.cc",
        "src/google/protobuf/compiler/java/java_extension.cc",
        "src/google/protobuf/compiler/java/java_extension_lite.cc",
        "src/google/protobuf/compiler/java/java_field.cc",
        "src/google/protobuf/compiler/java/java_file.cc",
        "src/google/protobuf/compiler/java/java_generator.cc",
        "src/google/protobuf/compiler/java/java_generator_factory.cc",
        "src/google/protobuf/compiler/java/java_helpers.cc",
        "src/google/protobuf/compiler/java/java_kotlin_generator.cc",
        "src/google/protobuf/compiler/java/java_map_field.cc",
        "src/google/protobuf/compiler/java/java_map_field_lite.cc",
        "src/google/protobuf/compiler/java/java_message.cc",
        "src/google/protobuf/compiler/java/java_message_builder.cc",
        "src/google/protobuf/compiler/java/java_message_builder_lite.cc",
        "src/google/protobuf/compiler/java/java_message_field.cc",
        "src/google/protobuf/compiler/java/java_message_field_lite.cc",
        "src/google/protobuf/compiler/java/java_message_lite.cc",
        "src/google/protobuf/compiler/java/java_name_resolver.cc",
        "src/google/protobuf/compiler/java/java_primitive_field.cc",
        "src/google/protobuf/compiler/java/java_primitive_field_lite.cc",
        "src/google/protobuf/compiler/java/java_service.cc",
        "src/google/protobuf/compiler/java/java_shared_code_generator.cc",
        "src/google/protobuf/compiler/java/java_string_field.cc",
        "src/google/protobuf/compiler/java/java_string_field_lite.cc",
        "src/google/protobuf/compiler/js/js_generator.cc",
        "src/google/protobuf/compiler/js/well_known_types_embed.cc",
        "src/google/protobuf/compiler/objectivec/objectivec_enum.cc",
        "src/google/protobuf/compiler/objectivec/objectivec_enum_field.cc",
        "src/google/protobuf/compiler/objectivec/objectivec_extension.cc",
        "src/google/protobuf/compiler/objectivec/objectivec_field.cc",
        "src/google/protobuf/compiler/objectivec/objectivec_file.cc",
        "src/google/protobuf/compiler/objectivec/objectivec_generator.cc",
        "src/google/protobuf/compiler/objectivec/objectivec_helpers.cc",
        "src/google/protobuf/compiler/objectivec/objectivec_map_field.cc",
        "src/google/protobuf/compiler/objectivec/objectivec_message.cc",
        "src/google/protobuf/compiler/objectivec/objectivec_message_field.cc",
        "src/google/protobuf/compiler/objectivec/objectivec_oneof.cc",
        "src/google/protobuf/compiler/objectivec/objectivec_primitive_field.cc",
        "src/google/protobuf/compiler/php/php_generator.cc",
        "src/google/protobuf/compiler/plugin.cc",
        "src/google/protobuf/compiler/plugin.pb.cc",
        "src/google/protobuf/compiler/python/python_generator.cc",
        "src/google/protobuf/compiler/ruby/ruby_generator.cc",
        "src/google/protobuf/compiler/subprocess.cc",
        "src/google/protobuf/compiler/zip_writer.cc",
    ],
    copts = COPTS,
    includes = ["src/"],
    linkopts = LINK_OPTS,
    visibility = ["//visibility:public"],
    deps = [":protobuf"],
)

cc_binary(
    name = "protoc",
    srcs = ["src/google/protobuf/compiler/main.cc"],
    linkopts = LINK_OPTS,
    visibility = ["//visibility:public"],
    deps = [":protoc_lib"],
)

proto_lang_toolchain(
    name = "cc_toolchain",
    blacklisted_protos = [proto + "_proto" for proto in WELL_KNOWN_PROTO_MAP.keys()],
    command_line = "--cpp_out=$(OUT)",
    runtime = ":protobuf",
    visibility = ["//visibility:public"],
)

alias(
    name = "objectivec",
    actual = "//objectivec",
    visibility = ["//visibility:public"],
)

alias(
    name = "protobuf_objc",
    actual = "//objectivec",
    visibility = ["//visibility:public"],
)

