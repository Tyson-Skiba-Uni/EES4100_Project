include $(top_srcdir)/common.am

bin_PROGRAMS = bacnet_server_pp2

bacnet_server_pp2_SOURCES = bacnet_server_pp2.c

bacnet_server_pp2_CFLAGS = $(AM_CFLAGS) $(MODBUS_CFLAGS) $(BACNET_CFLAGS)
bacnet_server_pp2_LDADD = $(AM_LIBS) $(MODBUS_LIBS) $(BACNET_LIBS)

bin_PROGRAMS = modbus_client_pp1

modbus_client_pp1_SOURCES = modbus_client_pp1

modbus_client_pp1_CFLAGS = $(AM_CFLAGS) $(MODBUS_CFLAGS) $(BACNET_CFLAGS)
bmodbus_client_pp1_LDADD = $(AM_LIBS) $(MODBUS_LIBS) $(BACNET_LIBS)

BUILT_SOURCES = bacnet_namespace.h .bacnet_dependent_src_stamp

EXTRA_DIST = bacnet_api_names

.bacnet_dependent_src_stamp: bacnet_api_names $(BACNET_DEPENDENT_SRC)
	cat $< | $(top_srcdir)/scripts/check_namespace.sh $< $?
	touch $@

bacnet_namespace.h: bacnet_api_names
	cat $< | $(top_srcdir)/scripts/build_namespace.sh > $@
