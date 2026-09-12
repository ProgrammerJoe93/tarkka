#!/usr/bin/env bash
#
# Builds Tarkka for the desktop or for Android, and optionally runs it.
#
# The Android path needs more setting up than a plain cmake invocation: a
# per-ABI Qt kit, a JDK the Gradle plugin accepts, an NDK, and a signing step
# because the release APK comes out unsigned and cannot be installed. This
# script encodes those so neither you nor CI has to remember them.
set -euo pipefail

readonly PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
readonly APP_ID="fi.tarkka.app"
readonly ACTIVITY="org.qtproject.qt.android.bindings.QtActivity"

platform="desktop"
build_type="Release"
arches=()
run_app=false
run_tests=false
clean=false
sign=true
jobs=""
build_dir=""

die()  { printf 'error: %s\n' "$*" >&2; exit 1; }
note() { printf '\033[1;34m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;33mwarning:\033[0m %s\n' "$*" >&2; }

usage() {
    cat <<'USAGE'
Usage: tools/build.sh [options]

  -p, --platform <desktop|android>  Target platform (default: desktop)
  -a, --arch <arch>                 Target architecture. Repeatable, or comma
                                    separated. Android accepts arm64-v8a,
                                    armeabi-v7a, x86, x86_64 or all
                                    (default: arm64-v8a). Desktop builds
                                    native only.
  -t, --build-type <type>           Debug or Release (default: Release)
  -r, --run                         Run after building. On Android this
                                    installs to the attached device and
                                    launches it.
      --test                        Run the test suite (desktop only)
  -c, --clean                       Delete the build directory first
  -j, --jobs <n>                    Parallel build jobs
      --build-dir <path>            Override the build directory
      --no-sign                     Leave the APK unsigned (not installable)
      --qt-root <path>              Qt installation prefix for Android kits
                                    (default: $QT_ROOT, else newest in ~/Qt)
  -h, --help                        Show this help

Examples:
  tools/build.sh                                  # desktop release
  tools/build.sh --test                           # desktop, then run tests
  tools/build.sh -r                               # desktop, then launch
  tools/build.sh -p android -a arm64-v8a,x86_64   # a phone and an emulator
  tools/build.sh -p android -a x86_64 -r          # build, install, launch
USAGE
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -p|--platform)   platform="${2:-}"; shift 2 ;;
        -a|--arch)       IFS=',' read -ra parts <<< "${2:-}"
                         arches+=("${parts[@]}"); shift 2 ;;
        -t|--build-type) build_type="${2:-}"; shift 2 ;;
        -r|--run)        run_app=true; shift ;;
        --test)          run_tests=true; shift ;;
        -c|--clean)      clean=true; shift ;;
        -j|--jobs)       jobs="${2:-}"; shift 2 ;;
        --build-dir)     build_dir="${2:-}"; shift 2 ;;
        --no-sign)       sign=false; shift ;;
        --qt-root)       QT_ROOT="${2:-}"; shift 2 ;;
        -h|--help)       usage; exit 0 ;;
        *)               usage >&2; die "unknown option: $1" ;;
    esac
done

case "$platform" in
    desktop|android) ;;
    *) die "platform must be desktop or android, not '$platform'" ;;
esac
case "$build_type" in
    Debug|Release|RelWithDebInfo|MinSizeRel) ;;
    *) die "build type must be Debug or Release, not '$build_type'" ;;
esac

build_args=()
[[ -n "$jobs" ]] && build_args+=(--parallel "$jobs")

# ---------------------------------------------------------------- desktop ---

