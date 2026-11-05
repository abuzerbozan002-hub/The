const std = @import("std");

fn libName(b: *std.Build, name: []const u8, target: std.Target) []const u8 {
	return switch (target.os.tag) {
		.windows => b.fmt("{s}.lib", .{name}),
		else => b.fmt("lib{s}.a", .{name}),
	};
}

/// Resolved locations and C flags needed to build Cubyz against the NDK.
/// Zig cannot autodetect an Android libc, so the sysroot, the arch-specific
/// include dir, and clang's resource-dir headers all have to be passed in.
const AndroidEnv = struct {
	sysroot: []const u8,
	sysrootInclude: []const u8,
	archInclude: []const u8,
	clangInclude: []const u8,
	apiLibDir: []const u8,
	archLibDir: []const u8,
	glueDir: []const u8,
	libcFile: std.Build.LazyPath,
	unwindLib: []const u8,
	flags: []const []const u8,
};

fn androidEnv(b: *std.Build) AndroidEnv {
	const ndk = b.option([]const u8, "ndk", "Path to the Android NDK (r28c by default)") orelse
		"/workspaces/android/android-ndk-r28c";
	const sysroot = b.pathJoin(&.{ ndk, "toolchains/llvm/prebuilt/linux-x86_64/sysroot" });
	const archInclude = b.pathJoin(&.{ sysroot, "usr/include/aarch64-linux-android" });
	const clangLibRoot = b.pathJoin(&.{ ndk, "toolchains/llvm/prebuilt/linux-x86_64/lib/clang" });

	// The clang resource dir is versioned (`.../lib/clang/19/include`).
	var clangInclude: []const u8 = "";
	var clangVer: []const u8 = "";
	if (std.Io.Dir.cwd().openDir(b.graph.io, clangLibRoot, .{.iterate = true})) |dir| {
		var dirVar = dir;
		var it = dirVar.iterate();
		while (it.next(b.graph.io) catch null) |entry| {
			if (entry.kind != .directory) continue;
			const candidate = b.pathJoin(&.{ clangLibRoot, entry.name, "include" });
			std.Io.Dir.cwd().access(b.graph.io, candidate, .{}) catch continue;
			clangInclude = b.dupe(candidate);
			clangVer = b.dupe(entry.name);
			break;
		}
	} else |_| {}

	// Zig's `--libc` file: the Android libc is present, it just isn't detected.
	const apiLibDir = b.pathJoin(&.{ sysroot, "usr/lib/aarch64-linux-android/29" });
	const libcText = b.fmt(
		\\include_dir={s}/usr/include
		\\include_dir={s}
		\\sys_include_dir={s}
		\\cc_dir={s}
		\\crt_dir={s}
		\\msvc_lib_dir=
		\\kernel32_lib_dir=
		\\darwin_sdk_dir=
		\\
	, .{ sysroot, archInclude, archInclude, apiLibDir, apiLibDir });
	const wf = b.addWriteFiles();
	const libcFile = wf.add("libc-android.txt", libcText);

	const sysrootInclude = b.pathJoin(&.{ sysroot, "usr/include" });

	const flags = b.allocator.dupe([]const u8, &.{
		"-g",
		"-fPIC",
		b.fmt("-isystem{s}", .{sysrootInclude}),
		b.fmt("-isystem{s}", .{archInclude}),
	}) catch @panic("OOM");

	return .{
		.sysroot = sysroot,
		.sysrootInclude = sysrootInclude,
		.archInclude = archInclude,
		.clangInclude = clangInclude,
		.apiLibDir = b.pathJoin(&.{ sysroot, "usr/lib/aarch64-linux-android/29" }),
		.archLibDir = b.pathJoin(&.{ sysroot, "usr/lib/aarch64-linux-android" }),
		.glueDir = b.pathJoin(&.{ ndk, "sources/android/native_app_glue" }),
		.libcFile = libcFile,
		.unwindLib = b.pathJoin(&.{ ndk, "toolchains/llvm/prebuilt/linux-x86_64/lib/clang", clangVer, "lib/linux/aarch64/libunwind.a" }),
		.flags = flags,
	};
}

