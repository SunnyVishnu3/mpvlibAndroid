#!/bin/bash -e

set -o pipefail

aar=$1
[ -f "$aar" ] || { echo "AAR not found: $aar" >&2; exit 1; }

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
. "$script_dir/../include/build_config.sh"

workdir=$(mktemp -d)
trap 'rm -rf "$workdir"' EXIT
unzip -q "$aar" -d "$workdir"

abis=(armeabi-v7a arm64-v8a)
[ "$ENABLE_X86_ARCH" = "true" ] && abis+=(x86 x86_64)
libraries=(
	libavcodec.so
	libavdevice.so
	libavfilter.so
	libavformat.so
	libavutil.so
	libc++_shared.so
	libmpv.so
	libplayer.so
	libswresample.so
	libswscale.so
)

for abi in "${abis[@]}"; do
	case "$abi" in
		armeabi-v7a) expected_class=ELF32; expected_machine='ARM' ;;
		arm64-v8a) expected_class=ELF64; expected_machine='AArch64' ;;
		x86) expected_class=ELF32; expected_machine='Intel 80386' ;;
		x86_64) expected_class=ELF64; expected_machine='Advanced Micro Devices X86-64' ;;
	esac

	for library in "${libraries[@]}"; do
		path="$workdir/jni/$abi/$library"
		[ -f "$path" ] || { echo "Missing jni/$abi/$library" >&2; exit 1; }
		header=$(readelf -h "$path")
		grep -Eq "Class:[[:space:]]+$expected_class" <<<"$header"
		grep -Eq "Machine:[[:space:]]+$expected_machine" <<<"$header"
	done

	for path in "$workdir/jni/$abi"/*.so; do
		while IFS= read -r needed; do
			case "$needed" in
				libav*.so|libmpv.so|libplayer.so|libc++_shared.so)
					[ -f "$workdir/jni/$abi/$needed" ] || {
						echo "Missing $needed required by $(basename "$path") for $abi" >&2
						exit 1
					}
					;;
			esac
		done < <(readelf -d "$path" | sed -n 's/.*Shared library: \[\(.*\)\]/\1/p')
	done

	symbols=$(readelf -Ws "$workdir/jni/$abi/libmpv.so")
	if grep -Eq ' UND .* (DVD[A-Za-z0-9_]*|dvdnav_[A-Za-z0-9_]*)$' <<<"$symbols"; then
		echo "libmpv.so has unresolved DVD symbols for $abi" >&2
		exit 1
	fi
done

mkdir "$workdir/classes"
unzip -q "$workdir/classes.jar" -d "$workdir/classes"
if grep -aERq '%[A-Z0-9_]+%' "$workdir/classes"; then
	echo "Unresolved version placeholders remain in classes.jar" >&2
	exit 1
fi

echo "Verified AAR native contents for: ${abis[*]}"
