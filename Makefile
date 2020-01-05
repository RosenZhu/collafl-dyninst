#
# collafl - makefile
# -----------------------------
#
# Written by Xiaogang Zhu <xiaogangzhu@swin.edu.au>
#
# Based on AFL (american fuzzy lop) by Michal Zalewski <lcamtuf@google.com>
# 
# ------------Original copyright below------------
# 
# Copyright 2013, 2014, 2015, 2016, 2017 Google Inc. All rights reserved.
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
# 
#   http://www.apache.org/licenses/LICENSE-2.0
#

##################################################################

# var- edit DYN_ROOT accordingly

DYN_ROOT 	= /home/xgzhu/apps/buildDyn
# These should point to where libelf and libdwarf are installed
LOCAL_INC = /usr/local/include
LOCAL_LIBS = /usr/local/lib
# TBB_INC = $(DYN_ROOT)/tbb/include
DYNINST_INCLUDE = $(DYN_ROOT)/include
DYNINST_LIB =  $(DYN_ROOT)/lib

CC 			= gcc 
CXX 		= g++
CXXFLAGS 	= -g -Wall -O3 -std=c++11
LIBFLAGS 	= -fpic -shared
LDFLAGS 	= -I/usr/include -I$(DYNINST_INCLUDE) -I$(LOCAL_INC) -L$(DYNINST_LIB) -L$(LOCAL_LIBS)\
					-lcommon -liberty -ldyninstAPI -lboost_system -linstructionAPI -lstdc++fs
# -I$(TBB_INC)



##################################################################

PROGNAME    = afl
VERSION     = $(shell grep '^\#define VERSION ' config.h | cut -d '"' -f2)

PREFIX     ?= /usr/local
BIN_PATH    = $(PREFIX)/bin
HELPER_PATH = $(PREFIX)/lib/afl
DOC_PATH    = $(PREFIX)/share/doc/afl
MISC_PATH   = $(PREFIX)/share/afl

# PROGS intentionally omit as, which gets installed elsewhere.
# afl-showmap 

PROGS       = collafl64 libCollAFLDyninst64 CollAFLDyninst64 collafl128 libCollAFLDyninst128 CollAFLDyninst128 collafl256 libCollAFLDyninst256 CollAFLDyninst256
SH_PROGS    = afl-plot

CFLAGS     ?= -O3 -funroll-loops
CFLAGS     += -Wall -D_FORTIFY_SOURCE=2 -g -Wno-pointer-sign \
	      -DAFL_PATH=\"$(HELPER_PATH)\" -DDOC_PATH=\"$(DOC_PATH)\" \
	      -DBIN_PATH=\"$(BIN_PATH)\"

ifneq "$(filter Linux GNU%,$(shell uname))" ""
  LDFLAGS  += -ldl
endif


COMM_HDR    = alloc-inl.h config.h debug.h types.h instConfig.h

all: test_x86 $(PROGS) all_done

ifndef AFL_NO_X86

test_x86:
	@echo "[*] Checking for the ability to compile x86 code..."
	@echo 'main() { __asm__("xorb %al, %al"); }' | $(CC) -w -x c - -o .test || ( echo; echo "Oops, looks like your compiler can't generate x86 code."; echo; echo "Don't panic! You can use the LLVM or QEMU mode, but see docs/INSTALL first."; echo "(To ignore this error, set AFL_NO_X86=1 and try again.)"; echo; exit 1 )
	@rm -f .test
	@echo "[+] Everything seems to be working, ready to compile."

else

test_x86:
	@echo "[!] Note: skipping x86 compilation checks (AFL_NO_X86 set)."

endif

# CollAFL dependencies

collafl64: collafl64.c $(COMM_HDR) | test_x86
	$(CC) $(CFLAGS) $@.c -o $@ $(LDFLAGS)

libCollAFLDyninst64: libCollAFLDyninst64.cpp
	$(CXX) $(CXXFLAGS) -o libCollAFLDyninst64.so libCollAFLDyninst64.cpp $(LDFLAGS) $(LIBFLAGS)

CollAFLDyninst64: CollAFLDyninst64.cpp
	$(CXX) -Wl,-rpath-link,$(DYN_ROOT)/lib -Wl,-rpath-link,$(DYN_ROOT)/include $(CXXFLAGS) -o CollAFLDyninst64 CollAFLDyninst64.cpp $(LDFLAGS)

collafl128: collafl128.c $(COMM_HDR) | test_x86
	$(CC) $(CFLAGS) $@.c -o $@ $(LDFLAGS)

libCollAFLDyninst128: libCollAFLDyninst128.cpp
	$(CXX) $(CXXFLAGS) -o libCollAFLDyninst128.so libCollAFLDyninst128.cpp $(LDFLAGS) $(LIBFLAGS)

CollAFLDyninst128: CollAFLDyninst128.cpp
	$(CXX) -Wl,-rpath-link,$(DYN_ROOT)/lib -Wl,-rpath-link,$(DYN_ROOT)/include $(CXXFLAGS) -o CollAFLDyninst128 CollAFLDyninst128.cpp $(LDFLAGS)

collafl256: collafl256.c $(COMM_HDR) | test_x86
	$(CC) $(CFLAGS) $@.c -o $@ $(LDFLAGS)

libCollAFLDyninst256: libCollAFLDyninst256.cpp
	$(CXX) $(CXXFLAGS) -o libCollAFLDyninst256.so libCollAFLDyninst256.cpp $(LDFLAGS) $(LIBFLAGS)

CollAFLDyninst256: CollAFLDyninst256.cpp
	$(CXX) -Wl,-rpath-link,$(DYN_ROOT)/lib -Wl,-rpath-link,$(DYN_ROOT)/include $(CXXFLAGS) -o CollAFLDyninst256 CollAFLDyninst256.cpp $(LDFLAGS)


# afl-showmap: afl-showmap.c $(COMM_HDR) | test_x86
# 	$(CC) $(CFLAGS) $@.c -o $@ $(LDFLAGS)



all_done: 
	@echo "[+] All done! Be sure to review README - it's pretty short and useful."
	
.NOTPARALLEL: clean

clean:
	rm -f $(PROGS) *.o *~ a.out core core.[1-9][0-9]* *.stackdump test .test *.so


