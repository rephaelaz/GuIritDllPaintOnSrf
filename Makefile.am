INCLUDES = -I$(top_srcdir)/$(IRIT_INCLUDE) -I$(top_srcdir)/$(IRIT_MODELER) -I../GuIritDllExtensions

lib_LTLIBRARIES = libGuIritDllPaintOnSrf.la

libGuIritDllPaintOnSrf_la_SOURCES = \
	GuIritDllPaintOnSrf.cpp

libGuIritDllPaintOnSrf_la_LDFLAGS = -version-info 0:0:0
libGuIritDllPaintOnSrf_la_LIBADD = $(GUIRIT_EXT_DLL_LIB)
LIBS = $(IRIT_LIBS) $(GRAPDRVS) $(GUIRIT_LIBS)
EXTRA_DIST = Icons
