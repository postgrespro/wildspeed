PG_CPPFLAGS = -DOPTIMIZE_WILDCARD_QUERY
MODULE_big = wildspeed
OBJS = wildspeed.o 

DATA_built = wildspeed.sql
DATA = uninstall_wildspeed.sql
REGRESS = wildspeed


subdir = contrib/wildspeed
top_builddir = ../..
include $(top_builddir)/src/Makefile.global

include $(top_srcdir)/contrib/contrib-global.mk
