CC=gcc 

ACCESSORY_DIR = accessory

OBJDIR = $(ACCESSORY_DIR)/obj
SRCDIR = $(ACCESSORY_DIR)/src
OBJS = $(OBJDIR)/wrapper.o $(OBJDIR)/wrapper_fax.o

$(info objs: $(SRCDIR))

build: $(OBJS)
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c $< -o $@
