#
# $Id$
#
# Copyright © 2004 Keith Packard
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of version 2 of the GNU General Public
# License as published by the Free Software Foundation
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
# Author: Keith Packard <keithp@keithp.com>
#

DEBIAN = debian/changelog debian/compat \
	debian/control debian/copyright debian/rules

man_MANS=netforward.1

EXTRA_DIST = autogen.sh $(DEBIAN) ChangeLog $(man_MANS)

bin_PROGRAMS=netforward

debuild debuild-signed: debuild-dirs
	(cd $(distdir)/debian && debuild)

debuild-unsigned: debuild-dirs
	(cd $(distdir)/debian && debuild -us -uc)

debuild-dirs: distdir
	rm -f $(PACKAGE)_$(VERSION).orig.tar.gz
	rm -rf $(distdir).orig
	cp -a $(distdir) $(distdir).orig
	rm -rf $(distdir).orig/debian
