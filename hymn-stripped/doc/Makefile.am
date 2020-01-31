# $Id: Makefile.am,v 1.4 2004/05/10 00:16:59 playfair Exp $
#
# To use this Makefile you need to install:
#   for HTML - xsltproc (http://xmlsoft.org/)
#   for TXT  - links (http://links.sourceforge.net/)
#            - cat, mv, rm and sed (standard unix utilities)
#   for PDF  - fop (http://xml.apache.org/dist/fop
#            - Jimi (http://java.sun.com/products/jimi)
#
# ***NOTE: there is currently a bug in FOP that prevents the manual
#          from being rendered to PDF properly.  I have patched the
#          stylesheet on my system to workaround the issue. <sigh>

PROGRAM=hymn
MANUAL=$(PROGRAM)-manual

# so that dist / distcheck targets work
EXTRA_DIST = cocoa_ui-screenshot-converting.png \
                cocoa_ui-screenshot-done.png \
                cocoa_ui-screenshot-error.png \
                cocoa_ui-screenshot-postdrag.png \
                cocoa_ui-screenshot-predrag.png \
                $(MANUAL).docbook \
                $(MANUAL).html \
                $(MANUAL).pdf \
                $(MANUAL).txt

FOP=@FOP@
LINKS=@LINKS@
XSLTPROC=@XSLTPROC@
XSLTPROC_OPTS=--stringparam section.autolabel 1 --stringparam section.label.includes.component.label 1

# These need to be reset on every system manually since I don't know
# how to have autoconf detect them.
XSL_HTML_SINGLE=/sw/share/xml/xsl/docbook-xsl/xhtml/docbook.xsl
XSL_FO=/sw/share/xml/xsl/docbook-xsl/fo/docbook.xsl

all: $(MANUAL).html $(MANUAL).pdf $(MANUAL).txt

%.fo: %.docbook *.png
	if [ "" != "$(XSLTPROC)" ]; then \
	if [ -f $(XSL_FO) ]; then \
	$(XSLTPROC) $(XSLTPROC_OPTS) -o $@ file://$(XSL_FO) $<; \
	else \
	echo "Set XSL_FO manually in doc/Makefile.in."; \
	fi; \
	fi

%.html: %.docbook
	if [ "" != "$(XSLTPROC)" ]; then \
	if [ -f $(XSL_HTML_SINGLE) ]; then \
	$(XSLTPROC) $(XSLTPROC_OPTS) -o $@ file://$(XSL_HTML_SINGLE) $<; \
	else \
	echo "Set XSL_HTML_SINGLE manually in doc/Makefile.in."; \
	fi; \
	fi

%.pdf: %.fo
	if [ "" != "$(FOP)" ]; then \
	$(FOP) -fo $< -pdf $@; \
	fi

%.txt: %.html
	if [ "" != "$(LINKS)" ]; then \
	$(LINKS) -dump $< > $@; \
	mv $@ $@.orig; \
	cat $@.orig | sed "s/\\\\001/\ /g" > $@; \
	rm $@.orig; \
	fi

clean:
	rm -f .log *.html *.fo *.log *.pdf *.txt

# EOF
