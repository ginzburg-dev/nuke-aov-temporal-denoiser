# Releasing the Nuke plug-in

Nuke plug-ins are binary-compatible with the NDK version they are built
against. The repository therefore publishes source releases rather than a
binary that may be incompatible with the user's Nuke installation.

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

Before distributing a locally built module, start Nuke in terminal mode and
run the included smoke test:

```bash
NUKE_PATH=/path/to/dist/GinzburgTemporalDenoiser \
    /path/to/Nuke -t tests/NukeSmokeTest.py
```

## Source release

Push an annotated semantic-version tag:

```bash
git tag -a v0.1.0 -m "Release v0.1.0"
git push origin v0.1.0
```

The release workflow:

1. Builds and tests the dependency-free core on a GitHub-hosted runner.
2. Creates the matching GitHub Release with generated release notes.
3. Publishes GitHub's standard ZIP and tarball snapshots of the tagged source.

No self-hosted runner or Nuke installation is required for a source release.
The release does not claim to contain a prebuilt or load-tested Nuke module.
