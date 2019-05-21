PREFIX ?= /usr

all: udpping

%.o : %.c
	$(CC) -c $< -o $@

clean:
	$(RM) src/*.o
	$(RM) udpping

udpping: src/udpping.o
	$(CC) -o $@ $<

install: udpping
	install -m755 $< $(PREFIX)/bin
