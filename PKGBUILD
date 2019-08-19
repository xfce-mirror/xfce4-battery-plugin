# Maintainer: Evangelos Foutras <evangelos@foutrelis.com>
# Contributor: aurelien <aurelien@archlinux.org>
# Contributor: Aurelien Foret <orelien@chez.com>
# Contributor: Jason Clark <mithereal@gmail.com >

pkgname=xfce4-battery-plugin
pkgver=1.1.4
pkgrel=1
pkgdesc="A battery monitor plugin for the Xfce panel"
arch=('x86_64')
license=('GPL2')
url="https://goodies.xfce.org/projects/panel-plugins/xfce4-battery-plugin"
groups=('xfce4-goodies')
depends=('xfce4-panel')
makedepends=('intltool')
#source=(https://archive.xfce.org/src/panel-plugins/$pkgname/${pkgver%.*}/$pkgname-$pkgver.tar.bz2)
#sha256sums=('12be0a44d16bd1e1618513ee64f946814925872db7d1c1188ab1454b00d040a3')

build() {
  cd "$srcdir/$pkgname-$pkgver"

  ./configure \
    --prefix=/usr \
    --sysconfdir=/etc \
    --libexecdir=/usr/lib \
    --localstatedir=/var \
    --disable-static \
    --disable-debug
  make
}

package() {
  cd "$srcdir/$pkgname-$pkgver"
  make DESTDIR="$pkgdir" install
}

# vim:set ts=2 sw=2 et: