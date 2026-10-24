# TopoPlay project configuration. Preserve the dependency paths separately from
# the application's chosen install folder. Configure with scripts/configure.py.
-include config.local.make
ifneq ($(strip $(FREENECT2_ROOT)),)
FREENECT2_BUILD ?= $(FREENECT2_ROOT)/build
FREENECT2_LIB_DIR ?= $(FREENECT2_BUILD)/lib
PROJECT_CFLAGS += -I$(FREENECT2_ROOT)/include -I$(FREENECT2_BUILD)
PROJECT_LDFLAGS += -L$(FREENECT2_LIB_DIR) -lfreenect2
else
ifneq ($(shell pkg-config --exists libfreenect2 2>/dev/null && echo yes),)
PROJECT_CFLAGS += $(shell pkg-config --cflags libfreenect2)
PROJECT_LDFLAGS += $(shell pkg-config --libs libfreenect2)
else
$(error libfreenect2 not configured. Run python3 scripts/configure.py --freenect2-root /path/to/libfreenect2)
endif
endif
