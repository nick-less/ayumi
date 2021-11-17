
all: ayumi_render decoder player 


ayumi_render: ayumi.o ayumi_render.o load_text.o
	gcc -l SDL2 ayumi.o ayumi_render.o load_text.o -o ayumi_render

decoder: decoder.o ayumi.o load_text.o
	gcc -l SDL2 decoder.o ayumi.o load_text.o -o decoder


player: player.o
	gcc -l SDL2 player.o -o player

decoder.o: decoder.c
	gcc -c decoder.c

player.o: player.c
	gcc -c player.c

ayumi_render.o: ayumi_render.c
	gcc -c ayumi_render.c

load_text.o: load_text.c load_text.h
	gcc -c load_text.c


ayumi.o: ayumi.c ayumi.h
	gcc -O3 -c ayumi.c

.PHONY: clean
clean:
	rm *.o
	