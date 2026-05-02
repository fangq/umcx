ROOTDIR ?= ..
MCXDIR  ?= $(ROOTDIR)
OUTPUT_DIR=$(MCXDIR)/bin
DOC_DIR=$(MCXDIR)/doc
MKDIR = mkdir
DOXYGEN = doxygen

CXXFLAGS += -g -Wall -Wextra -pedantic -std=c++11 -O3
BINARY = umcx
SRC = umcx.cpp
DOCCFG = umcxdoc.cfg
OBJS = $(SRC:.cpp=.o)

OMPACC=-fopenmp

ifeq ($(OS),Windows_NT)
    ifneq ($(findstring g++,$(CXX)),)
        USERLINKFLAGS += -static-libgcc -static-libstdc++
    endif
endif

ifneq (,$(filter $(MAKECMDGOALS),nvc))
    OMPACC=-mp=gpu
endif

ifneq (,$(filter $(ACC),on))
    ifneq (,$(filter $(MAKECMDGOALS),nvc))
        OMPACC=-acc=gpu
    else
        OMPACC=-fopenacc
    endif
endif

GFX      ?= gfx906
SM       ?= sm_50
AMDCXX   ?= /opt/rocm/llvm/bin/clang++
CC_ARCH  ?= cc70,cc80,cc86,cc90,ptx

nvc: CXX=nvc++
nvc: CXXFLAGS+=-Minfo=mp,accel -Minline -gpu=mem:managed,ptxinfo,ccall,$(CC_ARCH) -static-nvidia
amdclang: CXX=$(AMDCXX)
amdclang: CXXFLAGS+=-target x86_64-pc-linux-gnu -fopenmp -fopenmp-targets=amdgcn-amd-amdhsa -Xopenmp-target=amdgcn-amd-amdhsa -march=$(GFX) -fcf-protection=none -fno-stack-protector
amd: CXXFLAGS+=-foffload=amdgcn-amdhsa="-march=$(GFX)" -foffload="-lm" -fcf-protection=none -fno-stack-protector
nvidiaclang: CXX=clang++
nvidiaclang: CXXFLAGS+=-fopenmp-targets=nvptx64 --cuda-gpu-arch=$(SM)
nvidia: CXXFLAGS+=-foffload=nvptx-none -foffload="-lm" -fcf-protection=none -fno-stack-protector
nvidiaclang nvidia amd amdclang nvc: CXXFLAGS+=-DGPU_OFFLOAD
nvidiaclang nvidia amd amdclang nvc: omp

omp: CXXFLAGS+=$(OMPACC)
debugmulti multi: omp
debugmulti multi: CXXFLAGS+=-foffload=disable
debugsingle debugmulti: CXXFLAGS+=-DDEBUG
debugsingle debugmulti multi single omp: $(OUTPUT_DIR)/$(BINARY)

all: multi

makedirs:
	@if test ! -d $(OUTPUT_DIR); then $(MKDIR) $(OUTPUT_DIR); fi

$(OUTPUT_DIR)/$(BINARY): makedirs $(OBJS)
	$(CXX)  $(CXXFLAGS) -o $(OUTPUT_DIR)/$(BINARY) $(OBJS) $(USERLINKFLAGS)

%.o: %.c
	$(CC)  $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX)  $(CXXFLAGS) $(USERCXXFLAGS) -c $< -o $@

makedocdir:
	@if test ! -d $(DOC_DIR); then $(MKDIR) $(DOC_DIR); fi

doc: makedocdir
	$(DOXYGEN) $(DOCCFG)

clean:
	rm -rf $(OUTPUT_DIR)/$(BINARY) *.o $(DOC_DIR)/*

# derived the astyle settings from https://github.com/nlohmann/json
pretty:
	astyle \
	    --style=attach \
	    --indent=spaces=4 \
	    --indent-modifiers \
	    --indent-switches \
	    --indent-preproc-block \
	    --indent-preproc-define \
	    --indent-col1-comments \
	    --pad-oper \
	    --pad-header \
	    --align-pointer=type \
	    --align-reference=type \
	    --add-brackets \
	    --convert-tabs \
	    --close-templates \
	    --lineend=linux \
	    --preserve-date \
	    --suffix=none \
	    --formatted \
	    --break-blocks \
	   "*.cpp"

.DEFAULT_GOAL=all
