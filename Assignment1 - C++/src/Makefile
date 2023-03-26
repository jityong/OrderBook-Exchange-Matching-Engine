CC = clang
CXX = clang++

CFLAGS := $(CFLAGS) -g -O3 -Wall -Wextra -pedantic -Werror -std=c18 -pthread
CXXFLAGS := $(CXXFLAGS) -g -O3 -Wall -Wextra -pedantic -Werror -std=c++20 -pthread

all: engine client

SRCS = main.c engine.cpp io.cpp

engine: $(SRCS:%=%.o)
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

client: client.c.o
	$(LINK.c) $^ $(LOADLIBES) $(LDLIBS) -o $@

.PHONY: clean
clean:
	rm -f *.o client engine

# dependency handling
# https://make.mad-scientist.net/papers/advanced-auto-dependency-generation/#tldr

DEPDIR := .deps
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$<.d

COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c
COMPILE.cpp = $(CXX) $(DEPFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c

%.c.o: %.c
%.c.o: %.c $(DEPDIR)/%.c.d | $(DEPDIR)
	$(COMPILE.c) $(OUTPUT_OPTION) $<

%.cpp.o: %.cpp
%.cpp.o: %.cpp $(DEPDIR)/%.cpp.d | $(DEPDIR)
	$(COMPILE.cpp) $(OUTPUT_OPTION) $<

$(DEPDIR): ; @mkdir -p $@

DEPFILES := $(SRCS:%=$(DEPDIR)/%.d) $(DEPDIR)/client.c.d
$(DEPFILES):

include $(wildcard $(DEPFILES))
