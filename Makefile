package = pride-nyancat
version = 2.0.0
tarname = $(package)
distdir = $(tarname)-$(version)

all check nyancat:
	cd src && $(MAKE) $@
	cp src/pride-nyancat .

clean:
	cd src && $(MAKE) clean

dist: $(distdir).tar.gz

$(distdir).tar.gz: $(distdir)
	tar chof - $(distdir) | gzip -9 -c > $@
	rm -rf $(distdir)

$(distdir): FORCE
	mkdir -p $(distdir)/src
	cp Makefile $(distdir)
	cp src/Makefile $(distdir)/src
	cp src/nyancat.c $(distdir)/src
	cp src/animation.c $(distdir)/src
	cp src/telnet.h $(distdir)/src

FORCE:
	-rm $(distdir).tar.gz >/dev/null 2>&1
	-rm -rf $(distdir) >/dev/null 2>&1

distcheck: $(distdir).tar.gz
	gzip -cd $(distdir).tar.gz | tar xvf -
	cd $(distdir) && $(MAKE) all
	cd $(distdir) && $(MAKE) check
	cd $(distdir) && $(MAKE) clean
	rm -rf $(distdir)
	@echo "*** Package $(distdir).tar.gz is ready for distribution."

install: all
	install src/pride-nyancat /usr/local/bin/${package}

.PHONY: FORCE all clean check dist distcheck install
