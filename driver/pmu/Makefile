PWD = $(shell pwd)
KDIR = /lib/modules/$(shell uname -r)/build

obj-m = src/

EXTRA_CFLAGS +=	-std=gnu99					\
		-fno-builtin-memset				\
		-Werror						\
		-Wframe-larger-than=400				\
		-Wno-declaration-after-statement		\

.PHONY: modules modules_install clean load unload

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
