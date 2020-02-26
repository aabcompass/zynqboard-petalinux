#
# This file is the init-uapp recipe.
#

SUMMARY = "Simple init-uapp application"
SECTION = "PETALINUX/apps"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://init-uapp \
	"

S = "${WORKDIR}"

inherit update-rc.d

INITSCRIPT_NAME = "init-uapp"
INITSCRIPT_PARAMS = "start 99 S ."


do_install() {
#	     install -d ${D}/${bindir}
#	     install -m 0755 ${S}/init-uapp ${D}/${bindir}
install -d ${D}${sysconfdir}/init.d
install -m 0755 ${S}/init-uapp ${D}${sysconfdir}/init.d/init-uapp
}

FILES_${PN} += "${sysconfdir}/*"