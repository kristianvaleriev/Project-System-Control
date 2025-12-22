src = $(wildcard *.c)
src += $(wildcard ../../lib/*.c)

obj = $(src:.c=.o)
dep = $(obj:.o=.d)

CFLAGS += -MMD  -lncurses

all: $(obj)
	$(CC) $(CFLAGS) -o $(PROG_NAME) $^

-include $(dep)

.PHONY: clean
clean: 
	rm -f $(obj) $(dep)
