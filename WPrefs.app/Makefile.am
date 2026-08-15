SUBDIRS = xpm tiff po

AUTOMAKE_OPTIONS =

wpexecbindir = @wprefs_bindir@

wpexecbin_PROGRAMS = WPrefs

wpdatadir = @wprefs_datadir@

wpdata_DATA = WPrefs.tiff WPrefs.xpm

EXTRA_DIST = $(wpdata_DATA) \
	KeyboardSettings.c \
	Themes.c

WPrefs_SOURCES = \
	main.c \
	WPrefs.c \
	WPrefs.h \
	Appearance.c \
	Configurations.c \
	Docks.c \
	Expert.c \
	Focus.c \
	FontSimple.c \
	HotCornerShortcuts.c \
	Icons.c \
	KeyboardShortcuts.c \
	Menu.c \
	MenuPreferences.c \
	MouseSettings.c \
	Paths.c \
	Preferences.c \
	TexturePanel.c \
	TexturePanel.h \
	WindowHandling.c \
	Workspace.c \
	double.c \
	editmenu.c \
	editmenu.h \
	xmodifier.c

# not_yet_fully_implemented
#WPrefs_SOURCES += \
#	KeyboardSettings.c \
#	Themes.c

AM_CFLAGS = @PANGO_CFLAGS@

AM_CPPFLAGS = -DRESOURCE_PATH=\"$(wpdatadir)\" -DWMAKER_RESOURCE_PATH=\"$(pkgdatadir)\" \
	 -I$(top_srcdir)/awmcommon \
	 -I$(top_builddir) \
	 $(WINGs_CFLAGS) $(WRASTER_CFLAGS) \
	 @HEADER_SEARCH_PATH@

WPrefs_DEPENDENCIES =

WPrefs_LDADD = \
	$(WINGs_LIBS)\
	$(WRASTER_LIBS) \
	@XLFLAGS@ @XLIBS@ \
	@LIBM@ \
	@FCLIBS@ \
	@INTLIBS@
