FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}_${PV}:"

## Generic BlueZ 5.48 patches (order preserved from original apply sequence)
SRC_URI += "file://0001-bluetooth_service_in_generic.patch \
    file://breakpad.patch \
    file://bluez-5.48-003-add-configurable-char-write-value-options.patch \
    file://bluez-5.48-005-enable_auto_connect_on_all_disconnects.patch \
    file://bluez-5.48-006-change_cache_clear_timeout.patch \
    file://bluez-5.48-007-disable_sigpipe_signal.patch \
    file://bluez-5.48-008-make-storage-dir-runtime-configurable.patch \
    file://bluez-5.48-009-fix-gatt-characteristic-crash-on-remove.patch \
    file://bluez-5.48-015-delete-autoconnect-on-remove.patch \
    file://bluez-5.48-022-enable_bdaddr.patch \
    file://bluez-5.48-023-enable_discovery_filter.patch \
    file://bluez-5.48-027-free-discovery-reply-on-error.patch \
    file://bluez-5.48-028-crash-in-gatt-client-callback.patch \
    file://bluez-5.48-029-create_storage_directory_before_starting_service.patch \
    file://bluez-5.48-032-restore-file-AMLOGIC-1276.patch \
    file://bluez-5.48-033-enable-debug-logging.patch \
    file://0001-testtools-fix-SIOCGSTAMP-undeclared-error.patch \
    file://0002-libexecdir-location.patch \
    file://bluez-5.48-035-avrcp-transport-volume-change.patch \
    file://bluez-5.48-036-changes_to_fix_crash_during_BT_SIG_tests.patch \
    file://bluez-5.48-037-bluetooth_avdtp_a2dp_abort.patch \
    file://bluez-5.48-038-bluez-stream-free-crash-fix.patch \
    file://bluez-5.48-039-bluetooth_a2dp_ref_negative_abort.patch \
    file://bluez-5.48-040-bluez-btrmgr-crash.patch \
    file://bluez-5.48-041-clear_old_cache_list.patch \
    file://bluez-5.48-043-auto-accept-connection-for_5.10-Kernel.patch \
    file://bluez-5.48-044-add-up-to-date-battery-service.patch \
    file://bluez-5.48-045-ensure-bluez-connects-on-bredr-to-audio-devices.patch \
    file://bluz5_5.48_gatt_db_service_crash.patch \
    file://bluez-5.48-046-prevent-scan-becoming-stuck.patch \
    file://bluez-5.48-049-Queue_remove_crash.patch \
    file://bluez-5.48-051-fix-for-incorrect-transaction-label.patch \
    file://bluez-5.48-052-bt_uuid_to_uuid128-crash.patch \
    file://bluez-5.48-053-set-ad-flags-and-update-cache-timeout.patch \
    file://bluez-5.48-054-enable_ccc_callback_crash.patch \
    file://bluez-5.48-055-kernel-dev-node-delete-create.patch \
    file://bluez-5.48-056-remove-pairing-failure-cache.patch \
    file://bluez-5.48-057-stop-scan-getting-stuck-when-adapter-busy.patch \
    file://bluez-5.48-058-set-le-hid-auto-connect-flags.patch \
    file://bluez-5.48-059-hci-version-update.patch \
    file://bluez-5.48-061-Queue-crash.patch \
    file://bluez-5.48-061-RDK-56281-BT-SIG-PTS-disable-gatt-server-opcodes.patch \
    file://bluez-5.48-062-Add-timer-for-removing-temp-devices.patch \
    file://bluez-5.48-063-stop-gatt-db-reset-on-early-disconnection.patch \
    file://bluez-5.48-064-allow-large-sevices-changed-gatt.patch \
    file://bluez-5.48-066-unregister-batt_io_ccc_written_cb-check-session.patch \
    file://bluez-5.48-067-remove-unused-binaries.patch \
    file://bluez-5.48-068-RDKEMW-8673-stream-use-after-free.patch \
    file://bluez-5.48-069-RDKEMW-11885-UAF-in-messagefilter.patch \
    file://bluez-5.48-kirkstone_compile_errors.patch \
    file://0001-Fix-race-issue-with-tools-directory.patch \
    file://CVE-2019-8922.patch \
    file://CVE-2020-27153.patch \
    file://CVE-2022-0204.patch \
    file://CVE-2020-0556.patch \
    file://0002-Fixing-connection-failure-due-to-CVE-2020-0556.patch \
    file://CVE-2018-10910.patch \
    file://CVE-2018-10910_I.patch \
    file://bluez-5.48-034-check-if-dir-is-a-file.patch \
    file://CVE-2019-8921.patch \
    file://CVE-2022-39176_5.48_fix.patch \
    file://CVE-2022-39177_5.48_fix.patch \
    file://CVE-2023-45866.patch \
    file://0003-Fix-input-hog-connection-with-slow-pairing-devices.patch \
    file://CVE-2021-3658_5.48_fix.patch \
    file://bluez-5.48-070-RDK-59395-BT-SIG-PTS-certification-changes.patch \
    file://bluez-5.48-073-RDKOSS-1008-endpoint-crash.patch \
    file://bluez-5.48-074-LLAMA-18285-increase-gatt-notify-pipe-buffer.patch \
    file://0001-add-bt-secure-path-setup.patch \
    file://bt_secure_path_setup.sh \
    "

# Removed testtools package as it has a depedncy with python
PACKAGES:remove = "${PN}-testtools"
# Add DISTRO_FEATURES:append = ' blueztest' in rdke-distros.inc if we need to add ${PN}-testtools
PACKAGES += "${@bb.utils.contains('DISTRO_FEATURES', 'blueztest', '${PN}-testtools', '', d)}"
do_install:append() {
    install -d ${D}${sysconfdir}/bluetooth/
    install -m 0755 ${WORKDIR}/bt_secure_path_setup.sh ${D}${sysconfdir}/bluetooth/
    #Files inside /usr/lib/bluez are test files. These are required only when PACKAGE ${PN}-testtools is added. Without the packages, these files are not required and observing package_qa error since the files are not shipped. Remove it unless the distro is defined.
    if ${@bb.utils.contains('DISTRO_FEATURES', 'blueztest', 'false', 'true', d)}; then
        rm -rf ${D}${libdir}/bluez
    fi
}
FILES:${PN} += "${sysconfdir}/bluetooth/bt_secure_path_setup.sh"

