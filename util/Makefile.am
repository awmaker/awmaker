SUBDIRS = po

AUTOMAKE_OPTIONS =

pkgdatadir  = $(datadir)/@PACKAGE@

bin_PROGRAMS = wxcopy wxpaste wdwrite wdread getstyle setstyle convertfonts \
	seticons geticonset wmsetbg wmagnify wmgenmenu wmmenugen wmiv

bin_SCRIPTS = wm-oldmenu2new wkdemenu.pl

EXTRA_DIST = bughint wm-oldmenu2new wkdemenu.pl

AM_CPPFLAGS = \
	 $(DFLAGS) $(WINGs_CFLAGS) $(WRASTER_CFLAGS) \
	@HEADER_SEARCH_PATH@ \
	-DETCDIR=\"sysconfdir\" -DDATADIR=\"pkgdatadir\"

liblist= @LIBRARY_SEARCH_PATH@ @INTLIBS@

wdwrite_LDADD = $(WINGs_LIBS) $(liblist)

wdread_LDADD = $(WINGs_LIBS) $(liblist)

wxcopy_LDADD = @XLFLAGS@ @XLIBS@

wxpaste_LDADD = @XLFLAGS@ @XLIBS@

getstyle_LDADD = $(WINGs_LIBS) $(liblist)

getstyle_SOURCES = getstyle.c fontconv.c common.h

setstyle_LDADD = \
	$(WINGs_LIBS) \
	@XLFLAGS@ @XLIBS@ $(liblist)

setstyle_SOURCES = setstyle.c fontconv.c common.h

convertfonts_LDADD = $(WINGs_LIBS) $(liblist)

convertfonts_SOURCES = convertfonts.c fontconv.c common.h

seticons_LDADD= $(WINGs_LIBS) $(liblist)

geticonset_LDADD= $(WINGs_LIBS) $(liblist)

wmagnify_LDADD = \
	$(WINGs_LIBS) \
	$(WRASTER_LIBS) \
	@XLFLAGS@ @XLIBS@ @INTLIBS@

wmsetbg_LDADD = \
	$(WINGs_LIBS) \
	$(WRASTER_LIBS) \
	@XLFLAGS@ @LIBXINERAMA@ @XLIBS@ @INTLIBS@

wmgenmenu_LDADD = \
	$(WINGs_LIBS) \
	@INTLIBS@

wmgenmenu_SOURCES = wmgenmenu.c wmgenmenu.h

wmmenugen_LDADD = \
	$(WINGs_LIBS) \
	@INTLIBS@

wmmenugen_SOURCES = wmmenugen.c wmmenugen.h wmmenugen_misc.c \
	wmmenugen_parse_wmconfig.c \
	wmmenugen_parse_xdg.c

wmiv_LDADD = \
	$(WRASTER_LIBS) \
	@XLFLAGS@ @XLIBS@ \
	@GFXLIBS@ $(PTHREAD_CFLAGS) $(PTHREAD_LIBS) $(LIBEXIF)

wmiv_SOURCES = wmiv.c wmiv.h