fn linkAndroid(b: *std.Build, exe: *std.Build.Step.Compile, target: std.Build.ResolvedTarget, libsDeps: *std.Build.Dependency, headersDeps: *std.Build.Dependency, artifactName: []const u8) void {
	const optimize = exe.root_module.optimize.?;
	const env = androidEnv(b);

	// The glad/GLFW shim headers that stand in for the desktop ones. This must
	// come first: the cross-platform headers archive below also carries the
	// real GLFW and glad headers, and the shims have to win.
	const shimDir = b.path("src/android/shim");
	exe.root_module.addIncludePath(shimDir);

	// Third-party C libraries built for aarch64-linux-android, plus the
	// platform-independent headers archive (which carries glslang and
	// tinyfiledialogs headers that the deps archive does not ship).
	exe.root_module.addIncludePath(headersDeps.path("include"));
	exe.root_module.addObjectFile(libsDeps.path("lib").path(b, artifactName));

	// Cubyz's own Android platform layer: GLFW replacement, NDK entry point,
	// and the Vulkan/glslang/tinyfiledialogs stubs.
	exe.root_module.addCSourceFiles(.{
		.root = b.path("src/android"),
		.files = &.{ "glfw_shim.c", "android_main.c", "engine_stubs.c", "vulkan_stubs.c", "glad_shim.c", "android_native_app_glue.c" },
		.flags = env.flags,
	});
	exe.root_module.addIncludePath(b.path("src/android"));

	// The NDK's native_app_glue provides android_main's plumbing. The local copy
	// in src/android is the same file with one change: it raises the game thread's
	// stack to 16 MB. bionic defaults to ~1 MB, which Cubyz's ~2.25 MB
	// renderer.init frame and ~4.5 MB worldgen frames overflow on the first write.
	// (The engine assumes a big stack: Zig gives ThreadPool workers 16 MB.) The
	// header still comes from the NDK, but it goes on the include path after
	// src/android for clarity.
	exe.root_module.addIncludePath(.{.cwd_relative = env.glueDir});

	exe.root_module.addSystemIncludePath(.{.cwd_relative = env.sysrootInclude});
	exe.root_module.addSystemIncludePath(.{.cwd_relative = env.archInclude});
	if (env.clangInclude.len > 0) exe.root_module.addSystemIncludePath(.{.cwd_relative = env.clangInclude});
	exe.setLibCFile(env.libcFile);
	exe.root_module.addLibraryPath(.{.cwd_relative = env.apiLibDir});
	exe.root_module.addLibraryPath(.{.cwd_relative = env.archLibDir});
	// NDK clang++ links `libunwind.a` implicitly; zig does not. Without it the C++
	// objects in the deps archive (harfbuzz, vma) leave `_Unwind_Resume` undefined,
	// and bionic only exports that symbol from `libc.so` on API 30+ (`@@LIBC_R`) —
	// on API 29 the loader aborts the process at dlopen. Linking the archive here
	// matches the NDK toolchain and keeps minSdk 29 working.
	if (env.unwindLib.len > 0) exe.root_module.addObjectFile(.{.cwd_relative = env.unwindLib});
	// libc++_shared rather than c++: the latter makes zig try to build its own
	// libc++, which needs libc++ headers bionic does not provide.
	for ([_][]const u8{ "android", "log", "EGL", "GLESv3", "c++_shared", "dl", "m", "z" }) |lib| {
		exe.root_module.linkSystemLibrary(lib, .{});
	}

	const translate_c = b.addTranslateC(.{
		.root_source_file = b.path("src/c_android.h"),
		.target = target,
		.optimize = optimize,
	});
	translate_c.addIncludePath(shimDir);
	translate_c.addIncludePath(headersDeps.path("include"));
	// The cross-platform headers archive ships the glslang and tinyfiledialogs
	// headers, which have no architecture-specific content and are therefore
	// absent from the Android libraries archive. It also carries the *real*
	// glad and GLFW headers, so this must come after shimDir — otherwise the
	// desktop glad dispatch table shadows the GLES shim.
	if (b.option([]const u8, "androidHeaders", "Cross-platform cubyz_deps headers dir for the Android build")) |extra| {
		exe.root_module.addIncludePath(.{.cwd_relative = extra});
		translate_c.addIncludePath(.{.cwd_relative = extra});
	}
	translate_c.addSystemIncludePath(.{.cwd_relative = env.sysrootInclude});
	translate_c.addSystemIncludePath(.{.cwd_relative = env.archInclude});
	if (env.clangInclude.len > 0) translate_c.addSystemIncludePath(.{.cwd_relative = env.clangInclude});
	exe.root_module.addImport("c", translate_c.createModule());
}

