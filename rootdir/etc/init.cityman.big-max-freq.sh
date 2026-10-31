#!/vendor/bin/sh
#
# A57 user maximum for the performance profiles (power/README.md).
#
# The power HAL sets vendor.power.big_max_freq: "full" lifts scaling_max_freq
# of the A57 cluster to cpuinfo_max_freq for the performance profile, any
# other value restores the 1632 MHz written at boot by init.cityman.power.sh.
# Perf locks (profiles, thermal-engine) and msm_thermal only lower this
# limit further.

BOOT_MAX=1632000

apply() {
    # The cluster policy is reachable from any online A57. After the power
    # save lock is released core_ctl brings two back within milliseconds:
    # wait up to one second for them.
    for i in 1 2 3 4 5 6 7 8 9 10; do
        for c in 4 5 6 7; do
            d=/sys/devices/system/cpu/cpu$c/cpufreq
            [ -w $d/scaling_max_freq ] || continue
            if [ "$1" = "full" ]; then
                f=`cat $d/cpuinfo_max_freq`
            else
                f=$BOOT_MAX
            fi
            echo $f > $d/scaling_max_freq && return 0
        done
        sleep 0.1
    done
    return 1
}

# The property can change again while this runs (init does not start a
# running oneshot service twice): loop until the value is the applied one.
applied=
while :; do
    want=`getprop vendor.power.big_max_freq`
    [ "$want" = "$applied" ] && exit 0
    apply "$want"
    applied=$want
done
