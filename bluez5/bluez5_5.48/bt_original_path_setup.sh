#!/bin/sh

/bin/mkdir -p /var/lib/bluetooth
/bin/mkdir -p /opt/lib/bluetooth

if [ -d /opt/secure/lib/bluetooth ]; then
    cp -r /opt/secure/lib/bluetooth /opt/lib/
    rm -r /opt/secure/lib/bluetooth
fi
