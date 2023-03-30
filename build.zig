const std = @import("std");
const Self = @This();
const CrossTarget = std.zig.CrossTarget;

pub fn build(b: *std.Build) !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer {
        if (gpa.deinit()) {
            @panic("Memory leak!");
        }
    }
    var allocator = std.heap.ArenaAllocator.init(gpa.allocator());
    defer allocator.deinit();

    const target = b.standardTargetOptions(.{});
    const mode = b.standardOptimizeOption(.{});

    const dxlib_options: std.build.SharedLibraryOptions = .{
        .name = "DxPortLib",
        .target = target,
        .optimize = mode,
    };

    const dxLib: *std.build.CompileStep = b.addSharedLibrary(dxlib_options);

    dxLib.linkLibC();
    dxLib.linkLibCpp();

    dxLib.addIncludePath("DxPortLib/vorbis/include");
    dxLib.addIncludePath("DxPortLib/ogg/include");

    dxLib.linkSystemLibrary("SDL2");
    dxLib.linkSystemLibrary("SDL2_ttf");
    dxLib.linkSystemLibrary("SDL2_image");
    dxLib.linkSystemLibrary("vorbisfile");

    dxLib.addIncludePath("src");
    dxLib.addIncludePath("include");

    var dxlib_sources = try discover_dxportlib_sources(allocator.allocator());

    dxLib.addCSourceFiles(dxlib_sources.c, &.{});
    dxLib.addCSourceFiles(dxlib_sources.cpp, &.{"-std=c++11"});

    dxLib.install();
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
