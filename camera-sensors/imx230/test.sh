#!/usr/bin/env bash
set -euo pipefail
here=$(cd -- "$(dirname -- "$0")" && pwd)
kernel=$(realpath "${1:?usage: test.sh KERNEL_REPO ARM_GCC_PREFIX PRIVATE_OUTPUT}")
compiler=${2:?supply ARM Android GCC prefix}
out=$(realpath -m "${3:?supply private output directory}")
mkdir -p "$out"
common=(-std=gnu11 -Wall -Wextra -Werror -O2 -nostdinc -ffreestanding -fno-builtin
        -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables
        -mcpu=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=softfp -I"$kernel/include")
"${compiler}gcc" "${common[@]}" -nostdlib -static -Wl,-e,_start \
    "$here/sensor_lib_imx230.c" "$here/test_sensor.c" "$here/test_start.S" -o "$out/test_sensor"
qemu-arm -cpu max "$out/test_sensor"
echo 'ARM32 sensor tests passed'
"${compiler}gcc" "${common[@]}" -fPIC -fvisibility=hidden -nostdlib -shared -Wl,-Bsymbolic \
    -Wl,--no-undefined -Wl,-soname,libmmcamera_imx230.so \
    "$here/sensor_lib_imx230.c" -o "$out/libmmcamera_imx230.so"
sha256sum "$out/libmmcamera_imx230.so"
