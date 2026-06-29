FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}_${PV}:"

## Generic BlueZ 5.82 patches (order preserved from original apply sequence)
SRC_URI += "file://0001-bluetooth_service_in_generic.patch \
    file://bluez5p80-resolve-compilation.patch \
    file://0001-libexecdir-location-5.7xx.patch \
    file://0005-clear_old_cache_list.patch \
    file://0004-enable_auto_connect_on_all_disconnects.patch \
    file://0005-enable-auto-connect.patch \
    file://0006-Fix-input-hog-connection-with-slow-pairing-devices.patch \
    file://0007-create_storage_directory_before_starting_service.patch \
    file://0008-bluez-crash-fixes.patch \
    file://0009-delete-autoconnect-on-remove.patch \
    file://0010-ensure-bluez-connects-on-bredr-to-audio-devices.patch \
    file://0012-change_cache_clear_timeout.patch \
    file://0017-enable_bdaddr.patch \
    file://0018-bluez-stream-free-and-a2dp_ref_negative-crash-fixes.patch \
    file://0019-make-storage-dir-runtime-configurable.patch \
    file://0022-restore-pairing-info-after-kernel-crash.patch \
    file://0024-bluez-from-5.48-disable_sigpipe_signal.patch \
    file://0026-bt_uuid_to_uuid128-crash.patch \
    file://0027-set-ad-flags-and-update-cache-timeout.patch \
    file://0028-set-le-hid-auto-connect-flags.patch \
    file://0029-RDK-56281-BT-SIG-PTS-disable-gatt-server-opcodes.patch \
    file://0030-gatt-db-service-crash.patch \
    file://0031-prevent-scan-stuck-and-stop-scan-when-adapter-busy.patch \
    file://0032-breakpad.patch \
    file://0034-DELIA-69976-bluez-log-flood.patch \
    "

# Removed testtools package as it has a depedncy with python
PACKAGES:remove = "${PN}-testtools"
# Add DISTRO_FEATURES:append = ' blueztest' in rdke-distros.inc if we need to add ${PN}-testtools
PACKAGES += "${@bb.utils.contains('DISTRO_FEATURES', 'blueztest', '${PN}-testtools', '', d)}"
do_install:append() {
    #Files inside /usr/lib/bluez are test files. These are required only when PACKAGE ${PN}-testtools is added. Without the packages, these files are not required and observing package_qa error since the files are not shipped. Remove it unless the distro is defined.
    if ${@bb.utils.contains('DISTRO_FEATURES', 'blueztest', 'false', 'true', d)}; then
        rm -rf ${D}${libdir}/bluez
    fi
}
