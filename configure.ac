AC_INIT([modbus_client_pp1], [0.1])    #filename is modbus_client_ppc1
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])               #not required but keeps things neat
AC_CONFIG_HEADERS([config/config.h])    #neatness

AM_INIT_AUTOMAKE
AM_SILENT_RULES([yes])                  #will print only major lines such as CC and CCG

LT_INIT					#initialise lib tool

AC_PROG_CC
AM_PROG_CC_C_O

PKG_CHECK_MODULES([MODBUS], [libmodbus]) #Checks for Modbus Library on Local Machine
PKG_CHECK_MODULES([BACNET], [libbacnet]) #Checks for Bacnet Library on Local Machine

AC_SEARCH_LIBS([pthread_create], [pthread], [],           #[] action if found is to do nothing
    AC_MSG_ERROR([Unable to Find The Pthread Library!]))  #Warn that no pthread library is found

AC_CONFIG_FILES([
    Makefile
    scripts/Makefile
    common/Makefile                     
    src/Makefile
])                                   #unsure why i need common but it was in the test bench configure

AC_OUTPUT
