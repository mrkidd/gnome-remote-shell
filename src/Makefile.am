SUBDIRS = help

bin_PROGRAMS = gnome-remote-shell

dialogdir = $(datadir)/$(PACKAGE)/dialogs
dialog_DATA = gnome-remote-shell.glade

INCLUDES = \
	$(GNETWORK_CFLAGS) \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DPREFIX=\""$(prefix)/"\" \
	-DSYSCONFDIR=\""$(sysconfdir)/"\" \
	-DLIBDIR=\""$(libdir)/"\" \
	-DDATADIR=\""$(datadir)/"\" \
	-DDIALOGDIR=\""$(dialogdir)/"\"

gnome_remote_shell_SOURCES = gnome-remote-shell.c gnome-remote-shell.h
gnome_remote_shell_LDADD = $(GNETWORK_LIBS)

pixmapsdir = $(datadir)/pixmaps
pixmaps_DATA = gnome-remote-shell.png

Applicationsdir = $(datadir)/applications
Applications_in_files = gnome-remote-shell.desktop.in
Applications_DATA = $(Applications_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = $(Applications_in_files) \
	$(pixmaps_DATA) \
	$(dialog_DATA)
