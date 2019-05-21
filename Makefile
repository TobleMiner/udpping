PREFIX ?= /usr

all: udpping

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) src/*.o
	$(RM) udpping

udpping: src/udpping.o
	$(CC) $(CFLAGS) -o $@ $<

install: udpping
	install -m755 $< $(PREFIX)/bin
