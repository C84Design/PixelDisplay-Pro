# Artifacts & Burn-In

The imperfection layers that make a rendered panel read as a real, physical
screen. All run in scene-linear light and are fully deterministic (coordinate-
derived hashing, no stateful RNG).

## ImperfectionsStage (`Engine/Renderer/Stages/ImperfectionsStage.cpp`)

Applied after synthesis, before optics/encode. Gated as a whole — appended to the
frame graph only when at least one artifact is active.

| Artifact | Model |
|----------|-------|
| Dead pixels | Per-cell hash < `count/cellCount` → forced to `deadColor·brightness`; optional clustering via a coarser hash |
| Stuck pixels | Per-cell hash → fixed R/G/B/White or Random-RGB |
| Hot pixels | Per-cell hash → over-bright white |
| Mura | Signed fBm (`Engine/Noise/Noise.hpp`), mid-frequency blotching |
| Panel uniformity | Low-frequency fBm brightness variation |
| Brightness drift | Linear gradient across the panel |
| Column / row defects | Hashed per grid column/row → darkened |
| Banding | Quantize to `levels = mix(255, 6, banding)` |
| Backlight bleed | Additive cool light near the frame edges |
| Black level | Per-channel emission floor (panel can't show true black) |
| Dust / scratches / hair / fingerprints / pressure | Hash specks, value-noise lines, fBm streaks, fBm smudge haze, fBm discolouration |
| Light leakage | Warm corner leak, `pow(radialNorm, 3)` |
| Vignetting | `1 − amount·smoothstep(0.35, 1, radialNorm)` |

Dead/stuck/hot use the shared `GridMapping`, so defects land exactly on display
cells emitted by the synthesis stage.

## BurnInStage (`Engine/Renderer/Stages/BurnInStage.cpp`, model in `Artifacts/BurnInModel.hpp`)

Closed-form in layer time — **no accumulator** — so it stays MFR-safe
(DESIGN.md §12):

```
burn(x,t) = intensity · aging(age) · wear(x) · growth(t) · (1 − recovery)
```

- **wear(x)** priority: imported grayscale mask → analytic preset regions
  (logo / status bar / window / taskbar) → content-luminance proxy.
- **growth(t)** = baseline(age) + (1 − e^(−0.15·t))·(…): develops as the layer
  plays; a still frame still shows age-based wear.
- Effect: worn emitters dim (differential aging) + a faint persistent ghost
  (raised by `ghosting`) visible even on black. `persistence` /
  `imagePersistence` add a lingering retention trail.

## Verification

`Tests/test_artifacts.cpp` and `test_burnin.cpp`: vignette corners, dead-cell
creation, black-level floor, banding collapse, status-bar ghost on black, time
intensification, custom-mask control, and cross-thread determinism.
