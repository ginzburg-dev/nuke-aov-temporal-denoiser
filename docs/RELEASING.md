# Releasing the Nuke plug-in

Nuke plug-ins are binary-compatible with the NDK version they are built
against. The repository therefore publishes source releases rather than a
binary that may be incompatible with the user's Nuke installation.

## Local build

```bash
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DNTD_BUILD_EXAMPLES=OFF \
    -DNTD_BUILD_NUKE_PLUGIN=ON \
    -DNTD_BUILD_TESTS=OFF \
    -DNUKE_SDK_ROOT=/path/to/nuke
cmake --build build --config Release --parallel
cmake --install build --config Release --prefix dist
```

The install step creates a `GinzburgTemporalDenoiser` directory that can be
placed under `~/.nuke` or another directory on `NUKE_PATH`.

Before distributing a locally built module, start Nuke in terminal mode and
run the included smoke test:

```bash
NUKE_PATH=/path/to/dist/GinzburgTemporalDenoiser \
    /path/to/Nuke -t tests/NukeSmokeTest.py
```

## Source release

Update the project version in `CMakeLists.txt`:

```cmake
project(
    NukeAovTemporalDenoiser
    VERSION 2.0.1
)
```

Open a pull request with the version change and merge it into `main`. The
release workflow then:

1. Builds and tests the dependency-free core on a GitHub-hosted runner.
2. Creates an annotated tag matching the project version.
3. Creates the GitHub Release with generated release notes.
4. Publishes GitHub's standard ZIP and tarball snapshots of the tagged source.

No self-hosted runner or Nuke installation is required for a source release.
The release does not claim to contain a prebuilt or load-tested Nuke module.
