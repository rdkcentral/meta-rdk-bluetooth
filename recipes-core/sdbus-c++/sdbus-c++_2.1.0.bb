SUMMARY = "sdbus-c++"
DESCRIPTION = "High-level C++ D-Bus library designed to provide easy-to-use yet powerful API in modern C++"

SECTION = "libs"

LICENSE = "LGPLv2.1"
LIC_FILES_CHKSUM = "file://COPYING;md5=1803fa9c2c3ce8cb06b4861d75310742"

inherit cmake pkgconfig systemd ptest

PACKAGECONFIG ??= "with-external-libsystemd \
                   ${@bb.utils.contains('PTEST_ENABLED', '1', 'with-tests', '', d)}"
PACKAGECONFIG[with-builtin-libsystemd] = ",,sdbus-c++-libsystemd,libcap"
PACKAGECONFIG[with-external-libsystemd] = ",,systemd,libsystemd"
PACKAGECONFIG[with-tests] = "-DBUILD_TESTS=ON -DTESTS_INSTALL_PATH=${libdir}/${BPN}/tests,-DBUILD_TESTS=OFF"

DEPENDS += "expat basu"

SRCREV = "0261d0ec60b68c1f0a6ec9acf63d1379f7d569f8"
SRC_URI = "git://github.com/Kistler-Group/sdbus-cpp.git;protocol=https;branch=master"
SRC_URI += "file://run-ptest"

EXTRA_OECMAKE = "-DBUILD_CODE_GEN=ON \
                 -DBUILD_DOC=ON \
                 -DBUILD_DOXYGEN_DOC=OFF \
                 -DSDBUSCPP_BUILD_LIBSYSTEMD=OFF"

S = "${WORKDIR}/git"

FILES_${PN}:remove = "${sysconfdir}"
FILES_${PN}-ptest += "${sysconfdir}/dbus-1/system.d/"
FILES_${PN}-ptest += "${libdir}/${BPN}/tests"
FILES_${PN}-dev += "${bindir}/sdbus-c++-xml2cpp"
