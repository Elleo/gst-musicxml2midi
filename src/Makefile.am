# plugindir is set in configure
musicxml2midi_LTLIBRARIES = libgstmusicxml2midi.la

libgstmusicxml2midi_la_SOURCES = gstmusicxml2midi.c

libgstmusicxml2midi_la_CFLAGS = $(GST_CFLAGS)
libgstmusicxml2midi_la_LIBADD = $(GST_LIBS)
libgstmusicxml2midi_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstmusicxml2midi_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstmusicxml2midi.h
