# ldd recordings

The mock tests (`test_dependency_tracing_mock.cpp`) replay recordings from real
tests (`test_dependency_tracing_real.cpp`). One directory per platform with one
file per scenario (`dependency_tracing_scenarios.h`). Paths in the test's
directory are replaced with `/fixture`, and addresses with `0x1000`. The
recordings are made on x86_64, like the CI jobs run, so the paths of the
system's libraries are those of x86_64 (e.g., `/lib64/ld-linux-x86-64.so.2`),
which does not matter, as the mock tests only check the fixture files, and
pass on every architecture.

A scenario which makes no ldd calls has an empty recording. A scenario
which does not apply to a platform (e.g., one for musl's ldd) is skipped
by the real tests there, so there is no recording and the mock tests skip
it for that platform, too.

## Recording a platform

`ci/test-dependency-tracing.sh --record <platform>`, run from the repository
root, builds the real tests, runs them, and replaces the recordings in
`<platform>/`. If the real tests fail, or only some of them run (e.g., with
`$GTEST_FILTER`), it only adds the recordings of the scenarios which ran.

`ci/build-in-docker.sh` runs it in the CI containers if `$BUILD_TYPE` is
`record-ldd-calls`, and names the platform after the container (e.g., `alpine-llvm`).
It passes `$GTEST_FILTER` and the sharding variables on to the container:

```sh
BUILD_TYPE=record-ldd-calls                bash ci/build-in-docker.sh debian
BUILD_TYPE=record-ldd-calls TOOLCHAIN=musl bash ci/build-in-docker.sh alpine
BUILD_TYPE=record-ldd-calls TOOLCHAIN=llvm bash ci/build-in-docker.sh alpine
BUILD_TYPE=record-ldd-calls TOOLCHAIN=llvm bash ci/build-in-docker.sh gentoo
```

There is no container for FreeBSD, so record it on a FreeBSD machine like the
CI job's (15.0 on amd64, e.g., a VM of the official `FreeBSD-15.0-RELEASE-amd64-ufs.qcow2`
image) with the packages the CI job installs from the `latest` package set, and
copy `freebsd/` back. FreeBSD's ldd lists `[vdso] => [vdso]` on amd64, but not
on arm64.

```sh
mkdir -p /usr/local/etc/pkg/repos
sed -e 's|/quarterly|/latest|' /etc/pkg/FreeBSD.conf > /usr/local/etc/pkg/repos/FreeBSD.conf
pkg install -y bash cmake cimg googletest ninja patchelf pkgconf wget
bash ci/test-dependency-tracing.sh --record freebsd
```

A failing real test also prints its recording, so that it can be copied.
