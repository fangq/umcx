//////////////////////////////////////////////////////////////////////////////////////////////////
/// \file umcx_fuzz.cpp
/// \brief libFuzzer target for the uMCX input parser and simulation core
///
/// The fuzzer input is a JSON (text) or BJData (binary) simulation description, the same formats
/// umcx reads with -f and the MATLAB mex file receives. It is forwarded to MCX_run_simulation()
/// through the BJData buffer entry point, so the real input handling, domain rasterization,
/// detector masking and photon transport are all exercised.
///
/// Only the parameters that make a run expensive, rather than invalid, are clamped (domain size,
/// photon count, time gates, scattering coefficient, detector radius): without that the fuzzer
/// spends its time on minute-long simulations instead of finding memory errors.
///
/// build:  make -C test fuzz        (clang, -fsanitize=fuzzer,address,undefined)
/// run:    make -C test fuzzrun     (FUZZTIME=seconds, default 60)
/// replay: make -C test fuzzreplay  (any compiler, replays the seed corpus; used for coverage)
//////////////////////////////////////////////////////////////////////////////////////////////////
#include "../../src/umcx.cpp"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#undef main

static const int MAX_DIM = 24, MAX_PHOTON = 16, MAX_GATE = 8, MAX_LIST = 16;

/// clamp a number at path p of obj into [lo, hi]; anything that is not a number is left as is
static void clampnum(json& obj, const std::string& p, double lo, double hi) {
    json::json_pointer ptr(p);

    if (obj.is_object() && obj.contains(ptr) && obj[ptr].is_number()) {
        double v = obj[ptr].get<double>();
        obj[ptr] = std::isnan(v) ? lo : std::min(hi, std::max(lo, v));
    }
}
/// truncate an array at path p of obj to n elements
static void truncate(json& obj, const std::string& p, size_t n) {
    json::json_pointer ptr(p);

    if (obj.is_object() && obj.contains(ptr) && obj[ptr].is_array() && obj[ptr].size() > n) {
        obj[ptr].erase(obj[ptr].begin() + n, obj[ptr].end());
    }
}
/// clamp every number of the arrays at p into [lo, hi]
static void clamparray(json& obj, const std::string& p, double lo, double hi) {
    json::json_pointer ptr(p);

    if (obj.is_object() && obj.contains(ptr) && obj[ptr].is_array())
        for (size_t i = 0; i < obj[ptr].size(); i++) {
            clampnum(obj, p + "/" + std::to_string(i), lo, hi);
        }
}
/// returns false if the input describes a run that is too expensive to be worth fuzzing
static bool sanitize(json& cfg) {
    if (!cfg.is_object()) {
        return true;    // let umcx reject it
    }

    cfg["Session"]["ID"] = "umcxfuzz";                    // output files go to the scratch folder
    cfg["Session"]["RNGSeed"] = 1;                        // seed <= 0 means time(0): not reproducible
    clampnum(cfg, "/Session/Photons", 0, MAX_PHOTON);
    clampnum(cfg, "/Session/MaxDetPhoton", 0, 1000);
    clamparray(cfg, "/Domain/Dim", 0, MAX_DIM);
    clampnum(cfg, "/Domain/LengthUnit", 0.1, 2.0);
    truncate(cfg, "/Domain/Media", MAX_LIST);
    truncate(cfg, "/Optode/Detector", MAX_LIST);
    truncate(cfg, "/Shapes", MAX_LIST);

    // at most MAX_GATE time gates of at most 1 ns in total (~300 mm of travel)
    clampnum(cfg, "/Forward/T0", 0.0, 1e-9);
    clampnum(cfg, "/Forward/T1", 0.0, 1e-9);
    clampnum(cfg, "/Forward/Dt", 1e-9 / MAX_GATE, 1e-9);

    if (cfg.contains("Domain") && cfg["Domain"].contains("Media") && cfg["Domain"]["Media"].is_array()) {
        for (size_t i = 0; i < cfg["Domain"]["Media"].size(); i++) {
            std::string m = "/Domain/Media/" + std::to_string(i);
            bool isarray = cfg["Domain"]["Media"][i].is_array();
            clampnum(cfg, m + (isarray ? "/0" : "/mua"), 0.0, 10.0);
            clampnum(cfg, m + (isarray ? "/1" : "/mus"), 0.0, 10.0);  // bounds the scattering events per photon
            clampnum(cfg, m + (isarray ? "/2" : "/g"), -1.0, 1.0);
            clampnum(cfg, m + (isarray ? "/3" : "/n"), 1.0, 3.0);     // n > 0 keeps time-of-flight advancing
        }
    }

    if (cfg.contains("Optode") && cfg["Optode"].contains("Detector") && cfg["Optode"]["Detector"].is_array()) {
        for (size_t i = 0; i < cfg["Optode"]["Detector"].size(); i++) {
            clampnum(cfg, "/Optode/Detector/" + std::to_string(i) + "/R", 0.0, 4.0);
            clamparray(cfg, "/Optode/Detector/" + std::to_string(i) + "/Pos", -MAX_DIM, 2 * MAX_DIM);
        }
    }

    if (cfg.contains("Shapes") && cfg["Shapes"].is_array()) {
        for (size_t i = 0; i < cfg["Shapes"].size(); i++) {
            clamparray(cfg, "/Shapes/" + std::to_string(i) + "/Grid/Size", 0, MAX_DIM);
        }
    } else if (cfg.contains("Shapes") && cfg["Shapes"].is_object() && cfg["Shapes"].contains("_ArraySize_")) {
        const json& sz = cfg["Shapes"]["_ArraySize_"];

        for (size_t i = 0; i < sz.size(); i++)
            if (!sz[i].is_number() || sz[i].get<double>() < 0 || sz[i].get<double>() > MAX_DIM) {
                return false;    // a JData volume can not be shrunk without invalidating its payload
            }
    }

    return true;
}

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv) {
    (void)argc, (void)argv;
    char tmpl[] = "/tmp/umcxfuzz.XXXXXX";

    if (mkdtemp(tmpl) == nullptr || chdir(tmpl) != 0) {
        perror("umcx_fuzz: can not create a scratch folder");
        std::abort();
    }

    if (std::getenv("UMCX_FUZZ_VERBOSE") == nullptr) {
        (void)!freopen("/dev/null", "w", stdout);    // umcx prints a summary line for every run
    }

    return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    json cfg = json::parse(data, data + size, nullptr, false);

    if (cfg.is_discarded()) {
        try {
            cfg = json::from_bjdata(std::vector<uint8_t>(data, data + size));
        } catch (const std::exception&) {
            return 0;
        }
    }

    try {
        if (!sanitize(cfg)) {
            return 0;
        }

        std::vector<uint8_t> buf = json::to_bjdata(cfg);
        char* argv[] = {(char*)buf.data()};
        MCX_run_simulation(argv, -(int)buf.size());
    } catch (const std::exception&) {
        // rejecting invalid input with an exception is the expected behavior
    }

    return 0;
}

#ifdef UMCX_FUZZ_STANDALONE
/// replay driver for compilers without libFuzzer: runs each file given on the command line once
int main(int argc, char* argv[]) {
    std::vector<std::string> files(argv + 1, argv + argc);
    LLVMFuzzerInitialize(&argc, &argv);

    for (auto& f : files) {
        std::ifstream in(f, std::ios::binary);
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        fprintf(stderr, "replaying %s (%zu bytes)\n", f.c_str(), data.size());
        LLVMFuzzerTestOneInput(data.data(), data.size());
    }

    fprintf(stderr, "replayed %zu input(s)\n", files.size());
    return 0;
}
#endif
