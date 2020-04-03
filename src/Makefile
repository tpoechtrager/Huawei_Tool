PLATFORM ?= $(shell uname -s)
RELEASE ?= 0
DEBUG ?= 0
OPTIMIZE ?= 3
LTO ?= 0
STD ?= c++0x

override COMPILER= $(shell echo $(CXX) | awk -F- '{print $NF}')
override CPP= $(shell echo $(CXX) | sed 's/$(COMPILER)/cpp)/')

override PLATFORM:= $(shell echo $(PLATFORM) | tr '[:upper:]' '[:lower:]')
override FLAGS+= -std=$(STD) -O$(OPTIMIZE) -I. -Wall -Wextra -Werror=format
override LDFLAGS+= -pthread

ifeq (1, $(DEBUG))
    override FLAGS+= -g3
endif

#ifneq (1, $(RELEASE))
    override FLAGS+= -DWORK_IN_PROGRESS
#endif

ifeq (, $(findstring clang, $(CXX)))
    override LTOFLAG= -flto=8
else
    override LTOFLAG= -flto
endif

ifeq (1, $(LTO))
    override FLAGS+= $(LTOFLAG)
endif

ifneq (, $(LIBS))
    ifeq (, $(findstring darwin, $(PLATFORM)))
        ifneq (1, $(DEBUG))
            override LDFLAGS+= -Wl,-s
        endif
        override LDFLAGS+= -static -static-libgcc -static-libstdc++
        override LDFLAGS+= -Wl,--gc-sections
       # override LDFLAGS+= -Wl,--as-needed
    else
        override LDFLAGS+= -Wl,-dead_strip
    endif
endif

ifneq (, $(LIBS))
    override FLAGS+= -isystem $(LIBS)/include
    override LDFLAGS+= -L $(LIBS)/lib
endif

ifneq (, $(findstring darwin, $(PLATFORM)))
    override FLAGS+= -DUSE_GETTIMEOFDAY
endif

ifneq (, $(findstring linux, $(PLATFORM)))
    override EXTRA_LIBS+= -lrt
endif

ifneq (, $(findstring android, $(PLATFORM)))
    override FLAGS+=-fPIE
    override LDFLAGS+=-pie
    override EXTRA_LIBS+=-ldl -llog
endif

ifneq (, $(findstring mingw, $(PLATFORM)))
    override FLAGS+= -DCURL_STATICLIB -D__USE_MINGW_ANSI_STDIO
    override EXTRA_LIBS+= -lws2_32 -liphlpapi -lshlwapi
    override EXE_SUFFIX= .exe
endif

BIN=huawei_band_tool$(EXE_SUFFIX)

SRCS=at_tcp.cpp huawei_tools.cpp main.cpp tools.cpp web.cpp cli_tools.cpp

OBJS=$(subst .cpp,.o,$(SRCS))
OBJS:=$(subst .c,.o,$(OBJS))

override LDFLAGS+= -lcryptopp -lconfig4cpp -lcurl -lz -lSDL2_net -lSDL2 $(EXTRA_LIBS)

all: project

%.o: %.cpp
	$(CXX) $(FLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(FLAGS) -c -o $@ $<

project: $(OBJS)
	$(CXX) $(FLAGS) -o $(BIN) $(OBJS) $(LDFLAGS)

install: project
	cp -f $(BIN) ..

.PHONY: clean

clean:
	rm -f $(BIN){,.exe} $(OBJS)

