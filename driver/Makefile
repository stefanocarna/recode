PWD = $(shell pwd)
KDIR = /lib/modules/$(shell uname -r)/build

MODULES = pmu/ recode/

obj-m := $(MODULES)

EXTRA_CFLAGS =	-std=gnu99					\
		-fno-builtin-memset				\
		-Werror						\
		-Wframe-larger-than=400				\
		-Wno-declaration-after-statement

# PLUGIN :=
# CFLAGS_MODULE :=

# ifeq ($(PLUGIN), TMA)
# 	CFLAGS_MODULE += -DTMA_MODULE
# endif
# ifeq ($(PLUGIN), SECURITY)
# 	CFLAGS_MODULE += -DSECURITY_MODULE
# endif


# ECHO = echo

.PHONY: modules modules_install clean load unload debug security tma

modules:
	@$(MAKE) -w -C $(KDIR) M=$(PWD) modules

modules_install:
	@$(MAKE) -C $(KDIR) M=$(PWD) modules_install

clean:
	@$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -rf   Module.symvers modules.order

load:
	for mod in $(shell cat modules.order); do sudo insmod $$mod; done

unload:
	for mod in $(shell tac modules.order); do \
		sudo rmmod $(basename $$mod) || (echo "rmmod $$mod failed $$?"; exit 1); \
	done

debug:
	sudo insmod dependencies/pmu/src/pmudrv.ko dyndbg==pmf
	sudo insmod driver/recode.ko dyndbg==pmf
	# for mod in $(shell cat modules.order); do sudo insmod $$mod dyndbg==pmf; done

