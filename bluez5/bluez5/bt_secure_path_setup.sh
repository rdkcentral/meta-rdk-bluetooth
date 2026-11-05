#!/bin/sh

/bin/mkdir -p /var/lib/bluetooth
/bin/mkdir -p /opt/secure/lib/bluetooth

if [ -d /opt/lib/bluetooth ]; then
    cp -r /opt/lib/bluetooth /opt/secure/lib/
    rm -r /opt/lib/bluetooth
fi
