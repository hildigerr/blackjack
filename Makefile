SRCFILES=blackjack.cpp card.cpp card-selector.cpp chips.cpp dialog.cpp draw.cpp events.cpp game.cpp hand.cpp menu.cpp player.cpp press_data.cpp slot.cpp splash.cpp strategy.cpp games-conf.c games-stock.c games-runtime.c games-card-images.c games-card-themes.c games-card-theme.c games-card-theme-svg.c games-card-theme-preimage.c games-frame.c games-preimage.c games-help.c games-debug.c games-marshal.c games-pixbuf-utils.c games-card.c games-atk-utils.c games-show.c games-string-utils.c
SRCS=$(addprefix src/,${SRCFILES})

CFLAGS=`pkgconf --cflags --libs gtk+-2.0 glib-2.0 libxml-2.0 librsvg-2.0`
DFLAGS=-DENABLE_CARD_THEME_FORMAT_SVG -DHAVE_RSVG -DWITH_HELP_METHOD_FILE
LIBS=`pkgconf --libs gtk+-2.0 glib-2.0 libxml-2.0 librsvg-2.0` -L/usr/lib64 -lstdc++ -lm -lz -lrsvg-2

POS := $(wildcard po/*.po)
MOS := $(POS:.po=.mo)

CARD_THEMES=anglo bellot bonded ornamental paris tango
CARD_RULES=Ameristar Atlantic_City Vegas_Downtown Vegas_Strip

blackjack: ${SRCS}
	gcc -g -Wno-narrowing -Wno-deprecated-declarations ${SRCS} ${CFLAGS} ${DFLAGS} ${LIBS} -o $@

po: $(POS)

mo: $(MOS)

po/%.po: po/blackjack.pot
	msgmerge --update --no-location $@ $<

po/%.mo: po/%.po
	msgfmt $< -o $@

help/blackjack.html:
	xsltproc -o help/blackjack.html /usr/share/xml/docbook/xsl-stylesheets-1.79.2/html/docbook.xsl help/blackjack.xml

help/blackjack.6.gz: data/blackjack.6
	gzip -9c data/blackjack.6 > help/blackjack.6.gz

install: blackjack help/blackjack.6.gz help/blackjack.html mo
	install -Dm755 blackjack ${DESTDIR}/usr/bin/blackjack
	for each in $(CARD_THEMES); do \
		install -Dm644 pixmaps/cards/$$each.svg ${DESTDIR}/usr/share/games/blackjack/cards/$$each.svg; \
	done
	for each in $(CARD_RULES); do \
		install -Dm644 data/$$each.rules ${DESTDIR}/usr/share/games/blackjack/games/$$each.rules; \
	done
	install -Dm644 help/blackjack.html ${DESTDIR}/usr/share/games/blackjack/help/C/blackjack.html
	install -Dm644 help/figures/blackjack_start_window.png \
		${DESTDIR}/usr/share/games/blackjack/help/C/figures/blackjack_start_window.png
	install -Dm644 -t ${DESTDIR}/usr/share/games/blackjack/icons/ pixmaps/icons/menu/*
	install -Dm644 -t ${DESTDIR}/usr/share/games/blackjack/pixmaps/ pixmaps/chip-*.svg
	install -Dm644 -t ${DESTDIR}/usr/share/games/blackjack/pixmaps/ \
		pixmaps/baize.png pixmaps/cards/slots/slot.svg pixmaps/cards/slots/white-outline.svg
	@for each in $(MOS); do \
		lang=$$(basename $$each .mo); \
		install -Dm644 $$each ${DESTDIR}/usr/share/games/blackjack/locale/$$lang/LC_MESSAGES/blackjack.mo; \
		echo "Installed translation for $$lang"; \
	done
	install -Dm644 data/blackjack.desktop ${DESTDIR}/usr/share/applications/blackjack.desktop
	install -Dm644 help/blackjack.6.gz ${DESTDIR}/usr/share/man/man6/blackjack.6.gz
	install -Dm644 pixmaps/icons/hicolor_apps_scalable_gnome-blackjack.svg \
		${DESTDIR}/usr/share/icons/hicolor/scalable/apps/gnome-blackjack.svg
