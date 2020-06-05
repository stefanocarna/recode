export MODULE_NAME = recode
export KROOT = /lib/modules/$(shell uname -r)/build

INCLUDES = -I$(src)/../include -I$(src)/idt
# ccflags-y := $(INCLUDE_DIR)

EXTRA_CFLAGS =	-std=gnu99					\
		-fno-builtin-memset				\
		-Werror						\
		-Wframe-larger-than=400				\
		-Wno-declaration-after-statement		\
		$(INCLUDES)

obj-m := $(MODULE_NAME).o

$(MODULE_NAME)-objs := recode_module.o
$(MODULE_NAME)-objs += recode_core.o
$(MODULE_NAME)-objs += device/proc.o
# $(MODULE_NAME)-objs += intel/pmu.o intel/pmi.o intel/pebs.o
# ccflags-y := -I../include

# $(MODULE_NAME)-objs += haap_fops.o

# EXTRA_CFLAGS=-I$(PWD)/../include


modules:
	@$(MAKE) -w -C $(KROOT) M=$(PWD) modules

modules_install:
	@$(MAKE) -C $(KROOT) M=$(PWD) modules_install

clean:
	@$(MAKE) -C $(KROOT) M=$(PWD) clean
	rm -rf   Module.symvers modules.order

insert: modules
	sudo insmod $(MODULE_NAME).ko

remove:
	sudo rmmod $(MODULE_NAME)

reboot: clean insert
