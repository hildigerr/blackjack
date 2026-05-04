SRCFILES=blackjack.cpp card.cpp card-selector.cpp chips.cpp dialog.cpp draw.cpp events.cpp game.cpp hand.cpp menu.cpp player.cpp press_data.cpp slot.cpp splash.cpp strategy.cpp games-conf.c games-stock.c games-runtime.c games-card-images.c games-card-themes.c games-card-theme.c games-card-theme-svg.c games-card-theme-preimage.c games-frame.c games-preimage.c games-help.c games-debug.c games-marshal.c games-pixbuf-utils.c games-card.c games-atk-utils.c games-show.c games-string-utils.c

SRCS=$(addprefix src/,${SRCFILES})

CFLAGS=`pkgconf --cflags --libs gtk+-2.0 glib-2.0 libxml-2.0 librsvg-2.0` -DENABLE_CARD_THEME_FORMAT_SVG -DHAVE_RSVG -DWITH_HELP_METHOD_FILE
LIBS=`pkgconf --libs gtk+-2.0 glib-2.0 libxml-2.0 librsvg-2.0` -L/usr/lib64 -lstdc++ -lm -lz -lrsvg-2

POS := $(wildcard po/*.po)
MOS := $(POS:.po=.mo)

blackjack: ${SRCS}
	gcc -g -Wno-narrowing -Wno-deprecated-declarations ${SRCS} ${CFLAGS} ${LIBS} -o $@

po: $(POS)

mo: $(MOS)

po/%.po: po/blackjack.pot
	msgmerge --update --no-location $@ $<

po/%.mo: po/%.po
	msgfmt $< -o $@

help/blackjack.html:
	xsltproc -o help/blackjack.html /usr/share/xml/docbook/xsl-stylesheets-1.79.2/html/docbook.xsl help/blackjack.xml
