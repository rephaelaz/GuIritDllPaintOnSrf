INCLUDES = -I$(top_srcdir)/$(IRIT_INCLUDE) -I$(top_srcdir)/$(IRIT_MODELER) -I../GuIritDllExtensions

lib_LTLIBRARIES = libGuIritDllExample.la

libGuIritDllExample_la_SOURCES = \
	GuIritDllExample.cpp
	
libGuIritDllExample_la_LDFLAGS = -version-info 0:0:0
libGuIritDllExample_la_LIBADD = $(GUIRIT_EXT_DLL_LIB)
LIBS = $(IRIT_LIBS) $(GRAPDRVS) $(GUIRIT_LIBS)
EXTRA_DIST = Icons
