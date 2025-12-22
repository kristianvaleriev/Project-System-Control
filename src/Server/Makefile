src = $(wildcard *.c)
src += $(wildcard ../../lib/*.c)

obj = $(src:.c=.o)
dep = $(obj:.o=.d)

CFLAGS += -MMD -O3 -DWITHOUT_NCURSE

all: $(obj)
	$(CC) $(CFLAGS) -o $(PROG_NAME) $^

-include $(dep)

.PHONY: clean
clean: 
	rm -f $(obj) $(dep)
