#!/bin/bash -e

. ../../include/path.sh

build=_build$ndk_suffix

if [ "$1" == "build" ]; then
	true
elif [ "$1" == "clean" ]; then
	rm -rf "$build"
	exit 0
else
	exit 255
fi

unset CC CXX
meson setup "$build" --cross-file "$prefix_dir/crossfile.txt" \
	--default-library static \
	-Denable_docs=false \
	-Denable_examples=false
ninja -C "$build" -j"$cores"
DESTDIR="$prefix_dir" ninja -C "$build" install

pc="$prefix_dir/lib/pkgconfig/dvdnav.pc"
[ -f "$pc" ] || { echo "Missing dvdnav.pc" >&2; exit 1; }
for lib in -ldvdread -ldl; do
	if ! grep -q -- "^Libs:.*$lib" "$pc"; then
		${SED:-sed} -i.bak "/^Libs:/ s/$/ $lib/" "$pc"
		rm -f "$pc.bak"
	fi
done
PKG_CONFIG_LIBDIR="$prefix_dir/lib/pkgconfig" pkg-config --static --libs dvdnav >/dev/null
