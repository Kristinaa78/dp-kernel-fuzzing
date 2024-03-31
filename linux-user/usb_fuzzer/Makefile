ifndef KAFL_ROOT
$(error KAFL_ROOT is not set. Run "make env" at kAFL root repository)
endif

MAKEF_DIR := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
# linux kafl agent [modified linux kernel]
LINUX_AGENT_DIR := ${EXAMPLES_ROOT}/linux-user/linux_kafl_agent
LINUX_AGENT_BZIMAGE := $(LINUX_AGENT_DIR)/arch/x86/boot/bzImage
# kafl_initrd.cpio.gz [userspace]
GEN_INITRD_DIR := ${EXAMPLES_ROOT}/linux-user/scripts
KAFL_INITRD_PATH := $(GEN_INITRD_DIR)/kafl_initrd.cpio.gz
# usb fuzzer
USB_DIR := $(MAKEF_DIR)/src
TEST_USB_PATH := $(USB_DIR)/sampleUSB
FUZZ_USB_PATH := $(USB_DIR)/fuzzer

# depend on
# - kafl_initrd.cpio.gz in linux-user/scripts/
all: $(KAFL_INITRD_PATH) $(LINUX_AGENT_BZIMAGE) $(TEST_USB_PATH) $(FUZZ_USB_PATH)
	$(MAKE) -C $(GEN_INITRD_DIR)
	$(MAKE) -C $(USB_DIR)
	ln -sf ../src/sampleUSB $(MAKEF_DIR)/sharedir/sampleUSB
	ln -sf ../src/fuzzer $(MAKEF_DIR)/sharedir/fuzzer

clean:
	$(MAKE) -C $(GEN_INITRD_DIR) $@
	$(MAKE) -C $(LINUX_AGENT_DIR) $@
	$(MAKE) -C $(USB_DIR) $@

fuzz: all
	kafl fuzz --purge

# how to build bzImage
$(LINUX_AGENT_BZIMAGE):
	$(MAKE) -C $(LINUX_AGENT_DIR) x86_64_defconfig
	cd $(LINUX_AGENT_DIR) && ./scripts/config --disable MODULE_SIG
	cd $(LINUX_AGENT_DIR) && ./scripts/config --enable DEBUG_INFO_DWARF5
	cd $(LINUX_AGENT_DIR) && ./scripts/config --enable GDB_SCRIPTS
ifdef DVKM_KASAN
	cd $(LINUX_AGENT_DIR) && ./scripts/config --enable KASAN
	cd $(LINUX_AGENT_DIR) && ./scripts/config --enable KASAN_INLINE
endif
ifdef USB_FUZZING
	cd $(LINUX_AGENT_DIR) && ./scripts/config --enable USB_GADGET
	cd $(LINUX_AGENT_DIR) && ./scripts/config --enable USB_RAW_GADGET
	cd $(LINUX_AGENT_DIR) && ./scripts/config --enable USB_DUMMY_HCD
endif
	$(MAKE) -C $(LINUX_AGENT_DIR) olddefconfig
	$(MAKE) -C $(LINUX_AGENT_DIR) -j $(shell nproc)

# how to build kafl_initrd.cpio.gz
$(KAFL_INITRD_PATH):
	$(MAKE) -C $(GEN_INITRD_DIR)

# how to build USB raw gadget test device <sampleUSB.c>
$(TEST_USB_PATH):
	cd $(USB_DIR) && KRN_SOURCES=$(LINUX_AGENT_DIR) $(MAKE)

# how to build USB fuzzer <fuzzer.c>
$(FUZZ_USB_PATH):
	echo "compiling fuzzer.c"
	cd $(USB_DIR) && KRN_SOURCES=$(LINUX_AGENT_DIR) $(MAKE)

.PHONY: clean fuzz
