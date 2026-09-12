#!/usr/bin/env bash
#
# Starts an Android emulator suitable for looking at the app.
#
# The GPU flag is the point of this script. Booting with the default software
# rasteriser (-gpu swiftshader_indirect) shreds Qt Quick's rounded rectangles:
# buttons and cards come out with a diagonal across them and one triangle of
# the quad unfilled. It looks exactly like an application bug and is not one --
# the identical APK renders perfectly with -gpu host. Anything judging how the
# UI looks must therefore not use the software rasteriser.
set -euo pipefail

readonly SDK="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$HOME/Android/Sdk}}"
readonly EMULATOR="$SDK/emulator/emulator"

die() { printf 'error: %s\n' "$*" >&2; exit 1; }

[[ -x "$EMULATOR" ]] || die "no emulator at $EMULATOR; set ANDROID_SDK_ROOT"

avd="${1:-}"
if [[ -z "$avd" ]]; then
    avd="$("$EMULATOR" -list-avds | head -1)"
    [[ -n "$avd" ]] || die "no AVD found; create one in Android Studio first"
    printf 'Using AVD %s (pass a name to choose another; available:)\n' "$avd"
    "$EMULATOR" -list-avds | sed 's/^/  /'
fi

# swiftshader is the fallback for hosts with no usable GL. It boots, but do not
# trust anything it draws.
gpu="${TARKKA_EMULATOR_GPU:-host}"

printf 'Booting %s with -gpu %s\n' "$avd" "$gpu"
"$EMULATOR" -avd "$avd" -gpu "$gpu" -no-snapshot-load -no-audio -no-boot-anim \
    >/tmp/tarkka-emulator.log 2>&1 &

printf 'Waiting for boot'
until [[ "$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == "1" ]]; do
    printf '.'; sleep 3
done
printf '\nReady. Now: tools/build.sh -p android -a x86_64 --run\n'
