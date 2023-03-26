GO ?= go
GOFMT ?= gofmt "-s"
CC = clang
GOFILES := $(shell find . -name "*.go")

CFLAGS := $(CFLAGS) -g -O3 -Wall -Wextra -pedantic -Werror -std=c18 -pthread

all: engine client

engine: 
	$(GO) build -o $@ $(GOFILES)

client: client.c.o
	$(LINK.c) $^ $(LOADLIBES) $(LDLIBS) -o $@

.PHONY: clean
clean:
	rm -f *.o client engine

.PHONY: fmt
fmt:
	$(GOFMT) -w $(GOFILES)

# dependency handling
# https://make.mad-scientist.net/papers/advanced-auto-dependency-generation/#tldr

DEPDIR := .deps
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$<.d

COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c

%.c.o: %.c
%.c.o: %.c $(DEPDIR)/%.c.d | $(DEPDIR)
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(DEPDIR): ; @mkdir -p $@

DEPFILES := $(SRCS:%=$(DEPDIR)/%.d) $(DEPDIR)/client.c.d
$(DEPFILES):

include $(wildcard $(DEPFILES))
