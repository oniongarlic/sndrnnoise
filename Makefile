SRC=sndrnnoise.c
APPS=$(SRC:%.c=%)
LIBS=$(shell pkg-config --cflags --libs sndfile rnnoise) -lm
CFLAGS=-O2 -pipe

.PHONY: default clean

default: $(APPS)

%: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f $(APPS)
