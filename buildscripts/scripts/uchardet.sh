#!/bin/bash -e

. ../../include/path.sh
. ../../include/depinfo.sh
. ../../include/cmake-android.sh

build=_build$ndk_suffix

if [ "$1" == "build" ]; then
	true
elif [ "$1" == "clean" ]; then
	rm -rf "$build"
	exit 0
else
	exit 255
fi

android_cmake_setup . "$build" \
	-DBUILD_BINARY=OFF \
	-DBUILD_SHARED_LIBS=OFF \
	-DCHECK_SSE2=OFF
android_cmake_build "$build"
android_cmake_install "$build"

pc="$prefix_dir/lib/pkgconfig/uchardet.pc"
[ -f "$pc" ] || { echo "Missing uchardet.pc" >&2; exit 1; }
${SED:-sed} -i.bak 's/-lstdc++/-lc++/g' "$pc"
rm -f "$pc.bak"
if ! grep -q '^Libs:.*-lc++' "$pc"; then
	${SED:-sed} -i.bak '/^Libs:/ s/$/ -lc++/' "$pc"
	rm -f "$pc.bak"
fi
PKG_CONFIG_LIBDIR="$prefix_dir/lib/pkgconfig" pkg-config --static --libs uchardet >/dev/null
