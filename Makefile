KROOT = /lib/modules/$(shell uname -r)/build

MODULES = dependencies/ driver/

obj-y += $(MODULES)

EXTRA_CFLAGS :=
EXTRA_CFLAGS +=	-std=gnu99					\
		-fno-builtin-memset				\
		-Werror						\
		-Wframe-larger-than=400				\
		-Wno-declaration-after-statement		\
		$(INCLUDES)

.PHONY: modules modules_install clean debug insert remove reboot


modules:
# 	@echo $(wildcard ./driver/*.c)
	@$(MAKE) -w -C $(KROOT) M=$(PWD) modules

modules_install:
	@$(MAKE) -C $(KROOT) M=$(PWD) modules_install

clean:
	@$(MAKE) -C $(KROOT) M=$(PWD) clean
	rm -rf   Module.symvers modules.order

insert:
# 	sudo insmod $(addsuffix *.ko,$(wildcard $(MODULES)))
# 	@echo $(addsuffix *.ko,$(wildcard $(MODULES)))
# 	sudo insmod $(MODULE_NAME).ko
#	sudo insmod dependencies/idt_patcher/idt_patcher.ko
	sudo insmod dependencies/shook/shook.ko
	sudo insmod driver/recode.ko

remove:
	sudo rmmod recode
	sudo rmmod shook
#	sudo rmmod idt_patcher
# 	sudo rmmod $(patsubst %.ko,%,$(addsuffix *.ko,$(wildcard $(MODULES))))

reboot: remove insert