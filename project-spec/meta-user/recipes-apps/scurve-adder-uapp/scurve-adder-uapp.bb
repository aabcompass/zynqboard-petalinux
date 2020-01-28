#
# This file is the scurve-adder-uapp recipe.
#

SUMMARY = "Simple scurve-adder-uapp application"
SECTION = "PETALINUX/apps"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://scurve-adder-uapp.c \
	   file://scurve-adder-mod-intf.h \
	   file://Makefile \
		  "

S = "${WORKDIR}"

do_compile() {
	     oe_runmake
}

do_install() {
	     install -d ${D}${bindir}
	     install -m 0755 scurve-adder-uapp ${D}${bindir}
}
