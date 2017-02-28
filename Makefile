CC       := gcc
INCLUDES := -I.
CFLAGS   := -Wall -O2 -g
LDFLAGS_GEN  := -lm -lpthread
LDFLAGS_VIEW := -lm -lSDL -lGL -lfftw3f

EXEC_GEN  := gen
EXEC_VIEW := view

COMMON_OBJS := wav.o liss.o
GEN_OBJS := gen.o
VIEW_OBJS := locate.o view.o

ALL_OBJS := $(GEN_OBJS) $(VIEW_OBJS) $(COMMON_OBJS)
ALL_EXECS := $(EXEC_GEN) $(EXEC_VIEW)

ALL_OBJS_DOT = $(join $(dir $(ALL_OBJS)),$(addprefix .,$(notdir $(ALL_OBJS))))
ALL_DEPS = $(ALL_OBJS_DOT:.o=.dep)

.PHONY: clean all

all: $(ALL_EXECS)

$(EXEC_GEN): $(COMMON_OBJS) $(GEN_OBJS)
	$(CC) -o $(EXEC_GEN) $(COMMON_OBJS) $(GEN_OBJS) $(CFLAGS) $(LDFLAGS_GEN)

$(EXEC_VIEW): $(COMMON_OBJS) $(VIEW_OBJS)
	$(CC) -o $(EXEC_VIEW) $(COMMON_OBJS) $(VIEW_OBJS) $(CFLAGS) $(LDFLAGS_VIEW)

%.o: %.c
	@$(CC) $(INCLUDES) -MM -MP -MF $(dir $@).$(notdir $(basename $@)).dep -MT $@ $<
	$(CC) -c $(CFLAGS) $(INCLUDES) -o $@ $<

clean:
	find -name '.*.dep' | xargs rm -f
	find -name '*.o' | xargs rm -f
	rm -f $(ALL_EXECS) *~ */*~

-include $(ALL_DEPS)
