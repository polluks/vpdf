# "show" Makefile

ifeq ($(shell uname),MorphOS)
CXX=ppc-morphos-g++-11
CC=ppc-morphos-gcc-11
LD=ppc-morphos-gcc-11
else
CXX=ppc-morphos-g++-12
CC=ppc-morphos-gcc-12
LD=ppc-morphos-gcc-12
endif
SHELL = /bin/sh

FREETYPESHARED  = ../../libs/freetype/library/lib/
ROOTPATH=../../../
APPLICATION    = VPDF
FILTEROUT = CVS
CATALOGS = $(filter-out $(FILTEROUT), $(shell ls catalogs))
CATALOGDIR     = MorphOS/Locale/Catalogs
NEWCATCOMP = $(ROOTPATH)morphoswb/development/tools/catcomp/catcomp.pl

POPPLERVER = 26.01.0
JPEGVER = 9f

VERBOSE ?= 0
ifneq ($(BUILD_QUIET_COMPILE), y)
	VERBOSE = 0
endif

ifeq ($(wildcard ../showgirls/History),)
   INMORPHOS=y
else
   INMORPHOS=y
endif

ifeq ($(VERBOSE), 0)
	OUT   = @
	ECHO  = @echo
	QUIET = >/dev/null
else
	OUT   = @
	ECHO  = @echo >/dev/null
	QUIET =
endif

FONTCONFIGINC = -I../../libs/fontconfig/MorphOS/include

CPPFLAGS = -g -O3 -Wno-write-strings -Wall -std=c++20 -ffp-contract=fast -mcpu=750 -noixemul -fno-inline-functions -Isystem -Lsystem -Imcc -Lmcc -I. -Llib -Ipoppler-$(POPPLERVER)/poppler/ -Ipoppler-$(POPPLERVER) -Ljpeg-$(JPEGVER)

ifeq ($(INMORPHOS),y)
   CPPFLAGS += -I../showgirls -I../showgirls/system -I../../libs/cairo/MorphOS/os-include/cairo -I../../libs/cairo/MorphOS/os-include -I../../../include  -I../../libs/freetype/library/include ${FONTCONFIGINC} -I../../libs/freetype/include
   CPPFLAGS += -L$(FREETYPESHARED)
else
   # For building with the public SDK
   CPPFLAGS += -Iinclude -Iinclude/cairo -Lsystem
endif

#selection between shared libs and staticly linnked
OBJS = locale.o  vpdf.o poppler_io.o poppler.o clipboard.o window.o application.o logger.o settings.o  \
       fontcache.o arrowstring_class.o  pagenumberclass.o printer.o reggae_export.o  poppler_printer.o printer_image.o \
       poppler_export.o synctex.o

#selection between shared libs and staticly linnked
CAIRO_LIBS = ${FONTCONFIGLIB} -lcairo
POPPLER_LIBS = -lpoppler -lfofi -lpoppler-splash -lgoo
LIBS_SHARED = -Lpoppler-$(POPPLERVER)/poppler/ -Lpoppler-$(POPPLERVER)/fofi/ -Lpoppler-$(POPPLERVER)/splash -Lpoppler-$(POPPLERVER)/goo $(CAIRO_LIBS) -L../showgirls/system -L../showgirls/mcc -L../../libs/fontconfig/MorphOS/lib/libnix -L../../libs/cairo/MorphOS/lib/libnix -L../../libs/freetype/library/lib -L. -L../../../gen/host/libnix/lib -I../showgirls -lmuiclasses -lsystem -ldebug -lsyscall
LIBS=$(LIBS_SHARED) $(POPPLER_LIBS) $(CAIRO_LIBS) -lsystem -lfreetypeauto -ljpeg_float  -lopenjp2 -lpthread -llcms2 -lstdc++ -lm -ldebug -lsyscall


.PHONY: all install install-iso clean

all: vpdf

vpdf: VPDF_strings.h catalogs libs version.h poppler-$(POPPLERVER)/.extractedandpatched jpeg-$(JPEGVER)/.extractedandpatched $(OBJS)
	make -C poppler-$(POPPLERVER) CC=$(CC) CXX=$(CXX)
	make -C jpeg-$(JPEGVER) -j$(shell nproc) CC="$(CC) -noixemul"
	make -C mcc -j$(shell nproc) CC=$(CC) CXX=$(CXX) LD=$(LD)
	$(LD) $(CPPFLAGS) -o $@.db $(OBJS) $(LIBS)
	ppc-morphos-strip $@.db --remove-section=.comment --strip-unneeded -o $@
	chmod u+x $@

