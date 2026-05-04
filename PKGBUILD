# Maintainer: Hildigerr Vergaray <Maintainer at YmirSystems dot com>

pkgname=gnome-blackjack
pkgver=2.28.2
pkgrel=1
pkgdesc="Multiple deck, casino rules blackjack game resurected from gnome-games"
arch=('x86_64')
url="https://github.com/hildigerr/blackjack"
license=('GPL-2.0-or-later')
depends=(gtk2)
makedepends=(git docbook-xsl)

source=("$pkgname::git+$url")

md5sums=('SKIP')


build() {
  cd "$srcdir/$pkgname"
  make
}

package() {
  cd "$srcdir/$pkgname"
  make DESTDIR="$pkgdir/" install
}

