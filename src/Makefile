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

ifeq ($(ACC),on)
    NVC_GPU_MODE = -acc=gpu
    OMPACC = -fopenacc
else
    NVC_GPU_MODE = -mp=gpu
endif

MEX_BIN        ?= mex
MEXOUTDIR       = $(MCXDIR)/umcxlab
MEXBASEFLAGS    = $$CXXFLAGS
MEXBASELDFLAGS  = $$LDFLAGS
MEXDIRECT       =
MATLAB_ROOT    ?= $(patsubst %/bin/mex,%,$(realpath $(shell which $(MEX_BIN) 2>/dev/null)))
MEXEXT         ?= mexa64
MEXLNK          = $(MEX_BIN) CXX='$(CXX)' CXXFLAGS='$(MEXBASEFLAGS) $(CXXFLAGS) $(USERCXXFLAGS)' LDFLAGS='$(MEXBASELDFLAGS) $(CXXFLAGS)' $(SRC) -outdir $(OUTPUT_DIR)
MEXDIRLNK       = $(CXX) $(CXXFLAGS) $(USERCXXFLAGS) -shared -fPIC -DMATLAB_MEX_FILE -I$(MATLAB_ROOT)/extern/include -o $(OUTPUT_DIR)/$(BINARY).$(MEXEXT) $(SRC)

ifeq ($(MEX),1)
    OUTPUT_DIR = $(MEXOUTDIR)
endif

GFX      ?= gfx906
SM       ?= sm_50
AMDCXX   ?= /opt/rocm/llvm/bin/clang++
CC_ARCH  ?= cc70,cc80,cc86,cc90,ptx

nvc: CXX=nvc++
nvc: CXXFLAGS+=$(NVC_GPU_MODE) -Minfo=mp,accel -Minline -gpu=mem:managed,$(CC_ARCH) -static-nvidia
nvc: MEXBASEFLAGS =
nvc: MEXBASELDFLAGS =
nvc: MEXDIRECT = 1
amdclang: CXX=$(AMDCXX)
amdclang: CXXFLAGS+=-target x86_64-pc-linux-gnu -fopenmp -fopenmp-targets=amdgcn-amd-amdhsa -Xopenmp-target=amdgcn-amd-amdhsa -march=$(GFX) -fcf-protection=none -fno-stack-protector
amd: CXXFLAGS+=-foffload=amdgcn-amdhsa="-march=$(GFX)" -foffload="-lm" -fcf-protection=none -fno-stack-protector -no-pie
nvidiaclang: CXX=clang++
nvidiaclang: CXXFLAGS+=-fopenmp-targets=nvptx64 --cuda-gpu-arch=$(SM) -Wno-openmp-mapping -Wno-unused-command-line-argument
nvidia: CXXFLAGS+=-foffload=nvptx-none -foffload="-lm" -fcf-protection=none -fno-stack-protector -no-pie
nvidiaclang nvidia amd amdclang nvc: CXXFLAGS+=-DGPU_OFFLOAD
nvidiaclang nvidia amd amdclang: omp
nvc: $(OUTPUT_DIR)/$(BINARY)

omp: CXXFLAGS+=$(OMPACC)
debugmulti multi: omp
debugmulti multi: CXXFLAGS+=-foffload=disable
debugsingle debugmulti: CXXFLAGS+=-DDEBUG
debugsingle debugmulti multi single omp: $(OUTPUT_DIR)/$(BINARY)

all: multi

makemexdir:
	@if test ! -d $(MEXOUTDIR); then $(MKDIR) $(MEXOUTDIR); fi

mex: OUTPUT_DIR = $(MEXOUTDIR)
mex: CXXFLAGS+=-fopenmp -foffload=disable
mex: makemexdir
	$(MEXLNK)

makedirs:
	@if test ! -d $(OUTPUT_DIR); then $(MKDIR) $(OUTPUT_DIR); fi

$(OUTPUT_DIR)/$(BINARY): makedirs $(if $(filter 1,$(MEX)),,$(OBJS))
ifeq ($(MEX),1)
	$(if $(MEXDIRECT),$(MEXDIRLNK),$(MEXLNK))
else
	$(CXX)  $(CXXFLAGS) -o $(OUTPUT_DIR)/$(BINARY) $(OBJS) $(USERLINKFLAGS)
endif

%.o: %.c
	$(CC)  $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX)  $(CXXFLAGS) $(USERCXXFLAGS) -c $< -o $@

makedocdir:
	@if test ! -d $(DOC_DIR); then $(MKDIR) $(DOC_DIR); fi

doc: makedocdir
	$(DOXYGEN) $(DOCCFG)

clean:
	rm -rf $(OUTPUT_DIR)/$(BINARY) *.o $(DOC_DIR)/* $(MEXOUTDIR)/$(BINARY).mex*

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
ifeq ($(MEX),1)
    .DEFAULT_GOAL=mex
endif
