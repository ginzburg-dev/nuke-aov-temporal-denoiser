# AOV-Guided Temporal Denoiser for Nuke

[![CI](https://github.com/ginzburg-dev/nuke-aov-temporal-denoiser/actions/workflows/ci.yml/badge.svg)](https://github.com/ginzburg-dev/nuke-aov-temporal-denoiser/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/ginzburg-dev/nuke-aov-temporal-denoiser)](https://github.com/ginzburg-dev/nuke-aov-temporal-denoiser/releases/latest)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)

C++17 Nuke plug-in for spatial and temporal denoising of Monte Carlo renders.
It combines motion-compensated samples from neighboring frames and uses
renderer AOVs to preserve edges and reject invalid matches.

![Noisy input and denoised output](docs/images/sprite-fright-comparison.jpg)

Left: noisy input. Right: denoised output. Scene:
[Blender 3.0 – Sprite Fright](https://cloud.blender.org/p/gallery/617933e9b7b35ce1e1c01066)
by [Blender Studio](https://studio.blender.org/), CC BY.

## How it works

For each output pixel, the filter:

1. Predicts its position in neighboring frames from the motion AOV.
2. Searches around the prediction for the best match.
3. Rejects matches that disagree in beauty, albedo, or world position.
4. Combines accepted temporal samples with a cross-bilateral spatial filter.
5. Reuses the same weights for beauty and up to four additional RGB passes.

The temporal window covers up to three frames on each side of the current
frame. The filtering core has no Nuke dependency.

## Inputs

| Input | Components | Purpose |
|---|---:|---|
| Beauty | RGB | Filtered image and color guide |
| Motion | XY | Forward motion in pixels |
| Albedo | RGB | Material boundaries and temporal rejection |
| Normal | RGB | Surface orientation |
| Position | RGB | Correspondence and disocclusion rejection |
| Depth | 1 | Depth boundaries |
| Extra 0–3 | RGB | Additional passes filtered with the beauty weights |

`motion scale` converts the selected motion channels to pixel displacement.
`maximum motion / frame` sets the motion clamp and input tile padding.

## Nuke node

![Temporal denoiser node in Foundry Nuke](docs/images/nuke-node-ui.jpg)

## Build

### Core

Requires CMake 3.20 and a C++17 compiler.

```bash
cmake -S . -B build -DNTD_WARNINGS_AS_ERRORS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

### Nuke plug-in

Set `NUKE_SDK_ROOT` to the Nuke directory that contains
`cmake/NukeConfig.cmake` and `include/DDImage`:

```bash
cmake -S . -B build \
    -DNTD_BUILD_NUKE_PLUGIN=ON \
    -DNUKE_SDK_ROOT=/path/to/nuke
cmake --build build --target GinzburgTemporalDenoiser --parallel
cmake --install build --prefix dist
```

Add `dist/GinzburgTemporalDenoiser` to `NUKE_PATH`. The plug-in must be built
for the same Nuke version, platform, and architecture as the host.

## Validation

The test suite covers parameter bounds, spatial weights, temporal rejection,
correspondence search, AOV normalization, and synthetic image regression.

![Synthetic edge regression](docs/synthetic-demo.svg)

On the synthetic moving-edge scene, the default settings reduce RMSE from
`0.1176` to `0.0490`.

Implementation details and filter equations are in
[docs/ALGORITHM.md](docs/ALGORITHM.md).

## Releases

Bumping `VERSION` in `CMakeLists.txt` and merging it into `main` runs the tests,
creates the matching `v*` tag, and publishes a source release. Prebuilt Nuke
modules are not included.

See [docs/RELEASING.md](docs/RELEASING.md) for the release and local packaging
steps.

## License

The source code is licensed under Apache-2.0. The Sprite Fright image is
available under CC BY.
