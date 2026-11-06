#!/bin/bash -e

. ../../include/path.sh
. ../../include/cmake-android.sh

if [ "$prefix_name" != "arm64" ]; then
	# Adreno Tools is only supported on arm64-v8a
	exit 0
fi

build=_build$ndk_suffix

if [ "$1" == "build" ]; then
	true
elif [ "$1" == "clean" ]; then
	rm -rf "$build"
	exit 0
else
	exit 255
fi

# Source of adrenotools is in deps/adrenotools or ../../../libadrenotools
src_dir="$DIR/deps/adrenotools"
if [ ! -d "$src_dir" ]; then
	src_dir="$DIR/../../libadrenotools"
fi

if [ ! -d "$src_dir" ]; then
	echo "adrenotools source not found at $src_dir" >&2
	exit 1
fi

android_cmake_setup "$src_dir" "$build" \
	-DBUILD_SHARED_LIBS=ON \
	-DGEN_INSTALL_TARGET=ON

android_cmake_build "$build"
android_cmake_install "$build"

# Copy hooks to prefix_dir/lib so they package into the AAR jniLibs
mkdir -p "$prefix_dir/lib"
for hook in libmain_hook.so libhook_impl.so libfile_redirect_hook.so libadrenotools.so; do
	find "$build" -name "$hook" -exec cp -f {} "$prefix_dir/lib/" \;
done

