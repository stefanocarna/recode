export MODULE_NAME = recode
export KROOT = /lib/modules/$(shell uname -r)/build

# PWD            := $(shell pwd)

ccflags-y = -I$(src)/
ccflags-y += -I$(src)/../include 
ccflags-y += -I$(src)/../lib/hooks/include
ccflags-y += -I$(src)/../../pmu/include


EXTRA_CFLAGS +=	-std=gnu99					\
		-fno-builtin-memset				\
		-Werror						\
		-Wframe-larger-than=400				\
		-Wno-declaration-after-statement		\

$(MODULE_NAME)-objs := recode_config.o
$(MODULE_NAME)-objs += recode_core.o
$(MODULE_NAME)-objs += recode_collector.o
$(MODULE_NAME)-objs += recode_module.o
$(MODULE_NAME)-objs += recode_groups.o

$(MODULE_NAME)-objs += ../lib/hooks/src/hooks.o

# Logic (use flag to define one logic at time)
ifeq ($(findstring -DTMA_MODULE,$(CFLAGS_MODULE)),-DTMA_MODULE)
$(MODULE_NAME)-objs += logic/recode_tma.o
endif
ifeq ($(findstring -DSECURITY_MODULE,$(CFLAGS_MODULE)),-DSECURITY_MODULE)
$(MODULE_NAME)-objs += logic/recode_security.o
$(MODULE_NAME)-objs += device/proc_mitigations.o
$(MODULE_NAME)-objs += device/proc_thresholds.o
$(MODULE_NAME)-objs += device/proc_security.o
$(MODULE_NAME)-objs += device/proc_statistics.o
endif

DEVICE_DIR 	:= device
DEVICE_SOURCES 	:= $(wildcard $(DEVICE_DIR)/*.c)
DEVICE_OBJECTS 	:= $(DEVICE_SOURCES:%.c=%.o)

# $(MODULE_NAME)-m += $(DEVICE_OBJECTS)

$(MODULE_NAME)-m += device/proc.o
$(MODULE_NAME)-m += device/proc_cpus.o
# $(MODULE_NAME)-m += device/proc_frequency.o
# $(MODULE_NAME)-m += device/proc_sample_info.o
$(MODULE_NAME)-m += device/proc_processes.o
$(MODULE_NAME)-m += device/proc_state.o

obj-m := $(MODULE_NAME).o

ECHO = echo


.PHONY: modules modules_install clean insert debug remove reboot help


modules:
	@$(MAKE) -w -C $(KROOT) M=$(PWD) modules

modules_install:
	@$(MAKE) -C $(KROOT) M=$(PWD) modules_install

clean:
	@$(MAKE) -C $(KROOT) M=$(PWD) clean
	rm -rf Module.symvers modules.order

insert:
	sudo insmod $(MODULE_NAME).ko

debug: ccflags-y+=-DDEBUG
debug: insert

remove:
	sudo rmmod $(MODULE_NAME)

reboot: clean insert

help:
	@$(ECHO) "Targets $(DEVICE_OBJECTS)"
	@$(ECHO) "all     - buildcompile what is necessary"
	@$(ECHO) "clean   - cleanup old .o and .bin"
	@$(ECHO) "insert  - load the compiled module"
	@$(ECHO) "debug   - load the compiled module with debug prints"
	@$(ECHO) "remove  - unload the module (if loaded)"
	@$(ECHO) "reboot  - perform 'remove' and 'insert'"