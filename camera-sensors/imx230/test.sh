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
profile=${CITYMAN_CAMERA_SENSOR_PROFILE:-smia9}
case "$profile" in
    smia9) ;;
    nubia24)
        python3 "$here/generate-nubia24.py" "${CITYMAN_CAMERA_NUBIA_SENSOR_BLOB:?supply the private pinned Nubia sensor ELF}" \
            "$out/nubia24_registers.generated.h"
        common+=(-DCITYMAN_CAMERA_NUBIA24 -I"$out")
        ;;
    lumia20)
        python3 "$here/generate-lumia20.py" "${CITYMAN_CAMERA_LUMIA20_DCC:?supply the private pinned 0AEACA05.dcc}" \
            "$out/lumia20_registers.generated.h"
        # The headers also hold tables lumia20 does not use (smia_mode4,
        # lumia20_mode1). The platform clang ignores unused statics declared
        # in headers; this GCC does not.
        common+=(-DCITYMAN_CAMERA_LUMIA20 -I"$out" -Wno-unused-variable)
        ;;
    *) echo "Unsupported sensor profile: $profile" >&2; exit 1 ;;
esac
if [[ ${CITYMAN_CAMERA_LUMIA20_BINNED:-} == true ]]; then
    [[ $profile == lumia20 ]] || { echo "CITYMAN_CAMERA_LUMIA20_BINNED needs lumia20" >&2; exit 1; }
    common+=(-DCITYMAN_CAMERA_LUMIA20_BINNED)
fi
if [[ ${CITYMAN_CAMERA_AF:-} == true ]]; then
    common+=(-DCITYMAN_CAMERA_AF "-DCITYMAN_CAMERA_AF_NAME=\"${CITYMAN_CAMERA_AF_NAME:-cityman}\"")
fi
"${compiler}gcc" "${common[@]}" -nostdlib -static -Wl,-e,_start \
    "$here/sensor_lib_imx230.c" "$here/test_sensor.c" "$here/test_start.S" -o "$out/test_sensor"
status=0
qemu-arm -cpu max "$out/test_sensor" || status=$?
# A failing REQUIRE exits with its source line number (modulo 256).
[[ $status == 0 ]] || { echo "ARM32 sensor test failed: code $status" >&2; exit 1; }
echo "ARM32 sensor tests passed: $profile${CITYMAN_CAMERA_LUMIA20_BINNED:+ binned=$CITYMAN_CAMERA_LUMIA20_BINNED}"
"${compiler}gcc" "${common[@]}" -fPIC -fvisibility=hidden -nostdlib -shared -Wl,-Bsymbolic \
    -Wl,--no-undefined -Wl,-soname,libmmcamera_imx230.so \
    "$here/sensor_lib_imx230.c" -o "$out/libmmcamera_imx230.so"
sha256sum "$out/libmmcamera_imx230.so"
