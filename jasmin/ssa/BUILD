package(default_visibility = ["//visibility:private"])

cc_library(
    name = "register_coalescer",
    hdrs = ["register_coalescer.h"],
    srcs = ["register_coalescer.cc"],
    visibility = ["//visibility:public"],
    deps = [
        ":ssa",
        "@nth_cc//nth/debug",
        "@nth_cc//nth/container:disjoint_set",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

cc_library(
    name = "ssa",
    hdrs = ["ssa.h"],
    srcs = ["ssa.cc"],
    visibility = ["//visibility:public"],
    deps = [
        "//jasmin/core:function",
        "//jasmin/core:instruction",
        "//jasmin/core:metadata",
        "@nth_cc//nth/debug",
        "@nth_cc//nth/container:disjoint_set",
        "@nth_cc//nth/strings:interpolate",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_binary(
    name = "test",
    srcs = ["main.cc"],
    deps = [
        ":register_coalescer",
        ":ssa",
        "//jasmin/core:instruction",
        "//jasmin/instructions:arithmetic",
        "//jasmin/instructions:compare",
        "//jasmin/instructions:common",
        "@nth_cc//nth/debug",
        "@nth_cc//nth/debug/log:stderr_log_sink",
        "@com_google_absl//absl/container:flat_hash_set",
    ],
)
