inherit systemd syslog-ng-config-gen logrotate_config

SRC_URI:append:rdkstb  = " file://0001-Add_qca_btsoc_chip_support.patch "

SYSLOG-NG_FILTER:client += "bluetooth"
SYSLOG-NG_SERVICE_bluetooth:client += "bluetooth.service"
SYSLOG-NG_DESTINATION_bluetooth:client = "bluez.log"
SYSLOG-NG_LOGRATE_bluetooth:client = "medium"

LOGROTATE_NAME="bluez"
LOGROTATE_LOGNAME_bluez="bluez.log"
#HDD_DISABLE
LOGROTATE_SIZE_MEM_bluez="250000"
LOGROTATE_ROTATION_MEM_bluez="2"
#HDD_ENABLE
LOGROTATE_SIZE_bluez="512000"
LOGROTATE_ROTATION_bluez="5"

# Remapping default localstatedir which has a value /var to /opt (persistent memory) across boxes
# to store bluetooth device and runtime operations data across STB power cycles
EXTRA_OECONF:append:hybrid += " --localstatedir=/opt"
EXTRA_OECONF:append:client += " --localstatedir=/opt"

EXTRA_OECONF:append:broadband += " --localstatedir=/opt/secure"

PACKAGECONFIG:append = " experimental"
EXTRA_OECONF += " --with-systemdsystemunitdir=${systemd_unitdir}/system"

RDEPENDS:${PN} += "${PN}-noinst-tools"
RPROVIDES:${PN} += "${PN}-systemd"
RREPLACES:${PN} += "${PN}-systemd"
RCONFLICTS:${PN} += "${PN}-systemd"

FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

CFLAGS:append = " -DBT_UNSUPPORTED_GAMEPAD_ENABLED"

do_install:append() {
    mkdir -p ${D}${includedir}/bluetooth/audio/
    install -m 0644 ${S}/profiles/audio/a2dp-codecs.h ${D}${includedir}/bluetooth/audio/
    install -m 0644 ${S}/lib/uuid.h ${D}${includedir}/bluetooth/
    install -c -m 644 ${S}/src/main.conf  ${D}${sysconfdir}/bluetooth/main.conf
    #rm  ${D}${bindir}/bdaddr
    rm  ${D}${bindir}/avinfo
    rm  ${D}${bindir}/avtest
    rm  ${D}${bindir}/scotest
    rm  ${D}${bindir}/amptest
    rm  ${D}${bindir}/hwdb
    rm  ${D}${bindir}/hcieventmask
    rm  ${D}${bindir}/hcisecfilter
    rm  ${D}${bindir}/btinfo
    rm  ${D}${bindir}/btattach
    rm  ${D}${bindir}/btsnoop
    rm  ${D}${bindir}/btproxy
    rm  ${D}${bindir}/btiotest
    rm  ${D}${bindir}/mcaptest
    rm  ${D}${bindir}/cltest
    rm  ${D}${bindir}/oobtest
    rm  ${D}${bindir}/seq2bseq
    rm  ${D}${bindir}/ibeacon
    rm  ${D}${bindir}/btgatt-client
    rm  ${D}${bindir}/btgatt-server
    rm  ${D}${bindir}/gatt-service
    rm  ${D}${bindir}/iapd
}


do_install:append:hybrid() {
    mkdir -p ${D}${sysconfdir}/bluetooth/
    install -c -m 644 ${S}/src/main.conf  ${D}${sysconfdir}/bluetooth/main.conf
}


do_install:append:client() {
    mkdir -p ${D}${sysconfdir}/bluetooth/
    install -c -m 644 ${S}/src/main.conf  ${D}${sysconfdir}/bluetooth/main.conf
    install -m 0755 ${WORKDIR}/bt_original_path_setup.sh ${D}/${sysconfdir}/bluetooth/
}

SYSTEMD_SERVICE:${PN} = "bluetooth.service"
SYSTEMD_AUTO_ENABLE = "enable"

RDEPENDS:${PN}-testtools = ""
INSANE_SKIP:${PN}-testtools = "file-rdeps"

# From meta-rdk-comcast/recipes-connectivity/bluez/bluez5_5.%.bbappend
#This file is to patch the bluetooth.service to launch bluetooth daemon after /opt is mounted on the platform
FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"
SRC_URI += "file://0001-bluetooth_service_in_generic.patch \
           "

inherit breakpad-wrapper breakpad-logmapper
DEPENDS:append = " breakpad breakpad-wrapper"

BREAKPAD_BIN:append = " bluetoothd"
PACKAGECONFIG[breakpad] = "--enable-breakpad,,breakpad,"
# generating minidumps
PACKAGECONFIG:append = " breakpad"
# Breakpad processname and logfile mapping
BREAKPAD_LOGMAPPER_PROCLIST = "bluetoothd"
BREAKPAD_LOGMAPPER_LOGLIST = "bluez.log"
