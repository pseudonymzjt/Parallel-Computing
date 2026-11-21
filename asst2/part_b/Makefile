ARCH := $(shell uname -m)

ifeq ($(ARCH), aarch64)
    CXX = g++
else
    CXX = g++ -m64
endif

CXXFLAGS=-I. -I../common -I../tests -Iobjs/ -O3 -std=c++11 -Wall

# AddressSanitizer debug build: `make asan` -> ./runtasks_asan
ASAN_APP_NAME=runtasks_asan
ASAN_CXXFLAGS=-I. -I../common -I../tests -g -O0 -std=c++11 -Wall -fsanitize=address -fno-omit-frame-pointer

APP_NAME=runtasks
OBJDIR=objs
COMMONDIR=../common

PPM_CXX=$(COMMONDIR)/ppm.cpp
PPM_OBJ=$(addprefix $(OBJDIR)/, $(subst $(COMMONDIR)/,, $(PPM_CXX:.cpp=.o)))

default: $(APP_NAME)

.PHONY: dirs clean

dirs:
	/bin/mkdir -p $(OBJDIR)/

clean:
	/bin/rm -rf $(OBJDIR) *.ppm *~ $(APP_NAME) $(ASAN_APP_NAME)

OBJS=$(PPM_OBJ) $(OBJDIR)/tasksys.o

.PHONY: asan
asan:
	$(CXX) $(ASAN_CXXFLAGS) ../tests/main.cpp tasksys.cpp -o $(ASAN_APP_NAME) -lm -lpthread

$(APP_NAME): clean dirs $(OBJS)
	$(CXX) ../tests/main.cpp $(CXXFLAGS) -o $@ $(OBJDIR)/tasksys.o -lm -lpthread

$(OBJDIR)/%.o: $(COMMONDIR)/%.cpp
	$(CXX) $< $(CXXFLAGS) -c -o $@

$(OBJDIR)/%.o: %.cpp
	$(CXX) $< $(CXXFLAGS) -c -o $@