build_desktop() {
    if [[ ${#arches[@]} -gt 0 ]]; then
        local host; host="$(uname -m)"
        for arch in "${arches[@]}"; do
            [[ "$arch" == "native" || "$arch" == "$host" ]] ||
                die "desktop builds are native only; this host is $host, not '$arch'"
        done
    fi

    local dir="${build_dir:-$PROJECT_ROOT/build}"
    $clean && { note "Removing $dir"; rm -rf "$dir"; }

    note "Configuring desktop ($build_type)"
    cmake -S "$PROJECT_ROOT" -B "$dir" -G Ninja -DCMAKE_BUILD_TYPE="$build_type"

    note "Building"
    cmake --build "$dir" "${build_args[@]}"

    if $run_tests; then
        note "Running tests"
        ctest --test-dir "$dir" --output-on-failure
    fi

    if $run_app; then
        note "Launching"
        exec "$dir/tarkka"
    fi
    note "Built $dir/tarkka"
}

# ---------------------------------------------------------------- android ---

# Qt names its kits differently from the ABIs the NDK and Gradle use.
kit_for_abi() {
    case "$1" in
        arm64-v8a)   echo android_arm64_v8a ;;
        armeabi-v7a) echo android_armv7 ;;
        x86)         echo android_x86 ;;
        x86_64)      echo android_x86_64 ;;
        *) die "unknown Android ABI '$1' (expected arm64-v8a, armeabi-v7a, x86 or x86_64)" ;;
    esac
}

find_qt_root() {
    if [[ -n "${QT_ROOT:-}" ]]; then
        echo "$QT_ROOT"; return
    fi
    local newest
    newest="$(find "$HOME/Qt" -maxdepth 1 -type d -name '6.*' 2>/dev/null | sort -V | tail -1)"
    [[ -n "$newest" ]] || die "no Qt installation found; pass --qt-root or set QT_ROOT"
    echo "$newest"
}

# The Gradle plugin Qt generates rejects very new JDKs, so prefer a known-good
# one over whatever happens to be default.
find_jdk() {
    if [[ -n "${JAVA_HOME:-}" && -x "${JAVA_HOME}/bin/javac" ]]; then
        echo "$JAVA_HOME"; return
    fi
    local candidate
    for version in 21 17; do
        candidate="/usr/lib/jvm/java-${version}-openjdk"
        [[ -x "$candidate/bin/javac" ]] && { echo "$candidate"; return; }
    done
    die "no JDK 21 or 17 found; set JAVA_HOME to one the Android Gradle plugin accepts"
}

find_sdk() {
    local candidate
    for candidate in "${ANDROID_SDK_ROOT:-}" "${ANDROID_HOME:-}" "$HOME/Android/Sdk"; do
        [[ -n "$candidate" && -d "$candidate/platforms" ]] && { echo "$candidate"; return; }
    done
    die "no Android SDK found; set ANDROID_SDK_ROOT to one with platforms/ and build-tools/"
}

find_ndk() {
    local sdk="$1" candidate
    if [[ -n "${ANDROID_NDK_ROOT:-}" && -d "${ANDROID_NDK_ROOT}" ]]; then
        echo "$ANDROID_NDK_ROOT"; return
    fi
    candidate="$(find "$sdk/ndk" -maxdepth 1 -mindepth 1 -type d 2>/dev/null | sort -V | tail -1)"
    [[ -n "$candidate" ]] || die "no NDK found under $sdk/ndk; set ANDROID_NDK_ROOT"
    echo "$candidate"
}

newest_build_tools() {
    local found
    found="$(find "$1/build-tools" -maxdepth 1 -mindepth 1 -type d 2>/dev/null | sort -V | tail -1)"
    [[ -n "$found" ]] || die "no build-tools under $1"
    echo "$found"
}

