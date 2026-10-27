##############################################################################
# uMCX tests: unit tests, fuzzing and code coverage
#
#   make unit        build and run the unit tests (unit/umcx_unit.cpp)
#   make unitasan    unit tests with AddressSanitizer + UndefinedBehaviorSanitizer
#   make shell       run the command line tests (testumcx.sh) on ../bin/umcx
#   make fuzz        build the libFuzzer target (needs clang)
#   make fuzzrun     fuzz for FUZZTIME seconds, seeded from fuzz/corpus
#   make fuzzreplay  replay fuzz/corpus with any compiler (no libFuzzer needed)
#   make coverage    line coverage of src/umcx.cpp from all of the above (gcc + gcovr or lcov)
#   make clean
#
# The test programs #include ../src/umcx.cpp with -Dmain=umcx_main, so every
# internal class is testable without changing the source.
##############################################################################

ROOT     := $(abspath ..)
SRC      := $(ROOT)/src
BUILD    ?= $(ROOT)/test/build
CXX      ?= g++
FUZZCXX  ?= clang++
FUZZTIME ?= 60
JOBS     ?= 1# >1: parallel fuzzing jobs, each logging to fuzz-<n>.log

CXXFLAGS ?= -g -O2
COMMON    = -std=c++11 -Wall -Wextra -Dmain=umcx_main -I$(SRC)
OMPFLAGS ?= -fopenmp $(if $(findstring clang,$(CXX)),,-foffload=disable)
SANFLAGS  = -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined
# float-cast-overflow: nlohmann::json get<int>() of a huge number is out of scope here
FUZZFLAGS = -g -O1 -Wno-unknown-pragmas -fsanitize=fuzzer,address,undefined -fno-sanitize=float-cast-overflow -fno-sanitize-recover=undefined
# zmat sizes a gzip buffer from the stream's 32-bit length field: allow up to 4 GB
FUZZOPTS  = -timeout=10 -rss_limit_mb=6144 -malloc_limit_mb=4400 -dict=$(ROOT)/test/fuzz/umcx.dict
COVFLAGS  = -g -O0 --coverage -fprofile-update=atomic

UNITSRC  = $(ROOT)/test/unit/umcx_unit.cpp
FUZZSRC  = $(ROOT)/test/fuzz/umcx_fuzz.cpp
DEPS     = $(SRC)/umcx.cpp

export ASAN_OPTIONS ?= detect_leaks=0

.PHONY: all unit unitasan shell fuzz fuzzrun fuzzreplay coverage clean

all: unit

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/umcx_unit: $(UNITSRC) $(DEPS) | $(BUILD)
	cd $(BUILD) && $(CXX) $(COMMON) $(CXXFLAGS) $(OMPFLAGS) -o $@ $(UNITSRC)

$(BUILD)/umcx_unit_asan: $(UNITSRC) $(DEPS) | $(BUILD)
	cd $(BUILD) && $(CXX) $(COMMON) -g -O1 $(SANFLAGS) $(OMPFLAGS) -o $@ $(UNITSRC)

$(BUILD)/umcx_fuzz: $(FUZZSRC) $(DEPS) | $(BUILD)
	$(FUZZCXX) $(COMMON) $(FUZZFLAGS) -o $@ $(FUZZSRC)

$(BUILD)/umcx_fuzzreplay: $(FUZZSRC) $(DEPS) | $(BUILD)
	cd $(BUILD) && $(CXX) $(COMMON) $(CXXFLAGS) -Wno-unknown-pragmas -DUMCX_FUZZ_STANDALONE -o $@ $(FUZZSRC)

unit: $(BUILD)/umcx_unit
	cd $(BUILD) && ./umcx_unit

unitasan: $(BUILD)/umcx_unit_asan
	cd $(BUILD) && ./umcx_unit_asan

shell:
	./testumcx.sh

fuzz: $(BUILD)/umcx_fuzz

# the working corpus lives in $(BUILD), so fuzz/corpus only holds the curated seeds
fuzzrun: $(BUILD)/umcx_fuzz
	mkdir -p $(BUILD)/fuzzcorpus $(BUILD)/fuzzcrash
	cp -n fuzz/corpus/* $(BUILD)/fuzzcorpus/ 2>/dev/null || true
	$(BUILD)/umcx_fuzz $(FUZZOPTS) -max_total_time=$(FUZZTIME) $(if $(filter-out 1,$(JOBS)),-jobs=$(JOBS) -workers=$(JOBS)) \
	    -artifact_prefix=$(BUILD)/fuzzcrash/ $(BUILD)/fuzzcorpus

fuzzreplay: $(BUILD)/umcx_fuzzreplay
	$(BUILD)/umcx_fuzzreplay $(ROOT)/test/fuzz/corpus/*

# gcov counters from the unit tests, the fuzz corpus replay and short command line runs
# (-O0 binaries are slow: the full testumcx.sh is left to "make shell" on an optimized build)
coverage: | $(BUILD)
	mkdir -p $(BUILD)/cov && find $(BUILD)/cov -name '*.gcda' -delete
	OMP_NUM_THREADS=2 $(MAKE) unit fuzzreplay CXXFLAGS="$(COVFLAGS)" BUILD=$(BUILD)/cov
	cd $(BUILD)/cov && $(CXX) -std=c++11 $(COVFLAGS) $(OMPFLAGS) -I$(SRC) -o $(BUILD)/cov/umcx $(SRC)/umcx.cpp
	cd $(BUILD)/cov && export OMP_NUM_THREADS=2 && for b in cube60 cube60b cube60planar cubesph60b sphshells spherebox; do \
	    ./umcx --bench $$b -n 1e4 -s cov_$$b > /dev/null || exit 1; done; \
	    ./umcx --bench cube60 --dumpjson > /dev/null; ./umcx --bench cube60 --dumpmask > /dev/null; ./umcx > /dev/null; \
	    ./umcx --bench cube60 -n 1e3 -O e -w 53 --json '{"Optode":{"Source":{"Type":"isotropic","Pos":[30,30,30]}}}' > /dev/null; \
	    ./umcx cube60 -n 10 > /dev/null 2>&1; ./umcx --bench nosuch > /dev/null; rm -f *.bnii *.jdb; true
	@if command -v gcovr > /dev/null; then \
	    gcovr --root $(ROOT) --filter '$(SRC)/umcx.cpp' --txt --html-details $(BUILD)/coverage.html \
	        --xml $(BUILD)/coverage.xml --print-summary $(BUILD)/cov; \
	else \
	    lcov --quiet --capture --directory $(BUILD)/cov --output-file $(BUILD)/coverage.all.info && \
	    lcov --quiet --extract $(BUILD)/coverage.all.info '$(SRC)/umcx.cpp' --output-file $(BUILD)/coverage.info && \
	    genhtml --quiet $(BUILD)/coverage.info --output-directory $(BUILD)/coverage && \
	    lcov --summary $(BUILD)/coverage.info; \
	fi

clean:
	rm -rf $(BUILD)
