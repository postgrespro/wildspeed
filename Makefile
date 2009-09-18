PG_CPPFLAGS = -DOPTIMIZE_WILDCARD_QUERY
MODULE_big = wildspeed
OBJS = wildspeed.o 

DATA_built = wildspeed.sql
DATA = uninstall_wildspeed.sql
REGRESS = wildspeed

ifdef USE_PGXS
PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
else
subdir = contrib/wildspeed
top_builddir = ../..
include $(top_builddir)/src/Makefile.global
include $(top_srcdir)/contrib/contrib-global.mk
endif