fn linkLibraries(b: *std.Build, exe: *std.Build.Step.Compile, useLocalDeps: bool) void {
	const target = exe.root_module.resolved_target.?;
	const t = target.result;
	const optimize = exe.root_module.optimize.?;

	const isAndroid = t.abi.isAndroid();
	const archName = @tagName(t.cpu.arch);

	const depsLib = if (isAndroid) b.fmt("cubyz_deps_{s}-linux-android", .{archName}) else b.fmt("cubyz_deps_{s}-{s}-{s}", .{ archName, @tagName(t.os.tag), switch (t.os.tag) {
		.linux => "musl",
		.macos => "none",
		.windows => "gnu",
		else => "none",
	}});
	const artifactName = libName(b, depsLib, t);

	var depsName: []const u8 = if (isAndroid) b.fmt("cubyz_deps_{s}_linux_android", .{archName}) else b.fmt("cubyz_deps_{s}_{s}", .{ archName, @tagName(t.os.tag) });
	if (useLocalDeps) depsName = "local";

	const libsDeps = b.lazyDependency(depsName, .{
		.target = target,
		.optimize = optimize,
	}) orelse {
		// Lazy dependencies with a `url` field will fail here the first time.
		// build.zig will restart and try again.
		std.log.info("Downloading cubyz_deps libraries {s}.", .{depsName});
		return;
	};
	const headersDeps = if (useLocalDeps) libsDeps else b.lazyDependency("cubyz_deps_headers", .{}) orelse {
		std.log.info("Downloading cubyz_deps headers {s}.", .{depsName});
		return;
	};

	if (isAndroid) {
		linkAndroid(b, exe, target, libsDeps, headersDeps, artifactName);
		return;
	}

	exe.root_module.addIncludePath(headersDeps.path("include"));
	exe.root_module.addObjectFile(libsDeps.path("lib").path(b, artifactName));
	const subPath = libsDeps.path("lib").path(b, depsLib);
	exe.root_module.addObjectFile(subPath.path(b, libName(b, "glslang", t)));
	exe.root_module.addObjectFile(subPath.path(b, libName(b, "MachineIndependent", t)));
	exe.root_module.addObjectFile(subPath.path(b, libName(b, "GenericCodeGen", t)));
	exe.root_module.addObjectFile(subPath.path(b, libName(b, "glslang-default-resource-limits", t)));
	exe.root_module.addObjectFile(subPath.path(b, libName(b, "SPIRV", t)));
	exe.root_module.addObjectFile(subPath.path(b, libName(b, "SPIRV-Tools", t)));
	exe.root_module.addObjectFile(subPath.path(b, libName(b, "SPIRV-Tools-opt", t)));

	const translate_c = b.addTranslateC(.{
		.root_source_file = b.path("src/c.h"),
		.target = target,
		.optimize = optimize,
	});
	translate_c.addIncludePath(headersDeps.path("include"));

	exe.root_module.addImport("c", translate_c.createModule());

	if (t.os.tag == .macos) {
		const moltenVkLibInstall = b.addInstallFile(subPath.path(b, "libMoltenVK.dylib"), "bin/Cubyz.app/Contents/Frameworks/libMoltenVK.dylib");
		const moltenVkJsonInstall = b.addInstallFile(subPath.path(b, "MoltenVK_icd.json"), "bin/Cubyz.app/Contents/Resources/vulkan/icd.d/MoltenVK_icd.json");
		exe.step.dependOn(&moltenVkLibInstall.step);
		exe.step.dependOn(&moltenVkJsonInstall.step);

		const validationLayerLibInstall = b.addInstallFile(subPath.path(b, "libVkLayer_khronos_validation.dylib"), "bin/Cubyz.app/Contents/Frameworks/libVkLayer_khronos_validation.dylib");
		const validationLayerJsonInstall = b.addInstallFile(subPath.path(b, "VkLayer_khronos_validation.json"), "bin/Cubyz.app/Contents/Resources/vulkan/explicit_layer.d/VkLayer_khronos_validation.json");
		exe.step.dependOn(&validationLayerLibInstall.step);
		exe.step.dependOn(&validationLayerJsonInstall.step);
	}

	if (t.os.tag == .windows) {
		exe.root_module.linkSystemLibrary("bcrypt", .{});
		exe.root_module.linkSystemLibrary("comdlg32", .{});
		exe.root_module.linkSystemLibrary("crypt32", .{});
		exe.root_module.linkSystemLibrary("gdi32", .{});
		exe.root_module.linkSystemLibrary("ole32", .{});
		exe.root_module.linkSystemLibrary("opengl32", .{});
		exe.root_module.linkSystemLibrary("ws2_32", .{});
	} else if (t.os.tag == .macos) {
		exe.root_module.linkFramework("Cocoa", .{});
		exe.root_module.linkFramework("CoreFoundation", .{});
		exe.root_module.linkFramework("IOKit", .{});
		exe.root_module.linkFramework("QuartzCore", .{});
	} else if (t.os.tag != .linux) {
		std.log.err("Unsupported target: {}\n", .{t.os.tag});
	}
}

