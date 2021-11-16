OBJ_NAME = pmudrv

ccflags-y += -I$(src)/../include
ccflags-y += -I$(src)/../lib/hooks/include

obj-m          += ${OBJ_NAME}.o

$(OBJ_NAME)-objs := pmu_module.o
$(OBJ_NAME)-objs += pmu.o
$(OBJ_NAME)-objs += pmi.o
$(OBJ_NAME)-objs += pmu_tasks.o
$(OBJ_NAME)-objs += pmu_config.o
$(OBJ_NAME)-objs += pmu_events.o
$(OBJ_NAME)-objs += pmu_hooks.o
$(OBJ_NAME)-objs += pmu_debug.o
$(OBJ_NAME)-objs += multiplexer.o

# PROC_DIR 	:= proc
# PROC_SOURCES 	:= $(wildcard $(PROC_DIR)/*.c)
# PROC_OBJECTS 	:= $(PROC_SOURCES:%.c=%.o)
# $(OBJ_NAME)-objs += $(PROC_OBJECTS)

$(OBJ_NAME)-objs += proc/proc.o
$(OBJ_NAME)-objs += proc/proc_frequency.o
$(OBJ_NAME)-objs += proc/proc_state.o
$(OBJ_NAME)-objs += proc/proc_reset.o

# Lib

$(OBJ_NAME)-objs += ../lib/hooks/src/hooks.o

# ccflags-y += -DDEBUG
