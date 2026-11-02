# Third-party notices

## Phosphor Icons

The icons under `src/images/shadowchat/icons/` are selected unmodified glyphs from [Phosphor Icons Core](https://github.com/phosphor-icons/core), regular weight.

MIT License

Copyright (c) 2023 Phosphor Icons

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## VideoLAN VLC/libVLC runtime

Windows packages additionally bundle the official, unmodified VLC/libVLC 3.0.24 win32 runtime under `video-runtime/`. ShadowChat uses the embedded libraries and plugins; the standalone VLC player and its Qt GUI are excluded.

The original `COPYING.txt`, `COPYING.LIB`, author notices, `SOURCE-AND-LICENSES.txt`, and `runtime-manifest.json` are retained in that directory. These files describe the applicable VLC/plugin GPL terms, libVLC LGPL 2.1 terms, source locations, and individual runtime hashes; no single license is asserted for every bundled component.

The official binary archive SHA-256 is `8511356afd680817f3aea624c63032d0936f3d77b2175fb36c8fc16adf9744e8`. The corresponding VLC source archive is `https://download.videolan.org/pub/videolan/vlc/3.0.24/vlc-3.0.24.tar.xz`, SHA-256 `e7cab503d1d7d5849b89d2cf0e1ee60d0ef6d012407791b644b9cfc0cc225fdf`. The runtime's source notice and manifest retain the official checksum, library license, and third-party source index references.

The existing application license and all preceding third-party copyright notices remain in place.
