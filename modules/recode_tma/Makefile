PWD = $(shell pwd)
KDIR = /lib/modules/$(shell uname -r)/build

DRIVER_PATH = ../../driver


# Libs

HOOKS_INCLUDES = $(src)/$(DRIVER_PATH)/lib/hooks/include
HOOKS_BASEDIR = $(DRIVER_PATH)/lib/hooks/src/
HOOKS_SRCS = hooks.o

HOOKS_OBJS = $(addprefix $(HOOKS_BASEDIR), $(HOOKS_SRCS))


# Pmu

PMU_MODULE_DIR = $(DRIVER_PATH)/pmu/
PMU_INCLUDES = $(src)/$(DRIVER_PATH)/pmu/include 
# PMU_BASEDIR = $(DRIVER_PATH)/pmu/src/

# # src
# PMU_SRCS += pmu.o
# PMU_SRCS += pmi.o
# PMU_SRCS += pmu_tasks.o
# PMU_SRCS += pmu_config.o
# PMU_SRCS += pmu_events.o
# PMU_SRCS += pmu_hooks.o
# PMU_SRCS += pmu_debug.o
# PMU_SRCS += multiplexer.o
# #src/proc
# PMU_SRCS += proc/proc.o
# PMU_SRCS += proc/proc_frequency.o
# PMU_SRCS += proc/proc_state.o
# PMU_SRCS += proc/proc_reset.o

# PMU_OBJS = $(addprefix $(PMU_BASEDIR), $(PMU_SRCS))


# Recode

RECODE_INCLUDES = $(src)/$(DRIVER_PATH)/recode/include
RECODE_BASEDIR = $(DRIVER_PATH)/recode/src/

# src
# RECODE_SRCS := recode_module.o
RECODE_SRCS += recode_config.o
RECODE_SRCS += recode_core.o
RECODE_SRCS += recode_collector.o
RECODE_SRCS += recode_groups.o
#src/device
RECODE_SRCS += device/proc.o
RECODE_SRCS += device/proc_cpus.o
RECODE_SRCS += device/proc_processes.o
RECODE_SRCS += device/proc_state.o

RECODE_OBJS = $(addprefix $(RECODE_BASEDIR), $(RECODE_SRCS))

RECODE_PLUGINS_BASEDIR = $(RECODE_BASEDIR)plugins/

RECODE_PLUGINS_SRCS = recode_tma.o

RECODE_PLUGINS_OBJS = $(addprefix $(RECODE_PLUGINS_BASEDIR), $(RECODE_PLUGINS_SRCS))

# Module objects

MODNAME = recode_tma_module
obj-m = $(PMU_MODULE_DIR) $(MODNAME).o

$(MODNAME)-objs = $(HOOKS_OBJS)
# $(MODNAME)-objs += $(PMU_OBJS)
$(MODNAME)-objs += $(RECODE_OBJS)
$(MODNAME)-objs += $(RECODE_PLUGINS_OBJS)

$(MODNAME)-objs += recode_tma.o

ccflags-y = -I$(HOOKS_INCLUDES)
ccflags-y += -I$(PMU_INCLUDES)
ccflags-y += -I$(RECODE_INCLUDES)

EXTRA_CFLAGS =	-std=gnu99					\
		-fno-builtin-memset				\
		-Werror						\
		-Wframe-larger-than=400				\
		-Wno-declaration-after-statement

.PHONY: modules modules_install clean

modules:
	@ echo $(ccflags-y);
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

reboot: unload modules load