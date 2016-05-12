include $(top_srcdir)/common/common.am

bin_PROGRAMS = modbus_client_ppc1

modbus_client_ppc1_SOURCES = modbus_client_ppc1.c
modbus_client_ppc1_CFLAGS = $(AM_CFLAGS) $(MODBUS_CFLAGS)
modbus_client_ppc1_LDADD = $(AM_LIBS) $(MODBUS_LIBS) \
			$(top_srcdir)/common/libcommon.la


BUILT_SOURCES = bacnet_namespace.h .bacnet_dependent_src_stamp

EXTRA_DIST = bacnet_api_names

.bacnet_dependent_src_stamp: bacnet_api_names $(BACNET_DEPENDENT_SRC)
	cat $< | $(top_srcdir)/scripts/check_namespace.sh $< $?
	touch $@

bacnet_namespace.h: bacnet_api_names
	cat $< | $(top_srcdir)/scripts/build_namespace.sh > $@