build_android() {
    [[ ${#arches[@]} -gt 0 ]] || arches=(arm64-v8a)
    if [[ " ${arches[*]} " == *" all "* ]]; then
        arches=(arm64-v8a armeabi-v7a x86 x86_64)
    fi

    if $run_app; then
        command -v adb >/dev/null || die "adb is not on PATH"
        [[ -n "$(adb devices | awk 'NR>1 && $2=="device"')" ]] ||
            die "no device or emulator attached; start one before using --run"
    fi

    local qt_root sdk ndk jdk
    qt_root="$(find_qt_root)"
    sdk="$(find_sdk)"
    ndk="$(find_ndk "$sdk")"
    jdk="$(find_jdk)"

    # The kit supplying qt-cmake must be one of the requested ABIs; the rest
    # are built from the same invocation via QT_ANDROID_ABIS.
    local primary_kit qt_cmake abi_list
    primary_kit="$(kit_for_abi "${arches[0]}")"
    qt_cmake="$qt_root/$primary_kit/bin/qt-cmake"
    [[ -x "$qt_cmake" ]] ||
        die "no Qt for Android kit at $qt_cmake; install the ${arches[0]} kit"
    for abi in "${arches[@]}"; do kit_for_abi "$abi" >/dev/null; done
    abi_list="$(IFS=';'; echo "${arches[*]}")"

    local dir="${build_dir:-$PROJECT_ROOT/build-android}"
    $clean && { note "Removing $dir"; rm -rf "$dir"; }

    note "Qt      $qt_root"
    note "SDK     $sdk"
    note "NDK     $ndk"
    note "JDK     $jdk"
    note "ABIs    ${arches[*]}"

    export JAVA_HOME="$jdk"
    export ANDROID_SDK_ROOT="$sdk"
    export ANDROID_NDK_ROOT="$ndk"
    export PATH="$jdk/bin:$PATH"

    note "Configuring Android ($build_type)"
    "$qt_cmake" -S "$PROJECT_ROOT" -B "$dir" -G Ninja \
        -DCMAKE_BUILD_TYPE="$build_type" \
        -DQT_ANDROID_ABIS="$abi_list" \
        -DBUILD_TESTING=OFF

    note "Building APK"
    cmake --build "$dir" --target apk "${build_args[@]}"

    local lower unsigned apk
    lower="$(echo "$build_type" | tr '[:upper:]' '[:lower:]')"
    unsigned="$dir/android-build/build/outputs/apk/$lower/android-build-${lower}-unsigned.apk"
    apk="$dir/tarkka-${lower}.apk"

    if [[ ! -f "$unsigned" ]]; then
        # Debug builds are signed by Gradle already.
        unsigned="$(find "$dir/android-build/build/outputs/apk" -name '*.apk' | head -1)"
        [[ -n "$unsigned" ]] || die "no APK produced under $dir"
    fi

    if $sign; then
        local tools keystore
        tools="$(newest_build_tools "$sdk")"
        keystore="$HOME/.android/debug.keystore"
        if [[ -f "$keystore" ]]; then
            note "Signing with the debug key"
            "$tools/zipalign" -f -p 4 "$unsigned" "$dir/aligned.apk"
            "$tools/apksigner" sign --ks "$keystore" \
                --ks-pass pass:android --key-pass pass:android \
                --ks-key-alias androiddebugkey --out "$apk" "$dir/aligned.apk"
            rm -f "$dir/aligned.apk"
        else
            warn "no debug keystore at $keystore; leaving the APK unsigned"
            cp "$unsigned" "$apk"
        fi
    else
        cp "$unsigned" "$apk"
        warn "APK is unsigned and cannot be installed"
    fi

    note "Built $apk"

    if $run_app; then
        local device_abi; device_abi="$(adb shell getprop ro.product.cpu.abi | tr -d '\r')"
        [[ " ${arches[*]} " == *" $device_abi "* ]] ||
            warn "the device is $device_abi but this APK has ${arches[*]}; it may not start"

        note "Installing"
        adb install -r "$apk"
        note "Launching"
        adb shell am start -n "$APP_ID/$ACTIVITY"
    fi
}

case "$platform" in
    desktop) build_desktop ;;
    android)
        $run_tests && die "the test suite is desktop only; drop --test or use -p desktop"
        build_android ;;
esac
