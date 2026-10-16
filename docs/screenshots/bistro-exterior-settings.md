# Bistro Exterior preview

Captured directly from this project renderer on 2026-10-16. No image generation or color retouching. The 1080p version is downsampled from the 4K render.

- Model: `assets/Bistro/BistroExterior.gltf`, identity transform
- HDR: `assets/Bistro/san_giuseppe_bridge_4k.hdr`
- Camera position: `(-16, 4, 10)`
- Camera target: `(0, 7, -6)`
- Camera up: `(0, 1, 0)`
- Vertical FOV: `60 degrees`
- Directional light Euler rotation: `(-32, -35, 0) degrees`
- Directional light color (linear RGB): `(1.0, 0.88, 0.74)`
- Directional light intensity: `2.5`
- Directional shadows: enabled
- SSAO: enabled
- TAA: enabled, 64 frames at a fixed camera
- Tone mapping: ACES
- Exposure: `1.0`
- Bloom intensity: `0.035`, threshold `1.0`, soft knee `0.5`
- Capture resolution: `3840 x 2160`

The capture utility reuses the current Renderer, Model, EnvironmentMap and shader implementations; it omits the editor overlay. Application startup defaults were not changed.

The model and HDR remain third-party assets; keep their original attribution and licensing when publishing this screenshot.
