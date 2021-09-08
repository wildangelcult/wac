CC := gcc
MKDIR := mkdir -p

DFLAGS := -g3 -ggdb
CFLAGS := -Wall -MMD -MP $(DFLAGS)

SDIR := src
ODIR := obj

OUT := bin/wac

SRCS := $(shell find $(SDIR) -name *.c)
OBJS := $(SRCS:$(SDIR)/%.c=$(ODIR)/%.o)
DEPS := $(OBJS:.o=.d)

all: $(OBJS)
	$(CC) $(OBJS) -o $(OUT) $(CFLAGS)

$(ODIR)/%.o: $(SDIR)/%.c
	$(MKDIR) $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean run-repl run-script run

clean:
	rm -fr $(ODIR) $(OUT){,.exe}

run-repl:
	./$(OUT)

run-script:
	./$(OUT) script.wac

run: run-repl

-include $(DEPS)
