#!/usr/bin/env bash
# Package the prebuilt libCubyzig.so + Cubyz assets into a signed "Cctes Beta" APK.
#
# Deliberately does not use Gradle: aapt2 + javac + d8 + zipalign + apksigner are
# enough, and it keeps the whole pipeline reproducible from one script.
#
# Inputs:
#   SO_PATH      path to libCubyzig.so              (default: ../zig-out/lib/libCubyzig.so)
#   ASSETS_DIR   Cubyz assets root                  (default: ../assets)
#   OUT_APK      output apk                         (default: ./cctes-beta.apk)
#   SKIP_ASSETS  1 to reuse assets already staged
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
SDK="${ANDROID_SDK_ROOT:-/workspaces/android/sdk}"
BT="$SDK/build-tools/34.0.0"
PLATFORM="$SDK/platforms/android-34/android.jar"

SO_PATH="${SO_PATH:-$HERE/../zig-out/lib/libCubyzig.so}"
ASSETS_DIR="${ASSETS_DIR:-$HERE/../assets}"
OUT_APK="${OUT_APK:-$HERE/cctes-beta.apk}"
SKIP_ASSETS="${SKIP_ASSETS:-0}"

APP="$HERE/app"
MAIN="$APP/src/main"
BUILD="$HERE/.build"
KEYSTORE="$HERE/debug.keystore"

for tool in aapt2 d8 zipalign apksigner; do
	[ -x "$BT/$tool" ] || { echo "missing $BT/$tool" >&2; exit 1; }
done
[ -f "$PLATFORM" ] || { echo "missing $PLATFORM" >&2; exit 1; }
[ -f "$SO_PATH" ] || { echo "missing native lib $SO_PATH" >&2; exit 1; }

# ---------------------------------------------------------------- native lib
# Strip debug info: a ReleaseFast build with debug_info is ~53 MB, stripped ~9 MB.
# The APK never needs the DWARF blobs.
STRIP="${STRIP:-/workspaces/android/android-ndk-r28c/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-strip}"
CXX_SHARED="${CXX_SHARED:-/workspaces/android/android-ndk-r28c/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so}"

mkdir -p "$MAIN/jniLibs/arm64-v8a"
if [ -x "$STRIP" ]; then
	"$STRIP" --strip-debug -o "$MAIN/jniLibs/arm64-v8a/libCubyzig.so" "$SO_PATH"
else
	cp -f "$SO_PATH" "$MAIN/jniLibs/arm64-v8a/libCubyzig.so"
fi

# libCubyzig.so links against the NDK's shared libc++ (build.zig links
# `c++_shared`, not the static `c++`). Nothing on the device provides it, so it
# has to ship inside the APK's lib dir — without it the loader fails with
# `dlopen failed: library "libc++_shared.so" not found` and the app dies on
# launch before any of our code runs.
[ -f "$CXX_SHARED" ] || { echo "missing $CXX_SHARED" >&2; exit 1; }
if [ -x "$STRIP" ]; then
	"$STRIP" --strip-debug -o "$MAIN/jniLibs/arm64-v8a/libc++_shared.so" "$CXX_SHARED"
else
	cp -f "$CXX_SHARED" "$MAIN/jniLibs/arm64-v8a/libc++_shared.so"
fi

echo "[1/6] staged native libs: $(du -sh "$MAIN/jniLibs/arm64-v8a" | cut -f1)"

# ------------------------------------------------------------------- assets
# Music is left out on purpose: 101 MB of .ogg that the audio layer treats as
# optional (it logs and returns null when a track is absent).
if [ "$SKIP_ASSETS" != "1" ]; then
	rm -rf "$MAIN/assets"
	mkdir -p "$MAIN/assets"
	( cd "$ASSETS_DIR" && tar -cf - --exclude='cubyz/music' cubyz ) | ( cd "$MAIN/assets" && tar -xf - )
	echo "[2/6] staged assets: $(du -sh "$MAIN/assets" | cut -f1) (music excluded)"
else
	echo "[2/6] reusing staged assets"
fi

# --------------------------------------------------------------------------
# The engine is named "Cctes Beta" but the on-disk asset tree, mod folder and
# shader paths are all keyed to "cubyz", so only the display string changes.
rm -rf "$BUILD"
mkdir -p "$BUILD/res" "$BUILD/gen" "$BUILD/classes" "$BUILD/dex"

# ------------------------------------------------------------------ resources
"$BT/aapt2" compile --dir "$MAIN/res" -o "$BUILD/res/resources.zip"
"$BT/aapt2" link \
	-o "$BUILD/base.apk" \
	-I "$PLATFORM" \
	--manifest "$MAIN/AndroidManifest.xml" \
	-R "$BUILD/res/resources.zip" \
	--auto-add-overlay \
	-A "$MAIN/assets" \
	--java "$BUILD/gen" \
	--min-sdk-version 28 \
	--target-sdk-version 34 \
	--version-code 1 --version-name "0.5.0-beta"
echo "[3/6] linked resources + assets"

# ----------------------------------------------------------------------- java
javac -source 17 -target 17 -encoding UTF-8 \
	-classpath "$PLATFORM" \
	-d "$BUILD/classes" \
	$(find "$MAIN/java" "$BUILD/gen" -name '*.java')
echo "[4/6] compiled java"

"$BT/d8" --min-api 28 --lib "$PLATFORM" --output "$BUILD/dex" \
	$(find "$BUILD/classes" -name '*.class')
echo "[5/6] dexed"

# --------------------------------------------------------------------- package
# Overlay the native lib (and the dex) onto the resource apk aapt2 produced.
#
# Start from aapt2's apk verbatim instead of unpacking and re-zipping: a plain
# `zip -r` would re-compress resources.arsc, and Android 11+ (targetSdk 30+)
# rejects an APK whose resources.arsc is DEFLATED with
# INSTALL_PARSE_FAILED_UNEXPECTED_EXCEPTION. cp preserves aapt2's STORED entry;
# `zip -0` adds the two new members without compressing them either.
cp -f "$BUILD/base.apk" "$BUILD/unsigned.apk"
STAGE="$BUILD/stage"
rm -rf "$STAGE"; mkdir -p "$STAGE/lib"
cp -R "$MAIN/jniLibs/arm64-v8a" "$STAGE/lib/arm64-v8a"
cp -f "$BUILD/dex/classes.dex" "$STAGE/classes.dex"
( cd "$STAGE" && zip -q0r "$BUILD/unsigned.apk" lib classes.dex )

"$BT/zipalign" -f -p 4 "$BUILD/unsigned.apk" "$BUILD/aligned.apk"

if [ ! -f "$KEYSTORE" ]; then
	keytool -genkeypair -keystore "$KEYSTORE" -storepass cctesbeta -keypass cctesbeta \
		-alias cctes -keyalg RSA -keysize 2048 -validity 10000 \
		-dname "CN=Cctes Beta, OU=Cctes, O=Cctes, L=., S=., C=TR" >/dev/null 2>&1
	echo "      generated debug keystore"
fi

"$BT/apksigner" sign --ks "$KEYSTORE" --ks-pass pass:cctesbeta --key-pass pass:cctesbeta \
	--ks-key-alias cctes --v1-signing-enabled true --v2-signing-enabled true \
	--out "$OUT_APK" "$BUILD/aligned.apk"
"$BT/apksigner" verify --print-certs "$OUT_APK" | head -3
echo "[6/6] signed -> $OUT_APK"
ls -lh "$OUT_APK"
