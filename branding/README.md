# Application brand assets

`source/logo.png` is an unchanged copy of the logo supplied by the owner at `E:/ShadowChat/logo.png` on 2026-10-28. No generated artwork, recoloring, background removal, or redrawing is involved.

Regenerate with `python branding/generate_assets.py`. To archive a replacement input and regenerate, append its path. Requires Pillow; the generated manifest records the version, input checksum, dimensions, and output checksums.

`generated/app.ico` contains 16, 20, 24, 32, 40, 48, 64, 128, and 256 pixel frames. Optimized PNGs cover 16–512 pixels; `shadowchat-512.webp` uses lossless encoding. All variants retain the full composition and original padding. The script also updates the Windows executable icon, installer icon, and existing Qt brand/tray image aliases. User profile images are separate.

Recommended web usage: 32/64 PNG for navigation, 128/256 PNG or 512 WebP for product cards, and `app.ico` as favicon when needed.
