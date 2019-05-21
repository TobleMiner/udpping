PREFIX ?= /usr

all: udpping

%.o : %.c
	$(CC) -c $< -o $@

clean:
	$(RM) *.o
	$(RM) udpping

udpping: udpping.o
	$(CC) -o $@ $<

install: udpping
	install -m755 $< $(PREFIX)/bin
