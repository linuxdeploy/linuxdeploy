#!/bin/sh

# mock ldd for replaying ldd calls from the recordings for mock tests
#
# usage: LC_ALL=C ldd <absolute path>
#
# install with writeReplayingLdd() from dependency_tracing_recording.h, which
# copies this script as ldd into a directory and writes the recorded calls into
# the replay directory next to it. Each call replays the next recorded call:
# print the recorded stdout and stderr, and exit with the recorded status
#
# uses only shell builtins, as the tests may set the PATH to nothing
#

set -eu

replay_dir=${0%/*}/replay
called=${1-}

fail() {
    exit_status=$1
    shift
    printf 'mock ldd: %s\n' "$*" >&2
    exit "$exit_status"
}

# print a file unchanged, like cat
print_file() {
    file=$1
    while IFS= read -r line; do
        printf '%s\n' "$line"
    done < "$file"
    # read fails on a last line without a newline, but still sets line
    printf '%s' "$line"
}

[ "${LC_ALL-}" = C ] || fail 90 "LC_ALL must be C, got '${LC_ALL-}'"
case $called in
    /*) ;;
    *) fail 91 "expected an absolute path, got $called" ;;
esac

# the first call that has not been replayed yet
n=0
while [ -e "$replay_dir/$n.replayed" ]; do
    n=$((n + 1))
done
[ -e "$replay_dir/$n.status" ] || fail 92 "unexpected call for $called"
: > "$replay_dir/$n.replayed"

read -r target < "$replay_dir/$n.target"
if [ -n "$target" ] && [ "$called" != "$target" ]; then
    fail 93 "expected a call for $target, got $called"
fi

print_file "$replay_dir/$n.stdout"
print_file "$replay_dir/$n.stderr" >&2
read -r status < "$replay_dir/$n.status"

exit "$status"
