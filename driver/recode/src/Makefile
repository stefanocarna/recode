MODULE_NAME = recode

ccflags-y += -I$(src)/../include 
ccflags-y += -I$(src)/../lib/hooks/include
ccflags-y += -I$(src)/../../pmu/include

$(MODULE_NAME)-objs := recode_config.o
$(MODULE_NAME)-objs += recode_core.o
$(MODULE_NAME)-objs += recode_collector.o
$(MODULE_NAME)-objs += recode_module.o
$(MODULE_NAME)-objs += recode_groups.o
$(MODULE_NAME)-objs += recode_hooks.o
$(MODULE_NAME)-objs += recode_tasks.o
$(MODULE_NAME)-objs += power.o

$(MODULE_NAME)-objs += ../lib/hooks/src/hooks.o


ifneq ($(POP_MODULE),)
	POP_MODULE_DIR = $(src)/../modules/$(POP_MODULE)
	include $(POP_MODULE_DIR)/Makefile

	ccflags-y += -I$(src)/../modules/$(POP_MODULE)/include

	POP_MODULE_OBJECTS_PREFIX = $(addprefix $(POP_MODULE_DIR)/,$(POP_MODULE_OBJECTS))
	POP_MODULE_OBJECTS_NO_PREFIX = $(subst $(src),,$(POP_MODULE_OBJECTS_PREFIX))


	$(MODULE_NAME)-objs += $(POP_MODULE_OBJECTS_NO_PREFIX)
# $(info $($(MODULE_NAME)-objs))

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
