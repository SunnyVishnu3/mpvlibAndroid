# Third-Party Notices

The build scripts download and combine third-party projects into the Android
AAR. Each project remains subject to its own license. This notice is a summary,
not a replacement for the license files distributed by those projects.

| Component | Source | License family |
|-----------|--------|----------------|
| mpv | https://github.com/mpv-player/mpv | GPL-2.0-or-later / LGPL-2.1-or-later, depending on build configuration |
| FFmpeg | https://github.com/FFmpeg/FFmpeg | LGPL-2.1-or-later / GPL-2.0-or-later; this build enables GPL and version 3 features |
| libbluray / libudfread | https://code.videolan.org/videolan/libbluray | LGPL-2.1-or-later |
| GNU libiconv | https://www.gnu.org/software/libiconv/ | LGPL-2.1-or-later and GPL utility code |
| uchardet | https://gitlab.freedesktop.org/uchardet/uchardet | MPL/GPL/LGPL tri-license provenance |
| libarchive | https://github.com/libarchive/libarchive | BSD-style |
| bzip2 | https://sourceware.org/bzip2/ | bzip2 license |
| xz / liblzma | https://github.com/tukaani-project/xz | 0BSD and public-domain components |
| zstd | https://github.com/facebook/zstd | BSD-3-Clause / GPL-2.0-only dual license |
| libdvdread | https://code.videolan.org/videolan/libdvdread | GPL-2.0-or-later |
| libdvdnav | https://code.videolan.org/videolan/libdvdnav | GPL-2.0-or-later |
| Rubber Band | https://github.com/breakfastquay/rubberband | GPL-2.0-or-later or commercial license |

The build disables libdvdcss, Blu-ray Java, AACS, and BD+ support. Playing
encrypted optical media requires separately supplied software and may be
restricted by local law.

Before distributing an AAR, retain the exact license and copyright files from
the downloaded source versions and provide corresponding source where required.
