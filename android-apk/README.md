# Cctes Beta (Android)

APK packaging for the Android port of Cubyz. The game itself is `libCubyzig.so`
(built from `../src` against the NDK); this directory wraps it in a `NativeActivity`
shell and produces a signed APK named **Cctes Beta**.

## Build

```sh
# 1) native library (aarch64)
cd ..
export PATH=/workspaces/tools/zig-x86_64-linux-0.17.1:$PATH
HDR=$(ls -d zig-pkg/*/include | head -1)
zig build -Dtarget=aarch64-linux-android -Doptimize=ReleaseFast \
          -Dlocal=true -DandroidHeaders="$HDR"

# 2) APK
cd android-apk
ANDROID_SDK_ROOT=/workspaces/android/sdk JAVA_HOME=/home/codespace/java/current ./build_apk.sh
# -> ./cctes-beta.apk
```

`build_apk.sh` drives `aapt2 → javac → d8 → zipalign → apksigner` directly; it does
not use Gradle. Output is ~18 MB, `minSdk 28` (Android 9) / `targetSdk 34`. A
debug keystore is generated on first run (`debug.keystore`, password `cctesbeta`).

The APK is signed with all three schemes (v1 + v2 + v3). `apksigner verify`
reports `v1: false` at `--min-sdk-version 28`, but that only means v1 is not the
scheme in use for API 28+ — run it with `--min-sdk-version 21` and all three
report `true`. `minSdk 28` (not 29) is safe because `libCubyzig.so` resolves
with zero missing symbols against the API 28 stubs; only API ≤ 27 leaves
`pthread_attr_setinheritsched` unresolved.

### Environment overrides

| var | default | meaning |
| --- | --- | --- |
| `SO_PATH` | `../zig-out/lib/libCubyzig.so` | native lib to package |
| `ASSETS_DIR` | `../assets` | Cubyz asset root |
| `OUT_APK` | `./cctes-beta.apk` | output path |
| `SKIP_ASSETS` | `0` | `1` reuses already-staged assets |
| `ANDROID_SDK_ROOT` | `/workspaces/android/sdk` | build-tools + platform |

## Layout

```
app/src/main/
  AndroidManifest.xml          package cctes.beta, NativeActivity, GLES 3.2, landscape
  java/cctes/beta/MainActivity.java    NativeActivity subclass (keep-screen-on, immersive)
  java/cctes/beta/AssetExtractor.java  unpacks APK assets to files/ on first launch
  res/values/strings.xml       app_name = "Cctes Beta"
  assets/                      staged Cubyz assets (music excluded)
  jniLibs/arm64-v8a/libCubyzig.so    staged, stripped
```

## How it starts up

1. `MainActivity.onCreate` runs `AssetExtractor.ensure(this, getFilesDir())` **before**
   `super.onCreate()`. That call is what spawns the native thread, and the engine
   opens `assets/cubyz/...` immediately, so the unpack has to finish first.
