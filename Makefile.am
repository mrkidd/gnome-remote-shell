SUBDIRS = po src

EXTRA_DIST = gnome-network.spec.in \
        intltool-extract.in \
        intltool-merge.in    \
        intltool-update.in

CLEANFILES = \
        intltool-extract \
        intltool-merge \
        intltool-update

dist-hook: gnome-network.spec
	cp gnome-network.spec $(distdir)

