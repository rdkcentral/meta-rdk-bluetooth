SUMMARY = "RMF Audio Capture PipeWire Source Plugin"
DESCRIPTION = "PipeWire source plugin that bridges RMF Audio Capture HAL with PipeWire, \
automatically started by WirePlumber when Bluetooth devices connect"
HOMEPAGE = "https://rdkcentral.com"
SECTION = "multimedia"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/Apache-2.0;md5=89aea4e17d99a7cacdbeed46a0096b10"

DEPENDS = "pipewire virtual/media-utils media-utils-headers"
RDEPENDS:${PN} = "pipewire wireplumber virtual/media-utils"

SRC_URI = "file://rmfAudioCapturePlugin.c \
           file://CMakeLists.txt \
           file://rmf-audio-on-bluez.lua \
           file://51-rmf-audio-bluez.conf \
           file://rmf-audio-capture-pipewire.service \
          "

S = "${WORKDIR}"

inherit cmake pkgconfig systemd

SYSTEMD_SERVICE:${PN} = "rmf-audio-capture-pipewire.service"
SYSTEMD_AUTO_ENABLE = "disable"

do_install:append() {

    install -d ${D}${bindir}
    install -m 0755 ${B}/rmf-audio-capture-source ${D}${bindir}/

    # Install systemd service
    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/rmf-audio-capture-pipewire.service ${D}${systemd_system_unitdir}/rmf-audio-capture-pipewire.service

    # Install WirePlumber Lua script
    install -d ${D}${datadir}/wireplumber/scripts
    install -m 0644 ${WORKDIR}/rmf-audio-on-bluez.lua ${D}${datadir}/wireplumber/scripts/51-rmf-audio-on-bluez.lua

    # Install WirePlumber configuration
    install -d ${D}${sysconfdir}/wireplumber/wireplumber.conf.d
    install -m 0644 ${WORKDIR}/51-rmf-audio-bluez.conf ${D}${sysconfdir}/wireplumber/wireplumber.conf.d/51-rmf-audio-bluez.conf
}

FILES:${PN} = " \
    ${bindir}/rmf-audio-capture-source \
    ${systemd_system_unitdir}/rmf-audio-capture-pipewire.service \
    ${datadir}/wireplumber/scripts/51-rmf-audio-on-bluez.lua \
    ${sysconfdir}/wireplumber/wireplumber.conf.d/51-rmf-audio-bluez.conf \
"

# Restart wireplumber after installation to load new configuration
pkg_postinst_ontarget:${PN}() {
    if [ -n "$D" ]; then
        exit 1
    fi
    
    # Check if wireplumber is running as systemd user service
    if systemctl --user is-active wireplumber.service >/dev/null 2>&1; then
        echo "Restarting WirePlumber to load RMF Audio Capture configuration..."
        systemctl --user restart wireplumber.service || true
    fi
}

