#!/vendor/bin/sh
# Bring-up aid for Android 16: keep the kernel log, the process list and the
# properties on /data every 5 s for 30 min, so a boot that never reaches adb
# can be read back over USB mass storage. Remove once A16 boots reliably.
d=/data/vendor/bootlog
mkdir -p $d
n=0
while [ $n -lt 360 ]; do
    dmesg > $d/dmesg.txt 2>&1
    ps -A > $d/ps.txt 2>&1
    getprop > $d/props.txt 2>&1
    cat /proc/uptime > $d/uptime.txt
    sync
    sleep 5
    n=$((n + 1))
done
