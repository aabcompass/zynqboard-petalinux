DESCRIPTION = "PETALINUX image definition for Xilinx boards"
LICENSE = "MIT"

require recipes-core/images/petalinux-image-common.inc 

inherit extrausers
IMAGE_LINGUAS = " "

IMAGE_INSTALL = "\
		kernel-modules \
		mtd-utils \
		canutils \
		dropbear \
		openssh-sftp-server \
		pciutils \
		run-postinsts \
		udev-extraconf \
		packagegroup-core-boot \
		packagegroup-core-ssh-dropbear \
		tcf-agent \
		bridge-utils \
		artix-load-uapp \
		dataprov-uapp \
		dma-uapp \
		init-uapp \
		scurve-adder-uapp \
		dataprov-mod \
		dma-mod \
		hvhk-mod \
		scurve-adder-mod \
		spaciroc-mod \
		"
EXTRA_USERS_PARAMS = "usermod -P root root;"
