SRCFILES=blackjack.cpp card.cpp card-selector.cpp chips.cpp dialog.cpp draw.cpp events.cpp game.cpp hand.cpp menu.cpp player.cpp press_data.cpp slot.cpp splash.cpp strategy.cpp games-conf.c games-stock.c games-runtime.c games-card-images.c games-card-themes.c games-card-theme.c games-frame.c games-preimage.c games-help.c games-debug.c games-marshal.c games-pixbuf-utils.c games-card.c games-atk-utils.c games-show.c

SRCS=$(addprefix src/,${SRCFILES})

CFLAGS=`pkgconf --cflags --libs gtk+-2.0 glib-2.0 libxml-2.0`

blackjack: ${SRCS}
	gcc -Wno-narrowing -Wno-deprecated-declarations ${SRCS} ${CFLAGS} -L/usr/lib64 -lstdc++ -lm -lz -o $@
