PLATFORM ?= $(shell uname -s)
OPTIMIZE ?= 3
LTO ?= 0

override PLATFORM:= $(shell echo $(PLATFORM) | tr '[:upper:]' '[:lower:]')

override FLAGS+= -O$(OPTIMIZE) -I. -Wall -Wextra
override FLAGS+= -DWORK_IN_PROGRESS

ifneq (, $(findstring darwin, $(PLATFORM)))
	override FLAGS+= -DUSE_GETTIMEOFDAY
endif

ifneq (, $(findstring mingw, $(PLATFORM)))
	override FLAGS+= -DCURL_STATICLIB
	override EXTRA_LIBS+= -lws2_32 -lwldap32
	override EXE_SUFFIX= .exe
endif

BIN=../huawei_band_tool$(EXE_SUFFIX)

SRCS=at_tcp.cpp huawei_tools.cpp main.cpp tools.cpp web.cpp

OBJS=$(subst .cpp,.o,$(SRCS))
OBJS:=$(subst .c,.o,$(OBJS))

override LDFLAGS+= -lcryptopp -lconfig4cpp -lcurl -lSDL2_net $(EXTRA_LIBS)

all: project

%.o: %.cpp
	$(CXX) -std=c++0x $(FLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(FLAGS) -c -o $@ $<

project: $(OBJS)
	$(CXX) $(FLAGS) -o $(BIN) $(OBJS) $(LDFLAGS)

.PHONY: clean

clean:
	rm -f $(BIN) $(OBJS)

