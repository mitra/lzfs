include $(top_srcdir)/config/rpm.am
include ${top_srcdir}/config/deb.am
include ${top_srcdir}/config/tgz.am

USER_DIR = etc
KERNEL_DIR = module
SUBDIRS = $(KERNEL_DIR) $(USER_DIR)

AUTOMAKE_OPTIONS = foreign dist-zip
EXTRA_DIST  = autogen.sh lzfs.spec.in META 
EXTRA_DIST += config/rpm.am config/deb.am config/tgz.am
noinst_HEADERS = lzfs_config.h 

distclean-local::
	-$(RM) -R autom4te*.cache
	-find . \( -name SCCS -o -name BitKeeper -o -name .svn -o -name CVS \
		-o -name .pc -o -name .hg -o -name .git \) -prune -o \
		\( -name '*.orig' -o -name '*.rej' -o -name '*~' \
		-o -name '*.bak' -o -name '#*#' -o -name '.*.orig' \
		-o -name '.*.rej' -o -name 'aclocal.m4' -o -size 0 \
		-o -name '*%' -o -name '.*.cmd' -o -name 'core' \
		-o -name 'Makefile' -o -name '$(LINUX_SYMBOLS)' \
		-o -name '*.order' -o -name '*.markers' \) \
		-type f -print | xargs $(RM)

install-data-local:
	destname=lzfs-$(LZFS_META_VERSION)/$(LINUX_VERSION); \
	instdest=$(DESTDIR)/${prefix}/src/$$destname; \
	echo "${LZFS_META_VERSION}" >$$instdest/lzfs.release; \
	for instfile in $(noinst_HEADERS) module/$(LINUX_SYMBOLS); do \
		$(INSTALL) -D $$instfile $$instdest/$$instfile; \
	done

ctags:
	$(RM) $(top_srcdir)/tags
	find $(top_srcdir) -name .git -prune -o -name '*.[hc]' | xargs ctags

etags:
	$(RM) $(top_srcdir)/TAGS
	find $(top_srcdir) -name .pc -prune -o -name '*.[hc]' | xargs etags -a

tags: ctags etags

pkg: @DEFAULT_PACKAGE@
pkg-modules: @DEFAULT_PACKAGE@-modules
pkg-utils: @DEFAULT_PACKAGE@-utils
