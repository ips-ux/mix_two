#!/usr/bin/env bash
# AI-generated (Claude Code). Start of AI-generated text.
# Mix Two local Linux build helper (CMake + Ninja). It has the same commands as
# tools/mixtwo_windows.bat. It installs nothing and never uses sudo: install
# the build dependencies of Mixxx first (see CONTRIBUTING.md).
#
#   tools/mixtwo_linux.sh [--dry-run] configure       Configure build/linux__portable
#   tools/mixtwo_linux.sh [--dry-run] build [target]  Build everything or one target
#                                                     (for example mixxx-test)
#   tools/mixtwo_linux.sh [--dry-run] test [gtest args]
#                                                     Run mixxx-test, for example
#                                                     --gtest_filter='*Midi*'
#   tools/mixtwo_linux.sh [--dry-run] run [--nobuild] [mixxx args]
#                                                     Build everything, then run Mix Two
#                                                     with a separate profile
#                                                     (MIXTWO_PROFILE, default
#                                                     build/mixtwo-profile).
#                                                     --nobuild starts the current build.
#   No command: the same as "run".
#
# --dry-run prints the commands and runs none of them. The exit code is 0.
set -eu

MIXXX_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${MIXXX_ROOT}/build/linux__portable"
MIXTWO_PROFILE="${MIXTWO_PROFILE:-${MIXXX_ROOT}/build/mixtwo-profile}"

DRY_RUN=0
if [ "${1:-}" = "--dry-run" ]; then
    DRY_RUN=1
    shift
fi

usage() {
    echo "Usage: $(basename "$0") [--dry-run] configure | build [target] | test [gtest args] | run [--nobuild] [mixxx args]" >&2
}

# Print a command with every word quoted, then run it unless this is a dry run.
run_cmd() {
    local word
    printf '+'
    for word in "$@"; do
        printf ' %q' "${word}"
    done
    printf '\n'
    if [ "${DRY_RUN}" -eq 0 ]; then
        "$@"
    fi
}

# Print an environment setting, then make it unless this is a dry run.
set_env() {
    printf '+ export %s=%q\n' "$1" "$2"
    if [ "${DRY_RUN}" -eq 0 ]; then
        export "$1=$2"
    fi
}

do_configure() {
    run_cmd cmake -S "${MIXXX_ROOT}" -B "${BUILD_DIR}" -G Ninja \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DGIT_DESCRIBE=2.7-mixtwo \
        -DOPTIMIZE=portable \
        -DQT6=ON \
        -DKEYFINDER=OFF \
        -DBATTERY=ON -DBROADCAST=ON -DBULK=ON -DHID=ON \
        -DFFMPEG=ON -DLILV=ON -DMAD=ON -DMODPLUG=ON \
        -DOPUS=ON -DQTKEYCHAIN=ON -DVINYLCONTROL=ON -DWAVPACK=ON \
        -DLOCALECOMPARE=ON -DDEBUG_ASSERTIONS_FATAL=ON \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
}

do_build() {
    # An empty target builds everything.
    if [ "$#" -ge 1 ] && [ -n "$1" ]; then
        run_cmd ninja -C "${BUILD_DIR}" "$1"
    else
        run_cmd ninja -C "${BUILD_DIR}"
    fi
}

do_test() {
    if [ "${DRY_RUN}" -eq 0 ] && [ ! -x "${BUILD_DIR}/mixxx-test" ]; then
        echo "mixxx-test not found. Build it first: $(basename "$0") build mixxx-test" >&2
        return 1
    fi
    set_env QT_QPA_PLATFORM offscreen
    printf '+ cd %q\n' "${BUILD_DIR}"
    if [ "${DRY_RUN}" -eq 0 ]; then
        cd "${BUILD_DIR}"
    fi
    run_cmd ./mixxx-test "$@"
}

do_run() {
    local nobuild=0
    local args=()
    local arg
    for arg in "$@"; do
        if [ "${arg}" = "--nobuild" ]; then
            nobuild=1
        else
            args+=("${arg}")
        fi
    done
    if [ "${nobuild}" -eq 0 ]; then
        echo 'Building Mix Two. Use "run --nobuild" to start the current build instead.'
        do_build
    fi
    if [ "${DRY_RUN}" -eq 0 ] && [ ! -x "${BUILD_DIR}/mixxx" ]; then
        echo "mixxx not found. Build it first: $(basename "$0") build mixxx" >&2
        return 1
    fi
    # Keep Mix Two away from the profile of an installed Mixxx in ~/.mixxx.
    run_cmd mkdir -p "${MIXTWO_PROFILE}"
    echo "Profile: ${MIXTWO_PROFILE}"
    # With "set -u", an empty array needs the ${args[@]+...} form in old bash.
    run_cmd "${BUILD_DIR}/mixxx" --settings-path "${MIXTWO_PROFILE}" ${args[@]+"${args[@]}"}
}

COMMAND="${1:-run}"
if [ "$#" -ge 1 ]; then
    shift
fi

case "${COMMAND}" in
    configure) do_configure ;;
    build) do_build "$@" ;;
    test) do_test "$@" ;;
    run) do_run "$@" ;;
    *)
        usage
        exit 1
        ;;
esac
# End of AI-generated text.
