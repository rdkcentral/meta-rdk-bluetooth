# Version & revision control for the Bluetooth audio stack components owned by
# meta-rdk-bluetooth. This is the single place to bump versions/revisions.
# Update these values and call them out when sharing a new meta-rdk-bluetooth tag.
#
# NOTE: PACKAGE_ARCH (feed routing) is intentionally NOT set here. It is applied
# in the vendor layers:
#   - OSS components (pipewire, wireplumber, sdbus-c++, basu) -> VENDOR_OSS_ARCH
#     in meta-oss-vendor-amlogic (conf/machine/include/vendor-oss-pkg_versions.inc)
#   - RDK components (bluetoothsdk, rmf-audio-capture-pipewire) -> VENDOR_LAYER_EXTENSION
#     in the OEM vendor layer (conf/include/vendor_pkg_versions.inc)

# RDK components
PV:pn-bluetoothsdk = "1.0.1"
PR:pn-bluetoothsdk = "r0"

PV:pn-rmf-audio-capture-pipewire = "1.0"
PR:pn-rmf-audio-capture-pipewire = "r2"

# OSS components
PV:pn-sdbus-c++ = "2.1.0"
PR:pn-sdbus-c++ = "r0"

PV:pn-basu = "0.2.1"
PR:pn-basu = "r0"

PV:pn-pipewire = "1.5.85"
PR:pn-pipewire = "r0"

PV:pn-wireplumber = "0.5.13"
PR:pn-wireplumber = "r0"

