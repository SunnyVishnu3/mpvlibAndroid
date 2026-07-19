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
	-Denable_tools=false \
	-Denable_docs=false \
	-Denable_devtools=false \
	-Denable_examples=false \
	-Dbdj_jar=disabled \
	-Dembed_udfread=false \
	-Dfontconfig=disabled \
	-Dfreetype=disabled \
	-Dlibxml2=disabled

ninja -C "$build" -j"$cores"
DESTDIR="$prefix_dir" ninja -C "$build" install

pc="$prefix_dir/lib/pkgconfig/libbluray.pc"
[ -f "$pc" ] || { echo "Missing libbluray.pc" >&2; exit 1; }
if ! grep -q -- '-ludfread' "$pc"; then
	${SED:-sed} -i.bak '/^Libs:/ s/$/ -ludfread/' "$pc"
	rm -f "$pc.bak"
fi
PKG_CONFIG_LIBDIR="$prefix_dir/lib/pkgconfig" pkg-config --static --libs libbluray >/dev/null
