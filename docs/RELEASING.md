# Releasing the Nuke plug-in

Nuke plug-ins are binary-compatible with the NDK version they are built
against. Release binaries are therefore built on a self-hosted macOS runner
with the matching licensed Nuke installation; the NDK is not copied into
GitHub artifacts.

## Runner setup

Register an Apple silicon GitHub Actions runner with these labels:

```text
self-hosted
macOS
ARM64
nuke-sdk
```

Set three repository-level Actions variables:

| Variable | Example | Purpose |
|---|---|---|
| `NUKE_EXECUTABLE` | `/Applications/Nuke17.0v3/Nuke17.0v3.app/Contents/MacOS/Nuke17.0` | Nuke executable used for the load test |
| `NUKE_SDK_ROOT` | `/Applications/Nuke17.0v3/Nuke17.0v3.app/Contents/MacOS` | Directory containing `cmake/NukeConfig.cmake` and `include/DDImage` |
| `NUKE_VERSION` | `17.0` | Version written into the archive name |

The runner must provide CMake, Apple Clang, Git, and the tools installed with
the GitHub Actions runner. A licensed Nuke or NukeX installation is required to
load and verify the resulting plug-in.

## Local build

```bash
cmake -S . -B build/nuke \
    -DCMAKE_BUILD_TYPE=Release \
    -DNTD_BUILD_EXAMPLES=OFF \
    -DNTD_BUILD_NUKE_PLUGIN=ON \
    -DNTD_BUILD_TESTS=OFF \
    -DNUKE_SDK_ROOT=/path/to/nuke
cmake --build build/nuke --config Release --parallel
cmake --install build/nuke --config Release --prefix dist
```

The install step creates a `GinzburgTemporalDenoiser` directory that can be
placed under `~/.nuke` or another directory on `NUKE_PATH`.

## Automated release

Push an annotated semantic-version tag:

```bash
git tag -a v0.1.0 -m "Release v0.1.0"
git push origin v0.1.0
```

The release workflow:

1. Builds and tests the dependency-free core on a GitHub-hosted runner.
2. Builds the Nuke module against the configured SDK on the self-hosted runner.
3. Starts Nuke in terminal mode and verifies that it can create the node.
4. Installs the plug-in, README, and license into a versioned ZIP archive.
5. Generates a SHA-256 checksum.
6. Creates or updates the matching GitHub Release.

Do not create a release tag until the runner has loaded the plug-in in the
configured Nuke version at least once.