2. `AssetExtractor` streams every `assets/` entry out of the APK zip (falling back
   to `AssetManager.list`/`open` if the APK isn't a plain file) into
   `<filesDir>/assets/`, stamping a version file so repeat launches skip it.
3. `android_main.c` sets HOME and cwd to `activity->internalDataPath` — which is
   already `getFilesDir()`, i.e. exactly where step 2 unpacked to.

The assets must live on the real filesystem: the engine reads them with ordinary
file IO, and assets inside an APK are only reachable through `AssetManager`.

The engine's own asset tree, mod folder and shader paths are all keyed to
`cubyz`, so only the *display* label differs — nothing inside the game is renamed.

## Notes / gotchas

- **`-Osmall` is broken on Zig 0.17.1** (deterministic SEGV in the codegen pass:
  `segfault at 0 ... error 6`, same instruction every time, *not* OOM). Build with
  `-Doptimize=ReleaseFast`; the APK still strips to ~8.7 MB.
- **`-DsanitizeThread` / ReleaseSafe** fail in `translate-c` on NDK r28c's
  `bits/fortify/*.h` (`__memchr_chk` etc. are undeclared, `sprintf` redefined).
- **Music is excluded** from the APK: 101 MB of `.ogg` that `src/audio.zig` treats
  as optional (it logs and returns `null` when a track is missing). Delete the
  `--exclude='cubyz/music'` in `build_apk.sh` if you want it back.
- **`-DandroidHeaders` is mandatory** for a clean rebuild: the glslang headers
  (`glslang/Include/glslang_c_interface.h`) live only in the cross-platform
  `cubyz_deps_headers` archive, not the Android libraries archive. Pass
  `zig-pkg/*/include`.
- **libc++ namespace must match the NDK's (`__ndk1`, not Zig's `__1`).** Zig's
  bundled libc++ mangles `std::` types into `inline namespace __1`, but the NDK's
  `libc++_shared.so` — the only libc++ on an Android device — exports them as
  `__ndk1`. The two are ABI-identical (verified: `sizeof(std::mutex)==40`,
  `sizeof(std::condition_variable)==48` under all three schemes), so only the
  mangled prefix differs. Harfbuzz (`harfbuzz.cc`) and the Vulkan allocator
  (`vma.cpp`) are C++, and when the local `../Cubyz-Libs` deps archive is built
  with **Zig 0.16.0** (its `build.zig` uses the removed `Step.makeFn` API, so
  0.17.1 cannot build it) their objects reference `_ZNSt3__1...`. Those symbols
  exist nowhere on-device, and since `libCubyzig.so` links `c++_shared` with
  `BIND_NOW` set, the loader aborts at `dlopen` — before `main()` — with
  `cannot locate symbol "_ZNSt3__15mutexD1Ev"`. Fix: `androidCFlags()` in the
  Cubyz-Libs `build.zig` appends `-D_LIBCPP_ABI_NAMESPACE=__ndk1`, then rebuild
  the deps and `libCubyzig.so`. Verify with
  `llvm-readelf --dyn-syms libCubyzig.so | grep St3__1` → must be empty.
- **`_Unwind_Resume` must be linked statically (`libunwind.a`), not left
  undefined.** The NDK's `clang++` adds `-l:libunwind.a` to every link line
  implicitly; Zig does not. Without it the C++ objects in the deps archive
  (harfbuzz, vma) leave `_Unwind_Resume` as a **strong** undefined symbol — and
  the only provider is bionic's `libc.so`, which exports it as `_Unwind_Resume@@LIBC_R`
  starting at **API 30**. On API 29 (Android 10) the symbol does not exist, so
  with `BIND_NOW` set the loader aborts at `dlopen`. `libc++_shared.so` cannot
  help: its copy is `LOCAL HIDDEN`, invisible to other libraries. Fix: `linkAndroid()`
  in `build.zig` adds the NDK's
  `lib/clang/<ver>/lib/linux/aarch64/libunwind.a` via `addObjectFile`. Verify with
  `llvm-readelf --syms libCubyzig.so | grep _Unwind_Resume` → must show
  `LOCAL HIDDEN`, not `UND`. (The alternative is bumping `minSdk` to 30, which
  drops Android 10.)
- **`extractNativeLibs="true"`** in the manifest — the loader needs the `.so` on
  the real filesystem, since the assets are read through relative paths.
- **`resources.arsc` must stay uncompressed.** Android 11+ (targetSdk 30+) rejects
  an APK whose `resources.arsc` is DEFLATED with
  `INSTALL_PARSE_FAILED_UNEXPECTED_EXCEPTION` — a "can't install" that looks like
  nothing more than a corrupt download. `aapt2 link` emits it STORED, so the
  packaging step must not unpack-and-rezip the whole APK; it copies `base.apk`
  and adds the `.so`/`classes.dex` with `zip -0`. Verify with:
  `python3 -c "import zipfile;print(zipfile.ZipFile('cctes-beta.apk').getinfo('resources.arsc').compress_type)"`
  → `0` is correct.