pub fn addModFeatureModule(b: *std.Build, modFinderStep: *std.Build.Step.Run, exe: *std.Build.Step.Compile, modsFolder: std.Build.LazyPath, name: []const u8) !void {
	const module = b.createModule(.{
		.root_source_file = try modsFolder.join(b.allocator, b.fmt("{s}.zig", .{name})),
		.target = exe.root_module.resolved_target,
		.optimize = exe.root_module.optimize,
	});

	if (exe.kind == .@"test") {
		const exe_tests = b.addTest(.{
			.root_module = module,
			.test_runner = exe.test_runner,
		});
		const run_exe_tests = b.addRunArtifact(exe_tests);
		exe.step.dependOn(&run_exe_tests.step);
	}
	module.addImport("main", exe.root_module);
	exe.step.dependOn(&modFinderStep.step);
	exe.root_module.addImport(name, module);
}

fn addModFeatures(b: *std.Build, exe: *std.Build.Step.Compile) !void {
	const modFinder = b.addExecutable(.{
		.name = "mod_finder",
		.root_module = b.createModule(.{
			.root_source_file = b.path("scripts/mod_finder.zig"),
			.target = b.graph.host,
		}),
	});
	const modFinderStep = b.addRunArtifact(modFinder);
	modFinderStep.addDirectoryArg(b.path("mods"));
	modFinderStep.addDirectoryArg(b.path("mods"));

	try addModFeatureModule(b, modFinderStep, exe, b.path("mods"), "rotations");
}

fn createLaunchConfig(b: *std.Build) !void {
	var io = std.Io.Threaded.init(b.allocator, .{});
	defer io.deinit();
	std.Io.Dir.cwd().access(io.io(), "launchConfig.zon", .{}) catch {
		const launchConfig =
			\\.{
			\\    .cubyzDir = "",
			\\    .autoEnterWorld = "",
			\\    .headlessServer = false,
			\\    .threadPoolThreads = null, // Number of threads used by the threadPool, default is number of CPU cores - 1 (to leave room for rendering, audio, networking, server ticks, OS)
			\\    // .preferredAuthenticationAlgorithm = .ed25519, // Uncomment and change this if you own a server in an outdated game version where the default algorithm got compromised.
			\\}
		;
		try std.Io.Dir.cwd().writeFile(io.io(), .{
			.data = launchConfig,
			.sub_path = "launchConfig.zon",
		});
	};
}

