#!/usr/bin/env bash
set -euo pipefail
here=$(cd -- "$(dirname -- "$0")" && pwd)
kernel=$(realpath "${1:?usage: test-consumer.sh KERNEL ARM_GCC_PREFIX ANGLER_VENDOR_LIB PRIVATE_OUTPUT [CANDIDATE_SO]}")
compiler=${2:?supply ARM GCC prefix}
vendor=$(realpath "${3:?supply the official angler vendor/lib directory}")
out=$(realpath -m "${4:?supply a private output directory}")
mkdir -p "$out"
check_hash() {
    [[ $(sha256sum "$1" | cut -d' ' -f1) == "$2" ]] || { echo "Unexpected fixture: $1" >&2; exit 1; }
}
check_hash "$vendor/libmmcamera2_sensor_modules.so" 2f9949e423c8bb65089fb0264a3ab812274efde578a300b789f6626c2e1d6803
check_hash "$vendor/libmmcamera_imx377.so" 3c1f5d3583f88b3592eaf21afad8bd65b7b054a87ec178c74541f252b19d22bc
cp "$vendor/libmmcamera2_sensor_modules.so" "$out/consumer.so"
cp "$vendor/libmmcamera_imx377.so" "$out/reference.so"
if [[ -n ${5:-} ]]; then
    cp "$(realpath "$5")" "$out/candidate.so"
else
    "$here/test.sh" "$kernel" "$compiler" "$out/candidate-build"
    cp "$out/candidate-build/libmmcamera_imx230.so" "$out/candidate.so"
fi
(
    cd "$out"
    "${compiler}objcopy" -I binary -O elf32-littlearm -B arm consumer.so consumer.o
    "${compiler}objcopy" -I binary -O elf32-littlearm -B arm reference.so reference.o
    "${compiler}objcopy" -I binary -O elf32-littlearm -B arm candidate.so candidate.o
)
"${compiler}gcc" -std=gnu11 -Wall -Wextra -Werror -O2 -nostdinc -ffreestanding -fno-builtin \
    -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables \
    -mcpu=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=softfp -I"$kernel/include" \
    -nostdlib -static -Wl,-e,_start "$here/sensor_lib_imx230.c" "$here/test_consumer.c" \
    "$here/test_start.S" "$out/consumer.o" "$out/reference.o" "$out/candidate.o" -o "$out/test_consumer"
qemu-arm -cpu max "$out/test_consumer"
echo 'Real angler consumer: loaded candidate .so, capabilities, GRBG10, I2C, power and CSI tests passed'
