#!/bin/bash -e

. ../../include/path.sh

build=_build$ndk_suffix

check_iconv_files () {
	for file in "$prefix_dir/include/iconv.h" "$prefix_dir/lib/libiconv.a" "$prefix_dir/lib/libcharset.a"; do
		if [ ! -f "$file" ]; then
			echo "Missing libiconv file: $file" >&2
			exit 1
		fi
	done
}

patch_mpv_iconv_dependency () {
	local iconv_dep
	iconv_dep="iconv = declare_dependency(compile_args: ['-I$prefix_dir/include'], link_args: ['$prefix_dir/lib/libiconv.a', '$prefix_dir/lib/libcharset.a'])"
	${SED:-sed} -i.bak \
		-e "/^iconv = dependency('iconv', required: get_option('iconv'))$/c\\$iconv_dep" \
		-e "/^iconv = declare_dependency(compile_args: \['-I.*\/include'\], link_args: \['.*\/libiconv\.a', '.*\/libcharset\.a'\])$/c\\$iconv_dep" \
		meson.build
	rm -f meson.build.bak
	if ! grep -Fq "$iconv_dep" meson.build; then
		echo "Unable to configure mpv's static libiconv dependency" >&2
		exit 1
	fi
}

if [ "$1" == "build" ]; then
	true
elif [ "$1" == "clean" ]; then
	rm -rf $build
	exit 0
else
	exit 255
fi

unset CC CXX # meson wants these unset

check_iconv_files
patch_mpv_iconv_dependency

export LDFLAGS="$LDFLAGS -L$prefix_dir/lib"
export CPPFLAGS="$CPPFLAGS -I$prefix_dir/include"

echo "Checking MuJS before building mpv..."

if [ ! -f "$prefix_dir/lib/libmujs.a" ]; then
	echo "Error: libmujs.a not found at $prefix_dir/lib/libmujs.a" >&2
	echo "Build mujs first: ./buildall.sh --arch $prefix_name mujs" >&2
	exit 1
fi

if [ ! -f "$prefix_dir/include/mujs.h" ]; then
	echo "Error: mujs.h not found at $prefix_dir/include/mujs.h" >&2
	exit 1
fi

if [ ! -f "$prefix_dir/lib/pkgconfig/mujs.pc" ]; then
	echo "Error: mujs.pc not found at $prefix_dir/lib/pkgconfig/mujs.pc" >&2
	exit 1
fi

echo "pkg-config check for mujs:"
pkg-config --libs mujs
pkg-config --cflags mujs

meson setup $build --cross-file "$prefix_dir"/crossfile.txt \
	--default-library shared \
	-Diconv=enabled \
	-Duchardet=enabled \
	-Dlibarchive=enabled \
	-Ddvdnav=enabled \
	-D{lua,libcurl,rubberband}=enabled \
	-Djavascript=enabled \
	-Dvulkan=enabled \
	-Dlibmpv=true \
	-Dcplayer=false \
	-Dlibbluray=enabled \
	-Dmanpage-build=disabled

ninja -C $build -j$cores

if [ -f $build/libmpv.a ]; then
	echo >&2 "Meson produced static libmpv.a instead of shared libmpv.so, forcing rebuild."
	$0 clean
	exec $0 build
fi

DESTDIR="$prefix_dir" ninja -C $build install
