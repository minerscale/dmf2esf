#------------------------------------------------------------------------------#
# This makefile was generated by 'cbp2make' tool rev.147                       #
#------------------------------------------------------------------------------#


WORKDIR = `pwd`

CC = gcc
CXX = g++
AR = ar
LD = g++
WINDRES = windres

INC = 
CFLAGS = -Wall -Wno-unused-variable -Wno-sign-compare -Wno-unused-value -Wno-misleading-indentation -Wno-unused-but-set-variable -Wno-int-in-bool-context -Wno-maybe-uninitialized -fexceptions
RESINC = 
LIBDIR = 
LIB = 
LDFLAGS = 

INC_DEBUG = $(INC)
CFLAGS_DEBUG = $(CFLAGS) -g -DDEBUG
RESINC_DEBUG = $(RESINC)
RCFLAGS_DEBUG = $(RCFLAGS)
LIBDIR_DEBUG = $(LIBDIR)
LIB_DEBUG = $(LIB)
LDFLAGS_DEBUG = $(LDFLAGS)
OBJDIR_DEBUG = obj/Debug
DEP_DEBUG = 
OUT_DEBUG = bin/Debug/dmf2esf

INC_RELEASE = $(INC)
CFLAGS_RELEASE = $(CFLAGS) -O2
RESINC_RELEASE = $(RESINC)
RCFLAGS_RELEASE = $(RCFLAGS)
LIBDIR_RELEASE = $(LIBDIR)
LIB_RELEASE = $(LIB)
LDFLAGS_RELEASE = $(LDFLAGS) -s
OBJDIR_RELEASE = obj/Release
DEP_RELEASE = 
OUT_RELEASE = bin/Release/dmf2esf

OBJ_DEBUG = $(OBJDIR_DEBUG)/miniz.o $(OBJDIR_DEBUG)/main.o $(OBJDIR_DEBUG)/libsamplerate/src/src_zoh.o $(OBJDIR_DEBUG)/libsamplerate/src/src_sinc.o $(OBJDIR_DEBUG)/libsamplerate/src/src_linear.o $(OBJDIR_DEBUG)/libsamplerate/src/samplerate.o $(OBJDIR_DEBUG)/DMFConverter.o $(OBJDIR_DEBUG)/inireader.o $(OBJDIR_DEBUG)/ini.o $(OBJDIR_DEBUG)/functions.o $(OBJDIR_DEBUG)/ESFOutput.o

OBJ_RELEASE = $(OBJDIR_RELEASE)/miniz.o $(OBJDIR_RELEASE)/main.o $(OBJDIR_RELEASE)/libsamplerate/src/src_zoh.o $(OBJDIR_RELEASE)/libsamplerate/src/src_sinc.o $(OBJDIR_RELEASE)/libsamplerate/src/src_linear.o $(OBJDIR_RELEASE)/libsamplerate/src/samplerate.o $(OBJDIR_RELEASE)/DMFConverter.o $(OBJDIR_RELEASE)/inireader.o $(OBJDIR_RELEASE)/ini.o $(OBJDIR_RELEASE)/functions.o $(OBJDIR_RELEASE)/ESFOutput.o

all: debug release

clean: clean_debug clean_release

before_debug: 
	test -d bin/Debug || mkdir -p bin/Debug
	test -d $(OBJDIR_DEBUG) || mkdir -p $(OBJDIR_DEBUG)
	test -d $(OBJDIR_DEBUG)/libsamplerate/src || mkdir -p $(OBJDIR_DEBUG)/libsamplerate/src

after_debug: 

debug: before_debug out_debug after_debug

out_debug: before_debug $(OBJ_DEBUG) $(DEP_DEBUG)
	$(LD) $(LIBDIR_DEBUG) -o $(OUT_DEBUG) $(OBJ_DEBUG)  $(LDFLAGS_DEBUG) $(LIB_DEBUG)

$(OBJDIR_DEBUG)/miniz.o: miniz.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c miniz.c -o $(OBJDIR_DEBUG)/miniz.o

$(OBJDIR_DEBUG)/main.o: main.cpp
	$(CXX) $(CFLAGS_DEBUG) $(INC_DEBUG) -c main.cpp -o $(OBJDIR_DEBUG)/main.o

$(OBJDIR_DEBUG)/libsamplerate/src/src_zoh.o: libsamplerate/src/src_zoh.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c libsamplerate/src/src_zoh.c -o $(OBJDIR_DEBUG)/libsamplerate/src/src_zoh.o

$(OBJDIR_DEBUG)/libsamplerate/src/src_sinc.o: libsamplerate/src/src_sinc.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c libsamplerate/src/src_sinc.c -o $(OBJDIR_DEBUG)/libsamplerate/src/src_sinc.o

$(OBJDIR_DEBUG)/libsamplerate/src/src_linear.o: libsamplerate/src/src_linear.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c libsamplerate/src/src_linear.c -o $(OBJDIR_DEBUG)/libsamplerate/src/src_linear.o

$(OBJDIR_DEBUG)/libsamplerate/src/samplerate.o: libsamplerate/src/samplerate.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c libsamplerate/src/samplerate.c -o $(OBJDIR_DEBUG)/libsamplerate/src/samplerate.o

