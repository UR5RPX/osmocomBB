INCLUDES = $(all_includes) -I$(top_srcdir)/include
AM_CFLAGS = -Wall $(LIBOSMOCORE_CFLAGS)

noinst_LIBRARIES = liblayer23.a
liblayer23_a_SOURCES = l1ctl.c l1l2_interface.c sap_interface.c lapdm.c \
	logging.c networks.c sim.c