fontcache: fontcache.c
	$(CC) $(CPPFLAGS) -DSTANDALONE -c fontcache.c
	$(CC) $(CPPFLAGS) -o fontcache fontcache.o $(LIBS_SHARED) -lfreetypeauto

test: test.o
	path
	$(CC) $(CPPFLAGS) -o test.exe test.o $(LIBS)

libs:
ifeq ($(INMORPHOS),y)
	make -C ../../libs/cairo
	make -C ../../libs/pixman
	make -C ../../libs/freetype
	make -C ../showgirls/system
endif

ifeq ($(INMORPHOS),n)
LIBSYSTEMOBJS = $(addprefix system/,altivec.o directory.o dlist.o file.o functions.o init.o memory.o timer.o)

system/libsystem.a: CC=ppc-morphos-gcc
system/libsystem.a: CPPFLAGS=-O2 -Wall -fvec -I. -Iinclude
system/libsystem.a: $(LIBSYSTEMOBJS)
	rm -f $@
	ppc-morphos-ar rcs $@ $^

all: system/libsystem.a
endif

VERSIONDATE	 = `date +%d.%m.%y`
VERSION  = 1
REVISION = 5

version.h: FORCE
	@echo 'updating version.h'
	@echo '#define VERSION  "'${VERSION}'"'  >>version.h.new
	@echo '#define REVISION "'${REVISION}'"' >>version.h.new
	@echo '#define VERSIONDATE  "'${VERSIONDATE}'"'  >>version.h.new
	@echo '#define POPPLERVER  "'${POPPLERVER}'"'  >>version.h.new
	@# only update when required prevent senseless rebuilds
	@cmp -s version.h version.h.new 2>/dev/null || mv version.h.new version.h
	@rm -f version.h.new


.c.o:
	$(CC) $(CPPFLAGS) -c $< -o $@
.SUFFIXES: .cpp .c .o

clean:
	make -C mcc clean
	#make -C poppler-$(POPPLERVER) clean
	make -C jpeg-9f clean
	rm -f *.o VPDF VPDF.db VPDF_strings.h 
	rm -rf poppler-$(POPPLERVER)