$(OBJDIR_DEBUG)/DMFConverter.o: DMFConverter.cpp
	$(CXX) $(CFLAGS_DEBUG) $(INC_DEBUG) -c DMFConverter.cpp -o $(OBJDIR_DEBUG)/DMFConverter.o

$(OBJDIR_DEBUG)/inireader.o: inireader.cpp
	$(CXX) $(CFLAGS_DEBUG) $(INC_DEBUG) -c inireader.cpp -o $(OBJDIR_DEBUG)/inireader.o

$(OBJDIR_DEBUG)/ini.o: ini.c
	$(CC) $(CFLAGS_DEBUG) $(INC_DEBUG) -c ini.c -o $(OBJDIR_DEBUG)/ini.o

$(OBJDIR_DEBUG)/functions.o: functions.cpp
	$(CXX) $(CFLAGS_DEBUG) $(INC_DEBUG) -c functions.cpp -o $(OBJDIR_DEBUG)/functions.o

$(OBJDIR_DEBUG)/ESFOutput.o: ESFOutput.cpp
	$(CXX) $(CFLAGS_DEBUG) $(INC_DEBUG) -c ESFOutput.cpp -o $(OBJDIR_DEBUG)/ESFOutput.o

clean_debug: 
	rm -f $(OBJ_DEBUG) $(OUT_DEBUG)
	rm -rf bin/Debug
	rm -rf $(OBJDIR_DEBUG)
	rm -rf $(OBJDIR_DEBUG)/libsamplerate/src

before_release: 
	test -d bin/Release || mkdir -p bin/Release
	test -d $(OBJDIR_RELEASE) || mkdir -p $(OBJDIR_RELEASE)
	test -d $(OBJDIR_RELEASE)/libsamplerate/src || mkdir -p $(OBJDIR_RELEASE)/libsamplerate/src

after_release: 

release: before_release out_release after_release

out_release: before_release $(OBJ_RELEASE) $(DEP_RELEASE)
	$(LD) $(LIBDIR_RELEASE) -o $(OUT_RELEASE) $(OBJ_RELEASE)  $(LDFLAGS_RELEASE) $(LIB_RELEASE)

$(OBJDIR_RELEASE)/miniz.o: miniz.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c miniz.c -o $(OBJDIR_RELEASE)/miniz.o

$(OBJDIR_RELEASE)/main.o: main.cpp
	$(CXX) $(CFLAGS_RELEASE) $(INC_RELEASE) -c main.cpp -o $(OBJDIR_RELEASE)/main.o

$(OBJDIR_RELEASE)/libsamplerate/src/src_zoh.o: libsamplerate/src/src_zoh.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c libsamplerate/src/src_zoh.c -o $(OBJDIR_RELEASE)/libsamplerate/src/src_zoh.o

$(OBJDIR_RELEASE)/libsamplerate/src/src_sinc.o: libsamplerate/src/src_sinc.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c libsamplerate/src/src_sinc.c -o $(OBJDIR_RELEASE)/libsamplerate/src/src_sinc.o

$(OBJDIR_RELEASE)/libsamplerate/src/src_linear.o: libsamplerate/src/src_linear.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c libsamplerate/src/src_linear.c -o $(OBJDIR_RELEASE)/libsamplerate/src/src_linear.o

$(OBJDIR_RELEASE)/libsamplerate/src/samplerate.o: libsamplerate/src/samplerate.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c libsamplerate/src/samplerate.c -o $(OBJDIR_RELEASE)/libsamplerate/src/samplerate.o

$(OBJDIR_RELEASE)/DMFConverter.o: DMFConverter.cpp
	$(CXX) $(CFLAGS_RELEASE) $(INC_RELEASE) -c DMFConverter.cpp -o $(OBJDIR_RELEASE)/DMFConverter.o

$(OBJDIR_RELEASE)/inireader.o: inireader.cpp
	$(CXX) $(CFLAGS_RELEASE) $(INC_RELEASE) -c inireader.cpp -o $(OBJDIR_RELEASE)/inireader.o

$(OBJDIR_RELEASE)/ini.o: ini.c
	$(CC) $(CFLAGS_RELEASE) $(INC_RELEASE) -c ini.c -o $(OBJDIR_RELEASE)/ini.o

$(OBJDIR_RELEASE)/functions.o: functions.cpp
	$(CXX) $(CFLAGS_RELEASE) $(INC_RELEASE) -c functions.cpp -o $(OBJDIR_RELEASE)/functions.o

$(OBJDIR_RELEASE)/ESFOutput.o: ESFOutput.cpp
	$(CXX) $(CFLAGS_RELEASE) $(INC_RELEASE) -c ESFOutput.cpp -o $(OBJDIR_RELEASE)/ESFOutput.o

clean_release: 
	rm -f $(OBJ_RELEASE) $(OUT_RELEASE)
	rm -rf bin/Release
	rm -rf $(OBJDIR_RELEASE)
	rm -rf $(OBJDIR_RELEASE)/libsamplerate/src

.PHONY: before_debug after_debug clean_debug before_release after_release clean_release

