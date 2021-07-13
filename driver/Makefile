export MODULE_NAME = recode
export KROOT = /lib/modules/$(shell uname -r)/build

INCLUDES = -I$(src)/../include -I$(src)/driver/device
# ccflags-y := $(INCLUDE_DIR)

EXTRA_CFLAGS :=
EXTRA_CFLAGS +=	-std=gnu99					\
		-fno-builtin-memset				\
		-Werror						\
		-Wframe-larger-than=400				\
		-Wno-declaration-after-statement		\
		$(INCLUDES)

$(MODULE_NAME)-y := recode_config.o
$(MODULE_NAME)-y += recode_core.o
$(MODULE_NAME)-y += recode_pmi.o
$(MODULE_NAME)-y += recode_statistics.o
$(MODULE_NAME)-y += recode_module.o
$(MODULE_NAME)-y += recode_pmu.o
$(MODULE_NAME)-y += recode_tma.o

DEVICE_DIR 	:= device
DEVICE_SOURCES 	:= $(wildcard $(DEVICE_DIR)/*.c)
DEVICE_OBJECTS 	:= $(DEVICE_SOURCES:%.c=%.o)

# $(MODULE_NAME)-y += $(DEVICE_OBJECTS)

$(MODULE_NAME)-y += device/proc.o
$(MODULE_NAME)-y += device/proc_cpus.o
$(MODULE_NAME)-y += device/proc_events.o
$(MODULE_NAME)-y += device/proc_frequency.o
$(MODULE_NAME)-y += device/proc_processes.o
$(MODULE_NAME)-y += device/proc_state.o
$(MODULE_NAME)-y += device/proc_statistics.o

obj-m := $(MODULE_NAME).o

ECHO = echo

# $(MODULE_NAME)-y += intel/pmu.o intel/pmi.o intel/pebs.o
# ccflags-y := -I../include

# $(MODULE_NAME)-y += haap_fops.o

# EXTRA_CFLAGS=-I$(PWD)/../include

.PHONY: modules modules_install clean insert remove reboot help


modules:
	@$(MAKE) -w -C $(KROOT) M=$(PWD) modules

modules_install:
	@$(MAKE) -C $(KROOT) M=$(PWD) modules_install

clean:
	@$(MAKE) -C $(KROOT) M=$(PWD) clean
	rm -rf   Module.symvers modules.order

insert:
	sudo insmod $(MODULE_NAME).ko

remove:
	sudo rmmod $(MODULE_NAME)

reboot: clean insert

help:
	@$(ECHO) "Targets $(DEVICE_OBJECTS)"
	@$(ECHO) "all     - buildcompile what is necessary"
	@$(ECHO) "clean   - cleanup old .o and .bin"
	@$(ECHO) "insert  - load the compiled module"
	@$(ECHO) "remove  - unload the module (if loaded)"
	@$(ECHO) "reboot  - perform 'remove' and 'insert'"