install-iso: all
	mkdir -p $(ISOPATH)Applications/VPDF
	mkdir -p $(ISOPATH)Applications/VPDF/Fonts
	cp -r fonts/* $(ISOPATH)Applications/VPDF/Fonts
	cp vpdf $(ISOPATH)Applications/VPDF/VPDF
	cp vpdf.info $(ISOPATH)Applications/VPDF/VPDF.info
	for i in `find $(ISOPATH)Applications/VPDF/ -name "CVS"`; do rm -rf $$i; done
	for language in $(CATALOGS); \
	do \
		cp -f catalogs/$$language/$(APPLICATION).catalog  $(ISOPATH)/MorphOS/Locale/Catalogs/$$language/; \
	done

install: all
	mkdir -p /sys/Applications/VPDF
	mkdir -p /sys/Applications/VPDF/Fonts
	cp -r fonts/* /sys/Applications/VPDF/Fonts
	cp vpdf /sys/Applications/VPDF/VPDF
	cp vpdf.info /sys/Applications/VPDF/VPDF.info
	for i in `find /sys/Applications/VPDF/ -name "CVS"`; do rm -rf $$i; done
	for language in $(CATALOGS); \
	do \
		cp -f catalogs/$$language/$(APPLICATION).catalog /MOSSYS/Locale/Catalogs/$$language/; \
	done
	
source: $(APPLICATION)_strings.h
	(cd .. && tar --exclude "*.info" -cf $(SOURCEPATH)vpdf.tar vpdf)

	(cd ../showgirls/system && tar --transform "s,^,vpdf/system/," -rf $(SOURCEPATH)vpdf.tar altivec.h chunky.h data.h directory.h dlist.h file.h functions.h gentexture.h loadpng.h loadsunrast.h memory.h system.h timer.h)
	(cd ../showgirls/system && tar --transform "s,^,vpdf/system/," -rf $(SOURCEPATH)vpdf.tar altivec.c directory.c dlist.c file.c functions.c init.c memory.c timer.c)

source-postgg:
	(cd ../../libs/cairo && make headers lib/libnix/libcairo.a)
	(cd ../../libs/freetype/library && make sdk)

	(cd ../../development/os-include && tar --transform "s,^,vpdf/include/," -rf $(SOURCEPATH)vpdf.tar private/vapor/vapor.h)
	(cd ../../libs/cairo/MorphOS && tar --transform "s,^,vpdf/," -rf $(SOURCEPATH)vpdf.tar os-include)
	(cd ../../libs/freetype && tar --transform "s,^,vpdf/," -rf $(SOURCEPATH)vpdf.tar include)
	(cd ../../libs/freetype/library && tar --transform "s,^,vpdf/," -rf $(SOURCEPATH)vpdf.tar include)
	(cd ../../development/include && tar --transform "s,^,vpdf/include/," -rf $(SOURCEPATH)vpdf.tar fontconfig)

	(cd ../../libs/cairo/MorphOS/lib/libnix && tar --transform "s,^,vpdf/lib/," -rf $(SOURCEPATH)vpdf.tar libcairo.a)
	(cd ../../libs/freetype/library/lib && tar --transform "s,^,vpdf/lib/," -rf $(SOURCEPATH)vpdf.tar libfreetypeauto.a)

dump:
	objdump --section-headers --all-headers --reloc --disassemble-all --line-numbers vpdf.db >ram:zr.dump

dump-fontcache:
	objdump --section-headers --all-headers --reloc --disassemble-all fontcache >ram:fontcache.dump

dumptest:
	objdump --section-headers --all-headers --reloc --disassemble-all test.exe >ram:test.dump


bz2: all
	rm -f vpdf.bz2
	bzip2 -k vpdf

poppler-$(POPPLERVER)/.extractedandpatched: poppler-$(POPPLERVER).tar.xz poppler-$(POPPLERVER).diff
	rm -rf poppler-$(POPPLERVER)
	tar -xf poppler-$(POPPLERVER).tar.xz
	(cd poppler-$(POPPLERVER) && rm -rf glib qt4 qt5 m4 cmake)
	(cd poppler-$(POPPLERVER) && rm -rf *.in *.cmake *.m4 Makefile.am install-sh gtk-doc.make ltmain.sh CMakeLists.txt compile depcomp config* test-driver missing)
	(cd poppler-$(POPPLERVER) && patch -p1 <../poppler-$(POPPLERVER).diff)
	touch $@

jpeg-$(JPEGVER)/.extractedandpatched: jpegsrc.v$(JPEGVER).tar.xz jpegsrc.v$(JPEGVER).diff
	rm -rf jpeg-$(JPEGVER)
	tar -xf jpegsrc.v$(JPEGVER).tar.xz
	(cd jpeg-$(JPEGVER) && patch -p1 <../jpegsrc.v$(JPEGVER).diff)
	touch $@

FORCE:


vpdf.o:		    vpdf.c mcc/classes.h poppler.h version.h locale.h VPDF_strings.h
application.o:	application.c application.h poppler.h version.h
logger.o:		logger.c mcc/classes.h application.h
settings.o:		settings.c mcc/classes.h settings.h
poppler.o:		poppler.cpp poppler.h poppler_device.h poppler-$(POPPLERVER)/.extractedandpatched
poppler_printer.o:		poppler_printer.cpp poppler_device.h poppler_printer.h
poppler_io.o:	poppler_io.cpp poppler_io.h
synctex.o:		synctex.c synctex.h
fontcache.o:	fontcache.c fcstr.c fontcache.h
printer.o:      printer.c mcc/classes.h VPDF_strings.h poppler_printer.h
pagenumberclass.o: pagenumberclass.c pagenumberclass.h
arrowstring_class.o: arrowstring_class.c arrowstring_class.h
printer.o: 		printer.c VPDF_strings.h
reggae_export.o: 		reggae_export.c reggae_export.h VPDF_strings.h
poppler_export.o:		poppler_export.cpp poppler_device.h poppler_export.h
clipboard.o:    clipboard.c clipboard.h
printer_image.o:  printer_image.c  printer_image.h
locale.o: locale.c VPDF_strings.h

#  locale
#
$(ROOTPATH)morphoswb/development/tools/flexcat/flexcat:
	cd $(ROOTPATH)morphoswb/development/tools/flexcat && $(MAKE)

$(APPLICATION)_strings.h: $(ROOTPATH)morphoswb/development/tools/flexcat/flexcat $(APPLICATION).cd
ifeq ($(INMORPHOS),y)
	$(ECHO) "    $(COL_INFO)[CATCOMP]$(COL_END) $@"
	$(OUT)$(NEWCATCOMP) --no-code --no-block -cd $(APPLICATION).cd --cfile $@
endif

.PHONY: catalogs
catalogs: $(patsubst %,catalogs/%/$(APPLICATION).catalog,$(CATALOGS))

%.catalog: %.ct $(APPLICATION).cd
	$(ECHO) "    $(COL_INFO)[CATCOMP]$(COL_END) $@"
	$(OUT)$(NEWCATCOMP) -u -cd $(APPLICATION).cd -ct $< -o $@


