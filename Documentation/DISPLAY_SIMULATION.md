# Display Simulation

How PixelDisplay Pro reconstructs an image as real display hardware — the core of
the plugin. Everything is **analytic** (signed-distance fields), never bitmap
textures, so it is resolution-independent with clean anti-aliasing.

## The virtual grid

`Engine/DisplayLayouts/Grid.hpp` maps each output pixel to a display **cell**:

```
g       = rotate(pixel - center, gridRot) + center + gridOffset
cell    = floor(g / pitch)                 // pitch = pixelSize * resolutionScale
local   = g/pitch - (cell + 0.5)           // cell-local coord in [-0.5, 0.5]
```

The cell is driven by the **source signal resampled at the cell centroid**
(`Engine/Sampling/Resampler.hpp`, bilinear), converted to scene-linear. Optional
seeded per-cell jitter (position + brightness) models panel irregularity.

## Emitter footprint (whole-pixel types)

Square / Rounded Square / Circular / Diamond / Hexagonal / GameBoy are single
full-colour emitters. Coverage comes from an SDF evaluated at `local` with
analytic anti-aliasing:

```
coverage(sdf, aa) = 1 - smoothstep(-aa, +aa, sdf)   // aa ≈ 0.5/pitch + softness
```

## Subpixel decomposition (RGB panel types)

`Engine/DisplayLayouts/LayoutModel.hpp` splits a cell into channel-specific
subpixels; each emits **only its own input channel**, which is what makes this a
true display simulation rather than a coloured-dot overlay:

| Family | Layouts | Structure |
|--------|---------|-----------|
| Vertical stripe | LCD RGB/BGR, OLED, MiniLED, MicroLED, Retina, Studio, MacBook, DS | 3 rounded-box stripes R/G/B (or B/G/R) |
| Aperture grille | Trinitron | 3 stripes, full height (no vertical gap) |
| Dot triad | CRT shadow mask, LED billboard, RGB matrix | 3 circles; shadow mask offsets alternate rows (hex packing) |
| PenTile | PenTile OLED | green every cell; red/blue alternate between cells |
| Diamond PenTile | Samsung AMOLED, Diamond OLED | green diamonds + larger R/B diamonds, alternating |

Per subpixel: `emit[channel] += pow(signal[channel], spGamma) * scale[channel] *
coverage`, then × subpixel brightness. Controls: size, gap, softness, brightness,
per-channel gamma & scale, RGB/BGR ordering.

## Math (`Engine/Math/`)

- `Vec.hpp` — `Vec2/3/4`, `Mat3` (colour primaries), `saturate`, `lerp`,
  `smoothstepf`, `rotate`.
- `Sdf.hpp` — `sdfBox`, `sdfRoundedBox`, `sdfCircle`, `sdfHexagon`, and
  `coverage()` (analytic AA). SDFs are `< 0` inside, `> 0` outside.

These helpers are the single source of truth; the GPU shaders mirror them
function-for-function (DESIGN.md §7.4).

## Adding a layout

See [EXTENSION_GUIDE.md](EXTENSION_GUIDE.md).
