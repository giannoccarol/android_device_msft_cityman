#!/vendor/bin/sh
# On Android 16 the first power-up of the modem, requested by pm-proxy, times
# out during MBA image authentication (RMB status 3); a second request boots it.
# Keep /dev/subsys_modem open (this retries the power-up and keeps the modem
# up), and once the modem is online restart the QMI clients, which gave up
# while it was offline.

modem_state() {
    for d in /sys/bus/msm_subsys/devices/subsys*; do
        if [ "$(cat "$d/name" 2>/dev/null)" = modem ]; then
            cat "$d/state"
            return
        fi
    done
}

msg() {
    echo "CitymanModem: $1" > /dev/kmsg
}

attempt=0
holder=
waited=0
while [ "$attempt" -lt 4 ]; do
    attempt=$((attempt + 1))
    ( while :; do sleep 86400; done ) 3</dev/subsys_modem &
    holder=$!
    i=0
    while [ "$i" -lt 30 ]; do
        [ "$(modem_state)" = ONLINE ] && break
        kill -0 "$holder" 2>/dev/null || break
        sleep 1
        i=$((i + 1))
        waited=$((waited + 1))
    done
    [ "$(modem_state)" = ONLINE ] && break
    msg "modem power-up attempt $attempt failed"
    kill "$holder" 2>/dev/null
    holder=
    sleep 2
    waited=$((waited + 2))
done

if [ "$(modem_state)" != ONLINE ]; then
    msg "modem still offline, giving up"
    exit 1
fi

msg "modem online after ${waited}s (attempt $attempt)"
# Only when the first power-up failed: the QMI clients then gave up on a
# modem that stayed offline. A first attempt that is merely slow (15 s is
# normal) is followed by them, and restarting rild then reset the SIM and
# asked for the PIN a second time.
if [ "$attempt" -gt 1 ]; then
    setprop ctl.restart qmuxd
    sleep 2
    setprop ctl.restart netmgrd
    setprop ctl.restart vendor.ril-daemon
    setprop ctl.restart cityman-uim-bootstrap
    msg "restarted qmuxd, netmgrd, rild and the UIM bootstrap"
fi

# Keep the holder (and with it the modem) alive for the whole session.
wait
