SUBDIRS = po src

EXTRA_DIST = gnome-remote-shell.spec.in \
        intltool-extract.in \
        intltool-merge.in    \
        intltool-update.in

CLEANFILES = \
        intltool-extract \
        intltool-merge \
        intltool-update

dist-hook: gnome-remote-shell.spec
	cp gnome-remote-shell.spec $(distdir)

