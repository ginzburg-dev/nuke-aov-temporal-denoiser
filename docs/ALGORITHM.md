# Filter notes

## Guide weight

For reference sample \(r\) and candidate \(c\):

\[
D_g(r,c) =
\frac{\|B_r-B_c\|^2}{\sigma_B^2} +
\frac{\|A_r-A_c\|^2}{\sigma_A^2} +
\frac{\|N_r-N_c\|^2}{\sigma_N^2} +
\frac{\|P_r-P_c\|^2}{\sigma_P^2} +
\frac{(Z_r-Z_c)^2}{\sigma_Z^2}
\]

\(B\), \(A\), \(N\), \(P\), and \(Z\) are beauty, albedo, normal, world
position, and depth. Sigmas are clamped to a positive minimum before the
weights are evaluated.

For spatial pixel distance \(d_s^2\):

\[
w_s = \alpha_s \exp\left[
-\frac{1}{2}\left(D_g + \frac{d_s^2}{\sigma_s^2}\right)
\right]
\]

For temporal search offset \(d_q^2\) and frame offset \(\Delta f\):

\[
w_t = \alpha_t \exp\left[
-\frac{1}{2}\left(D_g + \frac{d_q^2}{\sigma_s^2}\right)
- \lambda(|\Delta f|-1)
\right]
\]

The implementation keeps distances squared and evaluates one exponential per
weight.

## Temporal search

Motion is sampled at each intermediate frame rather than multiplying the
current vector by the frame offset. Per-frame displacement is clamped by
`maximumMotionPerFrame`.

The search checks a square window around the predicted coordinate. Beauty,
albedo, and position thresholds reject invalid candidates before the normalized
guide cost is compared.

Past-frame lookup walks against the motion stored on the target frames. The
sign and units depend on the renderer, so `motion scale` must be checked with a
known translation before using a new vector pass.

## Accumulation

The center pixel starts with weight 1. Spatial and accepted temporal samples
are added to one accumulator. Beauty and the four extra RGB passes share the
same total weight.

Non-finite guide samples get zero weight. Radii and sigmas are bounded before
rendering. Tile padding is:

\[
\max(r_s,\ r_q + r_t m)
\]

where \(r_s\) is the spatial radius, \(r_q\) the search radius, \(r_t\) the
temporal radius, and \(m\) the declared per-frame motion limit.

## Cost

For each output pixel, the current implementation evaluates:

\[
O((2r_s+1)^2 + 2r_t(2r_q+1)^2)
\]

The search is exhaustive. SIMD guide evaluation or a hierarchical search would
be the first places to optimize after profiling on representative frames.
