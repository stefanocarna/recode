OBJ_NAME = pmudrv

ccflags-y += -I$(src)/../include
# ccflags-y += -I$(src)/../lib/hooks/include

obj-m          += ${OBJ_NAME}.o

$(OBJ_NAME)-objs := pmu_module.o
$(OBJ_NAME)-objs += pmu.o
$(OBJ_NAME)-objs += pmi.o
$(OBJ_NAME)-objs += pmu_config.o
$(OBJ_NAME)-objs += pmu_events.o
$(OBJ_NAME)-objs += pmu_debug.o
$(OBJ_NAME)-objs += multiplexer.o

$(OBJ_NAME)-objs += logic/tma.o

PROC_DIR 	= $(src)/proc
PROC_SOURCES 	= $(wildcard $(PROC_DIR)/*.c)
PROC_SOURCES_NO_PREFIX = $(subst $(src),,$(PROC_SOURCES))
PROC_OBJECTS 	= $(PROC_SOURCES_NO_PREFIX:%.c=%.o)
$(OBJ_NAME)-objs += $(PROC_OBJECTS)

# $(OBJ_NAME)-objs += proc/proc.o
# $(OBJ_NAME)-objs += proc/proc_info.o
# $(OBJ_NAME)-objs += proc/proc_frequency.o
# $(OBJ_NAME)-objs += proc/proc_state.o
# $(OBJ_NAME)-objs += proc/proc_reset.o
# $(OBJ_NAME)-objs += proc/proc_vector.o

# Lib

# $(OBJ_NAME)-objs += ../lib/hooks/src/hooks.o

# ccflags-y += -DDEBUG
