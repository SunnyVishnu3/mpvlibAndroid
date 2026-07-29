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

clone_ci_commit() {
	local repository=$1
	local expected_commit=$2
	local directory=$3
	local clone_mode=${4:-}

	if ! (
		set -e
		git init -q "$directory"
		git -C "$directory" remote add origin "$repository"
		git -C "$directory" fetch -q --depth=1 origin "$expected_commit"
		git -C "$directory" checkout -q --detach FETCH_HEAD
		if [[ "$clone_mode" == recursive ]]; then
			git -C "$directory" submodule update -q \
				--init --recursive --depth=1
		fi
		[[ $(git -C "$directory" rev-parse --verify 'HEAD^{commit}') == \
			"$expected_commit" ]]
	); then
		echo "Failed to check out $repository commit $expected_commit." >&2
		rm -rf "$directory"
		return 1
	fi
}

# mbedtls - use git clone with correct directory structure
if [ ! -d mbedtls ]; then
	git clone --depth 1 --branch mbedtls-$v_mbedtls https://github.com/Mbed-TLS/mbedtls.git mbedtls-tmp
	mv mbedtls-tmp mbedtls
	# Initialize submodules (required for config.py and build scripts)
	git -C mbedtls submodule update --init --recursive
fi

# dav1d
if [ ! -d dav1d ]; then
	if [ "$IN_CI" -eq 1 ]; then
		: "${DAV1D_GIT_COMMIT:?DAV1D_GIT_COMMIT must be set in CI}"
		clone_ci_commit \
			"${DAV1D_GIT_URL:-https://github.com/videolan/dav1d}" \
			"$DAV1D_GIT_COMMIT" dav1d
	else
		git clone --branch "$v_ci_dav1d" \
			"${DAV1D_GIT_URL:-https://github.com/videolan/dav1d}" dav1d
	fi
fi

# ffmpeg (using FongMI fork which has av_mediacodec_get_buffer_timestamp)
if [ ! -d ffmpeg ]; then
	if [ "$IN_CI" -eq 1 ]; then
		: "${FFMPEG_GIT_COMMIT:?FFMPEG_GIT_COMMIT must be set in CI}"
		clone_ci_commit \
			"${FFMPEG_GIT_URL:-https://github.com/FongMi/FFmpeg.git}" \
			"$FFMPEG_GIT_COMMIT" ffmpeg
	else
		git clone --branch "$v_ci_ffmpeg" \
			"${FFMPEG_GIT_URL:-https://github.com/FongMi/FFmpeg.git}" ffmpeg
	fi
fi


# freetype2
if [ ! -d freetype2 ]; then
	mkdir freetype2
	$WGET https://download.savannah.gnu.org/releases/freetype/freetype-$v_freetype.tar.gz -O - | \
		tar -xz -C freetype2 --strip-components=1
fi

# libaribcaption
if [ ! -d libaribcaption ]; then
	mkdir libaribcaption
	$WGET https://github.com/xqq/libaribcaption/archive/refs/tags/v${v_libaribcaption}.tar.gz -O - | \
		tar -xz -C libaribcaption --strip-components=1
fi

# libxml2
if [ ! -d libxml2 ]; then
	mkdir libxml2
	$WGET https://gitlab.gnome.org/GNOME/libxml2/-/archive/v${v_libxml2}/libxml2-v${v_libxml2}.tar.gz -O - | \
		tar -xz -C libxml2 --strip-components=1
fi

# fribidi - use vX.Y.Z tag format for releases
if [ ! -d fribidi ]; then
	mkdir fribidi
	$WGET https://github.com/fribidi/fribidi/releases/download/v$v_fribidi/fribidi-$v_fribidi.tar.xz -O - | \
		tar -xJ -C fribidi --strip-components=1
fi

# harfbuzz
if [ ! -d harfbuzz ]; then
	mkdir harfbuzz
	$WGET https://github.com/harfbuzz/harfbuzz/releases/download/$v_harfbuzz/harfbuzz-$v_harfbuzz.tar.xz -O - | \
		tar -xJ -C harfbuzz --strip-components=1
fi

# unibreak
if [ ! -d unibreak ]; then
	mkdir unibreak
	$WGET https://github.com/adah1972/libunibreak/releases/download/libunibreak_${v_unibreak//./_}/libunibreak-${v_unibreak}.tar.gz -O - | \
		tar -xz -C unibreak --strip-components=1
fi

# libass
if [ ! -d libass ]; then
	if [ "$IN_CI" -eq 1 ]; then
		: "${LIBASS_GIT_COMMIT:?LIBASS_GIT_COMMIT must be set in CI}"
		clone_ci_commit \
			"${LIBASS_GIT_URL:-https://github.com/libass/libass}" \
			"$LIBASS_GIT_COMMIT" libass
	else
		git clone --branch "$v_ci_libass" \
			"${LIBASS_GIT_URL:-https://github.com/libass/libass}" libass
	fi
fi

# lua - use 5.2.x (mpv requires < 5.3)
if [ ! -d lua ]; then
	mkdir lua
	$WGET https://www.lua.org/ftp/lua-$v_lua.tar.gz -O - | \
		tar -xz -C lua --strip-components=1
fi

# mujs
if [ ! -d mujs ]; then
	mkdir mujs
	$WGET https://mujs.com/downloads/mujs-$v_mujs.tar.gz -O - | \
		tar -xz -C mujs --strip-components=1
fi

# openssl
if [ ! -d openssl ]; then
	mkdir openssl
	$WGET https://github.com/openssl/openssl/releases/download/openssl-$v_openssl/openssl-$v_openssl.tar.gz -O - | \
		tar -xz -C openssl --strip-components=1
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
	mkdir curl
	$WGET https://curl.se/download/curl-$v_curl.tar.gz -O - | \
		tar -xz -C curl --strip-components=1
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
			"${LIBPLACEBO_GIT_URL:-https://github.com/haasn/libplacebo.git}" \
			"$LIBPLACEBO_GIT_COMMIT" libplacebo recursive
	else
		git clone --recursive --branch "$v_ci_libplacebo" \
			"${LIBPLACEBO_GIT_URL:-https://github.com/haasn/libplacebo.git}" libplacebo
	fi
fi

# mpv
[ ! -d mpv ] && git clone -b fongmi https://github.com/FongMi/mpv.git
if ! git -C mpv apply --reverse --check ../../patches/mpv_video_shaders.patch 2>/dev/null; then
	git -C mpv apply ../../patches/mpv_video_shaders.patch
fi

cd ..
