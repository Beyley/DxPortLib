const std = @import("std");
const Self = @This();
const CrossTarget = std.zig.CrossTarget;

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    var dxLibShared = try create_dxlib_library(b, target, optimize, b.allocator, true);
    var dxLibStatic = try create_dxlib_library(b, target, optimize, b.allocator, false);

    dxLibShared.compile_step.install();
    dxLibStatic.compile_step.install();
}

pub fn create_dxlib_library(b: *std.Build, target: std.zig.CrossTarget, optimize: std.builtin.Mode, allocator: std.mem.Allocator, shared: bool) !struct { compile_step: *std.build.CompileStep, config_step: *std.build.ConfigHeaderStep } {
    const shared_dxlib_options: std.build.SharedLibraryOptions = .{
        .name = "DxPortLib",
        .target = target,
        .optimize = optimize,
    };

    const static_dxlib_options: std.build.StaticLibraryOptions = .{
        .name = "DxPortLib",
        .target = target,
        .optimize = optimize,
    };

    var dxLib: *std.build.CompileStep = if (shared) b.addSharedLibrary(shared_dxlib_options) else b.addStaticLibrary(static_dxlib_options);

    dxLib.linkLibC();
    dxLib.linkLibCpp();

    dxLib.addIncludePath(root_path ++ "vorbis/include");
    dxLib.addIncludePath(root_path ++ "ogg/include");

    dxLib.linkSystemLibrary("SDL2");
    dxLib.linkSystemLibrary("SDL2_ttf");
    dxLib.linkSystemLibrary("SDL2_image");
    dxLib.linkSystemLibrary("vorbisfile");

    dxLib.addIncludePath(root_path ++ "src");
    dxLib.addIncludePath(root_path ++ "include");

    var dxlib_sources = try discover_dxportlib_sources(allocator);

    dxLib.addCSourceFiles(dxlib_sources.c, &.{});
    dxLib.addCSourceFiles(dxlib_sources.cpp, &.{"-std=c++11"});

    var config_header: *std.build.ConfigHeaderStep = b.addConfigHeader(std.build.ConfigHeaderStep.Options{
        .style = .blank,
        .include_path = "DPLBuildConfig.h",
    }, .{
        .DXPORTLIB = @boolToInt(true),
        .DXPORTLIB_VERSION = "\"0.5.0\"",
        .DXPORTLIB_DXLIB_INTERFACE = @boolToInt(true),
        .DXPORTLIB_LUNA_INTERFACE = @boolToInt(true),
        .DXPORTLIB_DPL_INTERFACE = @boolToInt(true),
        .DXPORTLIB_PLATFORM_SDL2 = @boolToInt(true),
        // .DXPORTLIB_NO_SJIS = @boolToInt(true),
        .DXPORTLIB_DRAW_OPENGL = @boolToInt(true),
        .DXPORTLIB_DRAW_OPENGL_ES2 = @boolToInt(true),
        // .DXPORTLIB_DRAW_DIRECT3D9 = @boolToInt(true),
        // .DXPORTLIB_NO_SOUND = @boolToInt(true),
        // .DXPORTLIB_NO_INPUT = @boolToInt(true),
        // .DXPORTLIB_NO_TTF_FONT = @boolToInt(true),
        // .DXPORTLIB_NO_DXLIB_DXA = @boolToInt(true),
        // .DXPORTLIB_LUNA_DYNAMIC_MATH_TABLE = @boolToInt(true),
    });

    dxLib.addConfigHeader(config_header);

    return .{ .compile_step = dxLib, .config_step = config_header };
}

/// Discovers the paths of all the dxlib sources, returns a struct with an array of both the c and cpp sources
/// Memory needs to be cleared manually by the caller
fn discover_dxportlib_sources(allocator: std.mem.Allocator) !struct { c: []const []const u8, cpp: []const []const u8 } {
    var c_list = std.ArrayList([]const u8).init(allocator);
    var cpp_list = std.ArrayList([]const u8).init(allocator);

    const dxportlib_source_root: []const u8 = root_path ++ "src/";

    var dir = try std.fs.openIterableDirAbsolute(dxportlib_source_root, .{});
    defer dir.close();

    var walker: std.fs.IterableDir.Walker = try dir.walk(allocator);
    defer walker.deinit();

    var itr_next: ?std.fs.IterableDir.Walker.WalkerEntry = try walker.next();
    while (itr_next != null) {
        var next: std.fs.IterableDir.Walker.WalkerEntry = itr_next.?;

        const file_type = source_file_type(next.path);

        //If the filetype is one of the
        if (file_type != FileType.Unknown) {
            //Allocate the item
            var item = try allocator.alloc(u8, next.path.len + dxportlib_source_root.len);

            //copy the root first
            std.mem.copy(u8, item, dxportlib_source_root);

            //copy the filepath next
            std.mem.copy(u8, item[dxportlib_source_root.len..], next.path);

            //Append the item to the correct list
            switch (file_type) {
                .C => try c_list.append(item),
                .Cpp => try cpp_list.append(item),
                .Unknown => unreachable,
            }
        }

        itr_next = try walker.next();
    }

    return .{ .c = try c_list.toOwnedSlice(), .cpp = try cpp_list.toOwnedSlice() };
}

const FileType = enum { Unknown, C, Cpp };

/// Returns the file type of the source file, or `Unknown` if its not a relavent source file
fn source_file_type(filename: []const u8) FileType {
    if (std.mem.endsWith(u8, filename, ".c")) {
        return FileType.C;
    }

    if (std.mem.endsWith(u8, filename, ".cpp")) {
        return FileType.Cpp;
    }

    return FileType.Unknown;
}

/// Finds the root of the project
fn root() []const u8 {
    return std.fs.path.dirname(@src().file) orelse @panic("Unable to get the directory name of the build.zig file!");
}

/// The root path of the project
const root_path = root() ++ "/";