pub fn build(b: *std.Build) !void {
	try createLaunchConfig(b);

	// Standard target options allows the person running `zig build` to choose
	// what target to build for. Here we do not override the defaults, which
	// means any target is allowed, and the default is native. Other options
	// for restricting supported target set are available.
	const target = b.standardTargetOptions(.{});

	// Standard release options allow the person running `zig build` to select
	// between Debug, ReleaseSafe, ReleaseFast, and ReleaseSmall.
	const optimize = b.standardOptimizeOption(.{});

	const isAndroidTarget = target.result.abi.isAndroid();

	const options = b.addOptions();
	const isRelease = b.option(bool, "release", "Removes the -dev flag from the version") orelse false;
	const sanitizeThread = b.option(bool, "sanitizeThread", "enables the builtin thread sanitizer");
	var version = try std.SemanticVersion.parse("0.5.0");
	if (b.option([]const u8, "version", "used by the CI to set the patch version, major and minor must match the ones in build.zig")) |tagVersion| {
		const tagParsed = try std.SemanticVersion.parse(tagVersion);
		if (tagParsed.major != version.major or tagParsed.minor != version.minor) {
			std.log.err("Provided version {s} does not match version in build.zig: {f}", .{tagVersion, version});
			return error.VersionMismatch;
		}
		version.patch = tagParsed.patch;
	}
	if (!isRelease) version.pre = "dev";
	options.addOption([]const u8, "version", b.fmt("{f}", .{version}));
	options.addOption(bool, "isTaggedRelease", isRelease);

	const useLocalDeps = b.option(bool, "local", "Use local cubyz_deps") orelse false;

	const largeAssets = b.dependency("cubyz_large_assets", .{});
	b.installDirectory(.{
		.source_dir = largeAssets.path("music"),
		.install_subdir = "assets/cubyz/music/",
		.install_dir = .{.custom = ".."},
	});
	b.installDirectory(.{
		.source_dir = largeAssets.path("fonts"),
		.install_subdir = "assets/cubyz/fonts/",
		.install_dir = .{.custom = ".."},
	});

	const mainModule = b.addModule("main", .{
		.root_source_file = b.path("src/main.zig"),
		.target = target,
		.optimize = optimize,
		.link_libc = true,
		// On Android, libc++ comes from the NDK (linked explicitly below);
		// letting zig build its own would require libc++ headers that bionic
		// does not ship (sched.h and friends).
		.link_libcpp = !isAndroidTarget,
		.sanitize_thread = sanitizeThread,
	});

	const isAndroid = isAndroidTarget;

	// On Android the game is a shared library loaded by the Java activity
	// (libCubyzig.so); everywhere else it is a normal executable.
	const exe = if (isAndroid) b.addLibrary(.{
		.name = "Cubyzig",
		.root_module = mainModule,
		.linkage = .dynamic,
	}) else b.addExecutable(.{
		.name = "Cubyz",
		.root_module = mainModule,
		.use_llvm = if (sanitizeThread orelse false) true else null,
	});
	exe.root_module.addOptions("build_options", options);
	exe.root_module.addImport("main", mainModule);
	try addModFeatures(b, exe);

	if (isRelease and target.result.os.tag == .windows) {
		exe.subsystem = .windows;
	}

	linkLibraries(b, exe, useLocalDeps);

	if (isAndroid) {
		// No `run` step and no tests: the artifact is an aarch64 .so that
		// cannot execute on the build host.
		const installLib = b.addInstallArtifact(exe, .{});
		b.getInstallStep().dependOn(&installLib.step);
		return;
	}

	var exeInstallOptions: std.Build.Step.InstallArtifact.Options = .{};
	if (target.result.os.tag == .macos) {
		exeInstallOptions = .{
			.dest_dir = .{.override = .{.custom = "bin/Cubyz.app/Contents/MacOS"}},
		};

		const plistContents =
			\\<?xml version="1.0" encoding="UTF-8"?>
			\\<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
			\\<plist version="1.0">
			\\<dict>
			\\    <key>CFBundleIconFile</key>
			\\    <string>logo</string>
			\\</dict>
			\\</plist>
		;

		const writeFiles = b.addWriteFiles();
		const plistPath = writeFiles.add("Info.plist", plistContents);
		const plistInstall = b.addInstallFile(plistPath, "bin/Cubyz.app/Contents/Info.plist");
		b.getInstallStep().dependOn(&plistInstall.step);
		const iconsInstall = b.addInstallFile(b.path("assets/cubyz/logo.icns"), "bin/Cubyz.app/Contents/Resources/logo.icns");
		b.getInstallStep().dependOn(&iconsInstall.step);

		// NOTE(blackedout): This is to make the Vulkan loader search in (bundle)/Contents/Frameworks to find the libs referenced in the manifest files
		exe.root_module.addRPathSpecial("@loader_path/../Frameworks");
	}

	const installExe = b.addInstallArtifact(exe, exeInstallOptions);
	b.getInstallStep().dependOn(&installExe.step);

	const run_cmd = b.addRunArtifact(exe);
	run_cmd.step.dependOn(b.getInstallStep());
	run_cmd.addPassthruArgs();

	const run_step = b.step("run", "Run the app");
	run_step.dependOn(&run_cmd.step);

	const dependencyWithTestRunner = b.lazyDependency("cubyz_test_runner", .{
		.target = target,
		.optimize = optimize,
	}) orelse {
		std.log.info("Downloading cubyz_test_runner dependency.", .{});
		return;
	};
	const exe_tests = b.addTest(.{
		.root_module = mainModule,
		.test_runner = .{.path = dependencyWithTestRunner.path("lib/compiler/test_runner.zig"), .mode = .simple},
	});
	linkLibraries(b, exe_tests, useLocalDeps);
	exe_tests.root_module.addOptions("build_options", options);
	exe_tests.root_module.addImport("main", mainModule);
	try addModFeatures(b, exe_tests);
	const run_exe_tests = b.addRunArtifact(exe_tests);

	const test_step = b.step("test", "Run unit tests");
	test_step.dependOn(&run_exe_tests.step);
}
