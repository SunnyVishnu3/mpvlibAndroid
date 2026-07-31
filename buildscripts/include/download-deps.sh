#!/bin/bash -e

set -o pipefail

. ./include/depinfo.sh

[ -z "$IN_CI" ] && IN_CI=0
[ -z "$WGET" ] && WGET=wget

mkdir -p deps && cd deps

download_extract() {
	local destination=$1
	local url=$2
	local tar_mode=$3
	local temporary="${destination}.tmp.$$"

	rm -rf "$temporary"
	mkdir "$temporary"
	if ! $WGET "$url" -O - | tar "$tar_mode" -C "$temporary" --strip-components=1; then
		rm -rf "$temporary"
		return 1
	fi
	mv "$temporary" "$destination"
}

# mbedtls - use git clone with correct directory structure
if [ ! -d mbedtls ]; then
	git clone --depth 1 --branch mbedtls-$v_mbedtls https://github.com/Mbed-TLS/mbedtls.git mbedtls-tmp
	mv mbedtls-tmp mbedtls
	# Initialize submodules (required for config.py and build scripts)
	git -C mbedtls submodule update --init --recursive
fi

# dav1d (canonical repo, GitHub is read-only mirror)
[ ! -d dav1d ] && git clone https://github.com/videolan/dav1d

# ffmpeg (using FongMI fork which has av_mediacodec_get_buffer_timestamp)
if [ ! -d ffmpeg ]; then
	git clone --branch release-8.1-fongmi https://github.com/FongMi/FFmpeg.git ffmpeg
fi


# freetype2
if [ ! -d freetype2 ]; then
	download_extract freetype2 \
		https://downloads.sourceforge.net/freetype/freetype-$v_freetype.tar.gz -xz || \
	download_extract freetype2 \
		https://download.savannah.gnu.org/releases/freetype/freetype-$v_freetype.tar.gz -xz
fi

# libaribcaption
if [ ! -d libaribcaption ]; then
	download_extract libaribcaption \
		https://github.com/xqq/libaribcaption/archive/refs/tags/v${v_libaribcaption}.tar.gz -xz
fi

# libxml2
if [ ! -d libxml2 ]; then
	download_extract libxml2 \
		https://gitlab.gnome.org/GNOME/libxml2/-/archive/v${v_libxml2}/libxml2-v${v_libxml2}.tar.gz -xz
fi

# fribidi - use vX.Y.Z tag format for releases
if [ ! -d fribidi ]; then
	download_extract fribidi \
		https://github.com/fribidi/fribidi/releases/download/v$v_fribidi/fribidi-$v_fribidi.tar.xz -xJ
fi

# harfbuzz
if [ ! -d harfbuzz ]; then
	download_extract harfbuzz \
		https://github.com/harfbuzz/harfbuzz/releases/download/$v_harfbuzz/harfbuzz-$v_harfbuzz.tar.xz -xJ
fi

# unibreak
if [ ! -d unibreak ]; then
	download_extract unibreak \
		https://github.com/adah1972/libunibreak/releases/download/libunibreak_${v_unibreak//./_}/libunibreak-${v_unibreak}.tar.gz -xz
fi

# libass - use GitHub mirror
[ ! -d libass ] && git clone https://github.com/libass/libass

# lua - use 5.2.x (mpv requires < 5.3)
if [ ! -d lua ]; then
	download_extract lua \
		https://www.lua.org/ftp/lua-$v_lua.tar.gz -xz
fi

# mujs
if [ ! -d mujs ]; then
	download_extract mujs \
		https://mujs.com/downloads/mujs-$v_mujs.tar.gz -xz
fi

# openssl
if [ ! -d openssl ]; then
	download_extract openssl \
		https://github.com/openssl/openssl/releases/download/openssl-$v_openssl/openssl-$v_openssl.tar.gz -xz
fi

# libbluray
if [ ! -d libbluray ]; then
	download_extract libbluray \
		https://downloads.videolan.org/pub/videolan/libbluray/${v_libbluray}/libbluray-${v_libbluray}.tar.xz -xJ
fi

# libiconv
if [ ! -d libiconv ]; then
	download_extract libiconv \
		https://ftp.gnu.org/pub/gnu/libiconv/libiconv-${v_libiconv}.tar.gz -xz
fi

# uchardet
if [ ! -d uchardet ]; then
	download_extract uchardet \
		https://gitlab.freedesktop.org/uchardet/uchardet/-/archive/v${v_uchardet}/uchardet-v${v_uchardet}.tar.gz -xz
fi

# bzip2
if [ ! -d bzip2 ]; then
	download_extract bzip2 \
		https://fossies.org/linux/misc/bzip2-${v_bzip2}.tar.gz -xz
fi

# xz
if [ ! -d xz ]; then
	download_extract xz \
		https://github.com/tukaani-project/xz/releases/download/v${v_xz}/xz-${v_xz}.tar.xz -xJ
fi

# zstd
if [ ! -d zstd ]; then
	download_extract zstd \
		https://github.com/facebook/zstd/releases/download/v${v_zstd}/zstd-${v_zstd}.tar.gz -xz
fi

# libarchive
if [ ! -d libarchive ]; then
	download_extract libarchive \
		https://github.com/libarchive/libarchive/releases/download/v${v_libarchive}/libarchive-${v_libarchive}.tar.xz -xJ
fi

# libdvdread
if [ ! -d libdvdread ]; then
	download_extract libdvdread \
		https://downloads.videolan.org/pub/videolan/libdvdread/${v_libdvdread}/libdvdread-${v_libdvdread}.tar.xz -xJ
fi

# libdvdnav
if [ ! -d libdvdnav ]; then
	download_extract libdvdnav \
		https://downloads.videolan.org/pub/videolan/libdvdnav/${v_libdvdnav}/libdvdnav-${v_libdvdnav}.tar.xz -xJ
fi

# rubberband
if [ ! -d rubberband ]; then
	download_extract rubberband \
		https://github.com/breakfastquay/rubberband/archive/refs/tags/v${v_rubberband}.tar.gz -xz
fi

# curl
if [ ! -d curl ]; then
	download_extract curl \
		https://curl.se/download/curl-$v_curl.tar.gz -xz
fi

# shaderc
mkdir -p shaderc
cat >shaderc/README <<'HEREDOC'
shaderc sources are provided by the NDK
see <ndk>/sources/third_party/shaderc
HEREDOC

# libplacebo
if [ ! -d libplacebo ]; then
	if [ "$IN_CI" -eq 1 ]; then
		: "${LIBPLACEBO_GIT_COMMIT:?LIBPLACEBO_GIT_COMMIT must be set in CI}"
		clone_ci_commit \
			"${LIBPLACEBO_GIT_URL:-https://github.com/FongMi/libplacebo.git}" \
			"$LIBPLACEBO_GIT_COMMIT" libplacebo recursive
	else
		git clone --depth 1 --recursive --branch "$v_ci_libplacebo" \
			"${LIBPLACEBO_GIT_URL:-https://github.com/FongMi/libplacebo.git}" libplacebo
	fi
fi
# mpv
[ ! -d mpv ] && git clone -b fongmi https://github.com/FongMi/mpv.git
if ! git -C mpv apply --reverse --check ../../patches/mpv_video_shaders.patch 2>/dev/null; then
	git -C mpv apply ../../patches/mpv_video_shaders.patch
fi

cd ..
