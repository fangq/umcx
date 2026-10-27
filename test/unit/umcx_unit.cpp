//////////////////////////////////////////////////////////////////////////////////////////////////
/// \file umcx_unit.cpp
/// \brief uMCX unit tests: compiles ../../src/umcx.cpp directly (with -Dmain=umcx_main, see
///        ../Makefile) so that every internal class can be tested without changing the source
//////////////////////////////////////////////////////////////////////////////////////////////////
#include "../../src/umcx.cpp"
#include <cstdio>
#include <vector>
#include <functional>
#ifdef _OPENMP
    #include <omp.h>
#endif

#undef main

// ---------------------------------------------------------------- a minimal test harness
static std::vector<std::pair<const char*, std::function<void()>>>& registry() {
    static std::vector<std::pair<const char*, std::function<void()>>> tests;
    return tests;
}
static int g_failed = 0, g_checks = 0;
struct Register {
    Register(const char* name, std::function<void()> fn) {
        registry().push_back({name, fn});
    }
};
#define TEST(name) static void name(); static Register reg_##name(#name, name); static void name()
#define CHECK(cond) do { g_checks++; if (!(cond)) { g_failed++; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_NEAR(a, b, tol) do { g_checks++; double a_ = (a), b_ = (b); if (!(fabs(a_ - b_) <= (tol))) { g_failed++; printf("  FAIL %s:%d: %s = %.9g, expected %.9g (tol %g)\n", __FILE__, __LINE__, #a, a_, b_, (double)(tol)); } } while (0)
#define CHECK_THROWS(expr) do { g_checks++; bool thrown_ = false; try { expr; } catch (const std::exception&) { thrown_ = true; } if (!thrown_) { g_failed++; printf("  FAIL %s:%d: %s did not throw\n", __FILE__, __LINE__, #expr); } } while (0)

// ---------------------------------------------------------------- helpers
/// build an MCX_userio from command line style arguments, as umcx would see them
static std::unique_ptr<MCX_userio> makeio(std::vector<std::string> args) {
    args.insert(args.begin(), "umcx");
    std::vector<char*> argv;

    for (auto& a : args) {
        argv.push_back(&a[0]);
    }

    return std::unique_ptr<MCX_userio>(new MCX_userio(argv.data(), (int)argv.size()));
}
/// build an MCX_userio from the cube60 benchmark modified by a JSON patch
static std::unique_ptr<MCX_userio> cube60with(const std::string& patch) {
    return makeio({"--bench", "cube60", "--json", patch});
}
/// build an MCX_userio from a JSON volume, a minimal config is added around it
static std::unique_ptr<MCX_userio> withvolume(const std::string& shapes) {
    return makeio({"--json", R"({"Domain":{"Dim":[1,1,1],"Media":[[0,0,1,1]]},"Optode":{"Source":{"Pos":[0,0,0],"Dir":[0,0,1]}},"Session":{}})", "--json", "{\"Shapes\":" + shapes + "}"});
}
static int countlabel(MCX_volume<int>& vol, int label) {
    int count = 0;

    for (uint64_t i = 0; i < vol.dimxyz; i++) {
        count += ((vol.vol[i] & MED_MASK) == (uint32_t)label);
    }

    return count;
}
static MCX_param defaultparam() {
    MCX_param g{};
    g.tstart = 0.f;
    g.tend = 1e-8f;
    g.rtstep = 1e8f;
    g.unitinmm = 1.f;
    g.maxgate = 1;
    g.issavevol = 1;
    g.outputtype = otEnergy;
    g.srctype = stPencil;
    return g;
}
static std::string base64zip(const void* data, size_t len, int zipid) {
    size_t zlen = 0, blen = 0;
    unsigned char* zbuf = nullptr, *bbuf = nullptr;
    int ret = 0;
    zmat_encode(len, (unsigned char*)data, &zlen, &zbuf, zipid, &ret);
    zmat_encode(zlen, zbuf, &blen, &bbuf, zmBase64, &ret);
    std::string out((char*)bbuf, blen);
    zmat_free(&zbuf);
    zmat_free(&bbuf);
    out.erase(std::remove(out.begin(), out.end(), '\n'), out.end());
    return out;
}
static float norm3(const float4& v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

// ================================================================ MCX_rand
TEST(rand_is_deterministic_and_in_range) {
    MCX_rand r1(1, 2, 3, 4), r2(1, 2, 3, 4), r3(1, 2, 3, 5);
    bool same = true, differ = false, inrange = true;
    double sum = 0.0;
    const int n = 20000;

    for (int i = 0; i < n; i++) {
        float a = r1.rand01(), b = r2.rand01(), c = r3.rand01();
        same = same && (a == b);
        differ = differ || (a != c);
        inrange = inrange && (a >= 0.f && a < 1.f);
        sum += a;
    }

    CHECK(same);
    CHECK(differ);
    CHECK(inrange);
    CHECK_NEAR(sum / n, 0.5, 0.01);
}

TEST(rand_scattering_length_is_exponential) {
    MCX_rand r(7, 7, 7, 7);
    double sum = 0.0;
    bool positive = true;
    const int n = 20000;

    for (int i = 0; i < n; i++) {
        float s = r.next_scat_len();
        positive = positive && (s > 0.f) && std::isfinite(s);
        sum += s;
    }

    CHECK(positive);
    CHECK_NEAR(sum / n, 1.0, 0.03);
}

// ================================================================ MCX_volume
TEST(volume_layout_and_bounds) {
    MCX_volume<int> v(3, 4, 5, 2, 7);
    CHECK(v.dimyz == 20 && v.dimxyz == 60 && v.dimxyzt == 120);
    CHECK(v.get(0) == 7 && v.get(119) == 7);
    CHECK(v.index(0, 0, 0) == 0);
    CHECK(v.index(0, 0, 1) == 1);                // z is the fastest index
    CHECK(v.index(0, 1, 0) == 5);
    CHECK(v.index(1, 0, 0) == 20);
    CHECK(v.index(2, 3, 4, 1) == 119);
    CHECK(v.index(-1, 0, 0) == -1);
    CHECK(v.index(3, 0, 0) == -1);
    CHECK(v.index(0, 4, 0) == -1);
    CHECK(v.index(0, 0, 5) == -1);
    CHECK(v.index(0, 0, 0, 2) == -1);
}

TEST(volume_add_mask_scale_copy) {
    MCX_volume<float> v(2, 2, 2, 1, 1.f);
    v.add(2.f, 3);
    CHECK(v.get(3) == 3.f);
    v.mask(0.f, 3);                              // a zero mask keeps the value
    CHECK(v.get(3) == 3.f);
    v.mask(5.f, 3);
    CHECK(v.get(3) == 5.f);
    v.scale(2.f);
    CHECK(v.get(3) == 10.f && v.get(0) == 2.f);
    MCX_volume<float> c(v);                      // deep copy
    c.vol[0] = -1.f;
    CHECK(v.get(0) == 2.f && c.get(3) == 10.f && c.dimxyzt == 8);
    v.reshape(1, 1, 1);
    CHECK(v.dimxyzt == 1 && v.get(0) == 0.f);
}

// ================================================================ MCX_detect
TEST(detect_buffer_layout) {
    MCX_param g = defaultparam();
    g.issavedet = 1;
    g.mediumnum = 3;
    g.maxdetphotons = 2;
    g.savedetflag = dpDetID | dpPPath;
    MCX_detect d(g);
    CHECK(d.detphotondatalen == 4);
    CHECK(d.ppathlen == 3);
    float4 pos(1.f, 2.f, 3.f, 0.5f), dir(0.f, 0.f, 1.f, 0.f);
    float ppath[3] = {0.1f, 0.2f, 0.3f};
    d.addphoton(2, pos, dir, ppath, g);
    d.addphoton(1, pos, dir, ppath, g);
    d.addphoton(1, pos, dir, ppath, g);          // overflows maxdetphotons, counted but not stored
    CHECK(d.detectedphoton == 3);
    CHECK(d.savedcount() == 2);
    CHECK(d.detphotondata[0] == 2.f && d.detphotondata[1] == 0.1f && d.detphotondata[3] == 0.3f);
    CHECK(d.detphotondata[4] == 1.f);

    g.savedetflag = dpDetID | dpPPath | dpExitPos | dpExitDir;
    MCX_detect d2(g);
    CHECK(d2.detphotondatalen == 10);
    d2.addphoton(1, pos, dir, ppath, g);
    CHECK(d2.detphotondata[4] == 1.f && d2.detphotondata[6] == 3.f && d2.detphotondata[9] == 1.f);

    g.issavedet = 0;
    MCX_detect d3(g);
    CHECK(d3.maxdetphotons == 0 && d3.ppathlen == 0);
}

// ================================================================ MCX_photon
struct PhotonFixture {
    MCX_param g = defaultparam();
    MCX_rand ran{11, 22, 33, 44};
    MCX_photon p{float4(0.5f, 0.5f, 0.5f, 1.f), float4(0.f, 0.f, 1.f, 0.f), ran, g};
};

TEST(photon_pencil_launch) {
    PhotonFixture f;
    float w = f.p.launch(float4(1.5f, 2.5f, 3.5f, 1.f), float4(0.f, 0.f, 1.f, 0.f), f.ran, f.g);
    CHECK(w == 1.f);
    CHECK(f.p.pos.x == 1.5f && f.p.pos.y == 2.5f && f.p.pos.z == 3.5f);
    CHECK(f.p.ipos.x == 1 && f.p.ipos.y == 2 && f.p.ipos.z == 3);
    CHECK(f.p.vec.z == 1.f && f.p.len.w == 1.f && std::isnan(f.p.len.x));
}

TEST(photon_area_and_angular_sources) {
    PhotonFixture f;
    bool ok_iso = true, ok_cone = true, ok_disk = true, ok_planar = true, ok_fourier = true;
    double meanz = 0.0;
    const int n = 5000;

    for (int i = 0; i < n; i++) {
        f.g.srctype = stIsotropic;
        f.p.launch(float4(5.f, 5.f, 5.f, 1.f), float4(0.f, 0.f, 1.f, 0.f), f.ran, f.g);
        ok_iso = ok_iso && fabsf(norm3(f.p.vec) - 1.f) < 1e-5f;
        meanz += f.p.vec.z;

        f.g.srctype = stCone;
        f.g.srcparam1 = float4(0.5f, 0.f, 0.f, 0.f);
        f.p.launch(float4(5.f, 5.f, 5.f, 1.f), float4(0.f, 0.f, 1.f, 0.f), f.ran, f.g);
        ok_cone = ok_cone && f.p.vec.z >= cosf(0.5f) - 1e-5f && fabsf(norm3(f.p.vec) - 1.f) < 1e-5f;

        f.g.srctype = stDisk;
        f.g.srcparam1 = float4(4.f, 0.f, 0.f, 0.f);
        f.p.launch(float4(10.f, 10.f, 0.f, 1.f), float4(0.f, 0.f, 1.f, 0.f), f.ran, f.g);
        ok_disk = ok_disk && hypotf(f.p.pos.x - 10.f, f.p.pos.y - 10.f) <= 4.f + 1e-4f && f.p.pos.z == 0.f;
        f.p.launch(float4(10.f, 10.f, 10.f, 1.f), float4(0.6f, 0.f, 0.8f, 0.f), f.ran, f.g);        // tilted disk: stays in the plane normal to Dir
        float4 d(f.p.pos.x - 10.f, f.p.pos.y - 10.f, f.p.pos.z - 10.f, 0.f);
        ok_disk = ok_disk && fabsf(d.x * 0.6f + d.z * 0.8f) < 1e-4f && norm3(d) <= 4.f + 1e-4f;

        f.g.srctype = stPlanar;
        f.g.srcparam1 = float4(6.f, 0.f, 0.f, 0.f);
        f.g.srcparam2 = float4(0.f, 3.f, 0.f, 0.f);
        f.p.launch(float4(1.f, 2.f, 0.f, 1.f), float4(0.f, 0.f, 1.f, 0.f), f.ran, f.g);
        ok_planar = ok_planar && f.p.pos.x >= 1.f && f.p.pos.x <= 7.f && f.p.pos.y >= 2.f && f.p.pos.y <= 5.f && f.p.pos.w == 1.f;

        f.g.srctype = stFourier;
        f.g.srcparam1 = float4(6.f, 0.f, 0.f, 2.f);
        f.g.srcparam2 = float4(0.f, 3.f, 0.f, 0.f);
        float w = f.p.launch(float4(1.f, 2.f, 0.f, 1.f), float4(0.f, 0.f, 1.f, 0.f), f.ran, f.g);
        ok_fourier = ok_fourier && w >= -1e-6f && w <= 1.f + 1e-6f;
    }

    CHECK(ok_iso);
    CHECK_NEAR(meanz / n, 0.0, 0.04);
    CHECK(ok_cone);
    CHECK(ok_disk);
    CHECK(ok_planar);
    CHECK(ok_fourier);
}

TEST(photon_fresnel_reflection) {
    PhotonFixture f;
    f.p.vec = float4(0.f, 0.f, 1.f, 0.f);
    f.p.ipos.w = 2;
    CHECK_NEAR(f.p.reflectcoeff(1.f, 1.5f), 0.04, 1e-6);       // ((n1-n2)/(n1+n2))^2 at normal incidence
    CHECK_NEAR(f.p.reflectcoeff(1.37f, 1.37f), 0.0, 1e-6);
    f.p.vec = float4(0.8f, 0.f, 0.6f, 0.f);                     // sin(theta)=0.8 > 1/1.5: total internal reflection
    CHECK(f.p.reflectcoeff(1.5f, 1.f) == 1.f);
    CHECK(f.p.reflectcoeff(1.f, 1.5f) < 1.f);
}

TEST(photon_snell_transmission) {
    PhotonFixture f;
    f.p.vec = float4(0.6f, 0.f, 0.8f, 0.f);
    f.p.ipos.w = 2;
    f.p.transmit(1.f, 1.5f);
    CHECK_NEAR(f.p.vec.x, 0.4, 1e-6);                            // n1 sin(t1) = n2 sin(t2)
    CHECK_NEAR(f.p.vec.z, sqrt(1.0 - 0.16), 1e-6);
    CHECK_NEAR(norm3(f.p.vec), 1.0, 1e-6);
}

TEST(photon_total_reflection_flips_direction) {
    PhotonFixture f;
    f.p.vec = float4(0.8f, 0.f, 0.6f, 0.f);
    f.p.rvec = float4(1.f / 0.8f, INFINITY, 1.f / 0.6f, 0.f);
    f.p.pos = float4(2.3f, 2.5f, 3.f, 1.f);
    f.p.ipos = short4(2, 2, 3, 2);
    f.p.lastvoxelidx = 42;
    f.p.mediaid = 1;
    int newvox = 43, newmed = 0;
    CHECK(f.p.reflect(1.5f, 1.f, f.ran, newvox, newmed) == 0);
    CHECK(f.p.vec.z == -0.6f && f.p.vec.x == 0.8f);
    CHECK(newvox == 42 && newmed == 1);
    CHECK(f.p.pos.z < 3.f && f.p.ipos.z == 2);                  // pushed back into the previous voxel
}

TEST(photon_rotation_and_scattering) {
    PhotonFixture f;
    f.p.vec = float4(0.f, 0.6f, 0.8f, 0.f);
    f.p.rotatevector(0.f, 1.f, 0.f, 1.f);                       // theta=0 keeps the direction
    CHECK_NEAR(f.p.vec.y, 0.6, 1e-6);
    CHECK_NEAR(f.p.vec.z, 0.8, 1e-6);

    MCX_medium hg(0.f, 1.f, 0.9f, 1.37f), iso(0.f, 1.f, 0.f, 1.37f);
    double meancos = 0.0, meaniso = 0.0;
    bool unit = true;
    const int n = 20000;

    for (int i = 0; i < n; i++) {
        f.p.vec = float4(0.f, 0.f, 1.f, 0.f);
        f.p.scatter(hg, f.ran);
        unit = unit && fabsf(norm3(f.p.vec) - 1.f) < 1e-5f && f.p.vec.w == 1.f && f.p.len.x > 0.f;
        meancos += f.p.vec.z;
        f.p.vec = float4(0.f, 0.f, 1.f, 0.f);
        f.p.scatter(iso, f.ran);
        meaniso += f.p.vec.z;
    }

    CHECK(unit);
    CHECK_NEAR(meancos / n, 0.9, 0.005);                         // Henyey-Greenstein: <cos(theta)> = g
    CHECK_NEAR(meaniso / n, 0.0, 0.02);
}

TEST(photon_step_crosses_voxel_wall) {
    PhotonFixture f;
    MCX_volume<int> vol(4, 4, 4, 1, 1);
    MCX_medium prop(0.1f, 0.f, 1.f, 1.f);
    f.p.launch(float4(0.5f, 0.5f, 0.5f, 1.f), float4(1.f, 0.f, 0.f, 0.f), f.ran, f.g);
    f.p.lastvoxelidx = vol.index(0, 0, 0);
    f.p.len.x = 10.f;
    int idx = f.p.step(vol, prop);
    CHECK(idx == vol.index(1, 0, 0));
    CHECK_NEAR(f.p.pos.x, 1.0, 1e-6);
    CHECK_NEAR(f.p.len.z, 0.5, 1e-6);                            // path length in this voxel
    CHECK_NEAR(f.p.pos.w, exp(-0.05), 1e-6);                     // Beer-Lambert attenuation
    CHECK_NEAR(f.p.len.y, 0.5 * ONE_OVER_C0, 1e-18);             // time of flight
}

TEST(photon_skip_to_domain_from_outside) {
    PhotonFixture f;
    MCX_volume<int> vol(10, 10, 10, 1, 1);
    f.p.launch(float4(5.5f, 5.5f, -3.f, 1.f), float4(0.f, 0.f, 1.f, 0.f), f.ran, f.g);
    CHECK_NEAR(f.p.skip(vol), 3.0, 1e-6);
    CHECK(f.p.pos.z == 0.f && f.p.lastvoxelidx == vol.index(5, 5, 0));
    f.p.launch(float4(5.5f, 5.5f, -3.f, 1.f), float4(0.f, 0.f, -1.f, 0.f), f.ran, f.g);
    CHECK(f.p.skip(vol) < 0.f);                                  // moving away from the domain
    f.p.launch(float4(15.5f, 5.5f, -3.f, 1.f), float4(0.f, 0.f, 1.f, 0.f), f.ran, f.g);
    CHECK(f.p.skip(vol) < 0.f);                                  // missing the domain
}

TEST(photon_launched_on_far_face_does_not_read_outside) {
    PhotonFixture f;
    MCX_volume<int> vol(10, 10, 10, 1, 1);
    MCX_volume<float> out(10, 10, 10, 1);
    MCX_medium props[2] = {MCX_medium(0.f, 0.f, 1.f, 1.f), MCX_medium(0.01f, 1.f, 0.9f, 1.f)};
    MCX_detect det;
    float buf[1] = {};
    f.p.launch(float4(10.f, 5.5f, -3.f, 1.f), float4(0.f, 0.f, 1.f, 0.f), f.ran, f.g);   // x == Nx: outside the [0,Nx) box
    f.p.run<false, false>(vol, out, props, nullptr, det, buf, f.ran, f.g);
    CHECK(f.p.mediaid == 0);
}

// ================================================================ MCX_userio: parsing
TEST(userio_benchmarks) {
    auto io = makeio({"--bench", "cube60"});

    for (auto& name : MCX_benchmarks) {
        io->benchmark(name.get<std::string>());
        CHECK(io->cfg["Session"]["ID"] == name);
        CHECK(io->cfg["Domain"]["Media"].size() >= 2);
        CHECK(io->cfg["Domain"]["Dim"].size() == 3 && io->cfg["Optode"]["Source"].contains("Pos"));

        if (io->cfg["Domain"]["Dim"][0] == 60) {
            io->initdomain();
            CHECK(io->domain.dimxyz == 60 * 60 * 60);
        }
    }

    CHECK_THROWS(makeio({"--bench", "nosuchbenchmark"}));
    CHECK_THROWS(makeio({"nosuchbenchmark"}));
}

TEST(userio_command_line_flags) {
    auto io = makeio({"--bench", "cube60", "-n", "1e3", "-s", "mytest", "-b", "0", "--savedetflag", "21", "-u", "0.5", "-O", "e"});
    CHECK(io->cfg["Session"]["Photons"].get<double>() == 1000.0);
    CHECK(io->cfg["Session"]["ID"] == "mytest");
    CHECK(io->cfg["Session"]["DoMismatch"] == 0);
    CHECK(io->cfg["Session"]["SaveDetFlag"] == 21);
    CHECK(io->cfg["Domain"]["LengthUnit"].get<float>() == 0.5f);
    CHECK(io->cfg["Session"]["OutputType"] == "e");
    CHECK_THROWS(makeio({"--bench", "cube60", "-n"}));                        // flag without a value
    CHECK_THROWS(makeio({"cube60", "cube60b"}));                              // positional args need flags
    CHECK_THROWS(makeio({"--bench", "cube60", "--json", "{bad json"}));
    CHECK_THROWS(makeio({"-f", "/nonexistent/input.json"}));
}

TEST(userio_json_patch_and_file_input) {
    auto io = cube60with(R"({"Optode":{"Source":{"Type":"isotropic"}},"Session":{"Photons":12}})");
    CHECK(io->cfg["Optode"]["Source"]["Type"] == "isotropic");
    CHECK(io->cfg["Optode"]["Source"]["Pos"][0] == 29.0);                      // merged, not replaced
    CHECK(io->cfg["Session"]["Photons"] == 12);

    const char* fname = "umcx_unit_input.json";
    std::ofstream(fname) << io->cfg.dump();
    auto io2 = makeio({fname});
    CHECK(io2->cfg == io->cfg);
    auto io3 = makeio({"-f", fname, "-n", "5"});
    CHECK(io3->cfg["Session"]["Photons"] == 5);
    std::remove(fname);
}

// ================================================================ MCX_userio: shapes
TEST(shape_grid_and_sphere) {
    auto io = cube60with(R"({"Shapes":[{"Grid":{"Tag":1,"Size":[20,20,20]}},{"Sphere":{"O":[10,10,10],"R":5,"Tag":2}}]})");
    CHECK(io->domain.size.x == 20 && io->domain.size.z == 20);
    int n2 = countlabel(io->domain, 2);
    CHECK(n2 > 480 && n2 < 570);                                              // 4/3*pi*5^3 = 523.6
    CHECK((io->domain.get(io->domain.index(10, 10, 10)) & MED_MASK) == 2);
    CHECK((io->domain.get(io->domain.index(1, 1, 1)) & MED_MASK) == 1);
    CHECK(countlabel(io->domain, 1) + n2 == 8000);
}

TEST(shape_box) {
    auto io = cube60with(R"({"Shapes":[{"Grid":{"Tag":1,"Size":[20,20,20]}},{"Box":{"O":[2,4,6],"Size":[3,5,7],"Tag":2}}]})");
    CHECK(countlabel(io->domain, 2) == 3 * 5 * 7);
    CHECK((io->domain.get(io->domain.index(2, 4, 6)) & MED_MASK) == 2);
    CHECK((io->domain.get(io->domain.index(4, 8, 12)) & MED_MASK) == 2);
    CHECK((io->domain.get(io->domain.index(4, 9, 12)) & MED_MASK) == 1);       // just past the y upper bound
    CHECK((io->domain.get(io->domain.index(4, 8, 13)) & MED_MASK) == 1);       // just past the z upper bound
}

TEST(shape_layers) {
    auto io = cube60with(R"({"Shapes":[{"Grid":{"Tag":1,"Size":[4,4,12]}},{"ZLayers":[[1,5,2],[6,10,3]]}]})");
    CHECK(countlabel(io->domain, 2) == 4 * 16);
    CHECK(countlabel(io->domain, 3) == 4 * 16);
    CHECK((io->domain.get(io->domain.index(0, 0, 0)) & MED_MASK) == 1);
    CHECK((io->domain.get(io->domain.index(0, 0, 11)) & MED_MASK) == 1);

    auto iox = cube60with(R"({"Shapes":[{"Grid":{"Tag":1,"Size":[12,4,4]}},{"XLayers":[[1,5,2]]}]})");
    CHECK(countlabel(iox->domain, 2) == 4 * 16 && (iox->domain.get(iox->domain.index(3, 0, 0)) & MED_MASK) == 2);
    auto ioy = cube60with(R"({"Shapes":[{"Grid":{"Tag":1,"Size":[4,12,4]}},{"YLayers":[[1,5,2]]}]})");
    CHECK(countlabel(ioy->domain, 2) == 4 * 16 && (ioy->domain.get(ioy->domain.index(0, 3, 0)) & MED_MASK) == 2);
}

TEST(shape_cylinder_is_finite) {
    auto io = cube60with(R"({"Shapes":[{"Grid":{"Tag":1,"Size":[20,20,20]}},{"Cylinder":{"C0":[0,10,10],"C1":[5,10,10],"R":3,"Tag":2}}]})");
    bool insideaxis = true;

    for (uint32_t x = 0; x < 20; x++)
        for (uint32_t y = 0; y < 20; y++)
            for (uint32_t z = 0; z < 20; z++)
                if ((io->domain.get(io->domain.index(x, y, z)) & MED_MASK) == 2) {
                    insideaxis = insideaxis && (x <= 4);
                }

    CHECK((io->domain.get(io->domain.index(2, 10, 10)) & MED_MASK) == 2);
    CHECK((io->domain.get(io->domain.index(10, 10, 10)) & MED_MASK) == 1);
    CHECK(insideaxis);
}

TEST(shape_unsupported_is_rejected) {
    CHECK_THROWS(cube60with(R"({"Shapes":[{"Grid":{"Tag":1,"Size":[4,4,4]}},{"Torus":{"Tag":2}}]})"));
    CHECK_THROWS(cube60with(R"({"Shapes":[{"Grid":{"Tag":1,"Size":[4,4,4]}},{}]})"));                  // found by fuzzing
    CHECK_THROWS(cube60with(R"({"Shapes":[{"Grid":{"Tag":1,"Size":[4,4,4]}},7]})"));
    CHECK_THROWS(cube60with(R"({"Optode":{"Detector":[{"Pos":[29,19,0]}]}})"));                         // detector without R, found by fuzzing
}

TEST(shape_jdata_raw_array) {
    auto io = withvolume(R"({"_ArraySize_":[2,3,4],"_ArrayType_":"uint8","_ArrayData_":[0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23]})");
    CHECK(io->domain.size.x == 2 && io->domain.size.y == 3 && io->domain.size.z == 4);
    bool ok = true;

    for (int i = 0; i < 24; i++) {
        ok = ok && io->domain.vol[i] == i;
    }

    CHECK(ok);
    CHECK_THROWS(withvolume(R"({"_ArraySize_":[2,3,4],"_ArrayType_":"uint8","_ArrayData_":[0,1,2]})"));   // too few elements
}

TEST(shape_jdata_compressed_array) {
    int32_t i32[24];
    uint8_t u8[24];

    for (int i = 0; i < 24; i++) {
        i32[i] = i * 3, u8[i] = (uint8_t)(i % 4);
    }

    auto io = withvolume(std::string(R"({"_ArraySize_":[2,3,4],"_ArrayType_":"int32","_ArrayZipType_":"zlib","_ArrayZipData_":")") + base64zip(i32, sizeof(i32), zmZlib) + "\"}");
    auto io2 = withvolume(std::string(R"({"_ArraySize_":[2,3,4],"_ArrayType_":"uint8","_ArrayZipType_":"gzip","_ArrayZipData_":")") + base64zip(u8, sizeof(u8), zmGzip) + "\"}");
    bool ok = true;

    for (int i = 0; i < 24; i++) {
        ok = ok && io->domain.vol[i] == i * 3 && io2->domain.vol[i] == i % 4;
    }

    CHECK(ok);
    // payload decompresses to fewer bytes than _ArraySize_ requires
    CHECK_THROWS(withvolume(std::string(R"({"_ArraySize_":[20,30,40],"_ArrayType_":"int32","_ArrayZipType_":"zlib","_ArrayZipData_":")") + base64zip(i32, sizeof(i32), zmZlib) + "\"}}"));
    CHECK_THROWS(withvolume(R"({"_ArraySize_":[2,3,4],"_ArrayType_":"int32","_ArrayZipType_":"zlib","_ArrayZipData_":"bm90IHpsaWI="})"));
}

// ================================================================ MCX_userio: detectors
TEST(detector_masking_on_all_faces) {
    auto io = makeio({"--bench", "cube60"});                            // 4 detectors on the z=0 face
    CHECK(io->domain.get(io->domain.index(29, 19, 0)) & DET_MASK);
    CHECK(!(io->domain.get(io->domain.index(29, 19, 5)) & DET_MASK));
    CHECK((io->domain.get(io->domain.index(29, 19, 0)) & MED_MASK) == 1);      // label is preserved

    auto far = cube60with(R"({"Optode":{"Detector":[{"Pos":[29,29,60],"R":2},{"Pos":[60,60,60],"R":2}]}})");
    CHECK(far->domain.get(far->domain.index(29, 29, 59)) & DET_MASK);           // detector on the z=Nz face
    CHECK(far->domain.get(far->domain.index(59, 59, 59)) & DET_MASK);           // detector on the far corner
    CHECK(!(far->domain.get(far->domain.index(29, 29, 50)) & DET_MASK));

    // an interior voxel next to the background on its -x side is a boundary voxel
    auto inner = cube60with(R"({"Shapes":[{"Grid":{"Tag":0,"Size":[20,20,20]}},{"Box":{"O":[5,0,0],"Size":[15,20,20],"Tag":1}}],"Optode":{"Detector":[{"Pos":[5,10,10],"R":1}]}})");
    CHECK(inner->domain.get(inner->domain.index(5, 10, 10)) & DET_MASK);
}

// ================================================================ MCX_kernel: physics
struct KernelFixture {
    json cfg;
    MCX_param g = defaultparam();
    MCX_volume<int> vol;
    MCX_volume<float> out;
    std::vector<MCX_medium> props;
    std::vector<float4> detpos;

    KernelFixture(uint32_t nx, uint32_t ny, uint32_t nz, std::vector<MCX_medium> media, int photons) : vol(nx, ny, nz, 1, 1), out(nx, ny, nz, 1), props(media) {
        cfg = {{"Session", {{"Photons", photons}, {"RNGSeed", 12345}}}, {"Optode", {{"Source", {{"Pos", {nx / 2.f, ny / 2.f, 0.f}}, {"Dir", {0.f, 0.f, 1.f}}}}}}};
        g.mediumnum = (int)props.size();
    }
    std::pair<double, double> run(bool reflect, bool savedet, MCX_detect& det) {
        g.isreflect = reflect;
        g.issavedet = savedet;
        std::fill_n(out.vol, out.dimxyzt, 0.f);
        return reflect ? (savedet ? MCX_kernel<true, true>(cfg, g, vol, out, detpos.data(), props.data(), det) : MCX_kernel<true, false>(cfg, g, vol, out, detpos.data(), props.data(), det))
               : (savedet ? MCX_kernel<false, true>(cfg, g, vol, out, detpos.data(), props.data(), det) : MCX_kernel<false, false>(cfg, g, vol, out, detpos.data(), props.data(), det));
    }
    double deposited() {
        double sum = 0.0;

        for (uint64_t i = 0; i < out.dimxyzt; i++) {
            sum += out.vol[i];
        }

        return sum;
    }
};

TEST(kernel_pure_absorber_beer_lambert) {
    KernelFixture k(5, 5, 60, {MCX_medium(0.f, 0.f, 1.f, 1.f), MCX_medium(0.01f, 0.f, 1.f, 1.f)}, 1000);
    MCX_detect det;
    std::pair<double, double> e = k.run(false, false, det);
    CHECK_NEAR(e.second, 1000.0, 1e-6);
    CHECK_NEAR(e.first / e.second, exp(-0.6), 1e-4);                         // transmitted fraction through 60 mm
    CHECK_NEAR(k.deposited(), e.second - e.first, 1e-2);
    // energy deposited in voxel z: w(z) - w(z+1) = exp(-mua*z) * (1 - exp(-mua))
    CHECK_NEAR(k.out.get(k.out.index(2, 2, 10)) / 1000.0, exp(-0.1) * (1.0 - exp(-0.01)), 1e-5);
}

TEST(kernel_energy_conservation_with_scattering_and_reflection) {
    KernelFixture k(20, 20, 20, {MCX_medium(0.f, 0.f, 1.f, 1.f), MCX_medium(0.005f, 1.f, 0.9f, 1.37f)}, 2000);

    for (int mode = 0; mode < 4; mode++) {
        k.g.savedetflag = dpDetID | dpPPath;
        k.g.maxdetphotons = 1000;
        k.detpos = {float4(10.f, 5.f, 0.f, 3.f)};
        k.g.detnum = 1;
        k.g.issavedet = mode & 1;
        MCX_detect det(k.g);
        std::pair<double, double> e = k.run(mode >> 1, mode & 1, det);
        CHECK_NEAR(e.second, 2000.0, 1e-6);
        CHECK(e.first > 0.0 && e.first < e.second);
        CHECK_NEAR(k.deposited(), e.second - e.first, 1e-3 * e.second);       // absorbed + escaped = launched
    }
}

TEST(kernel_time_gate_terminates_photons) {
    KernelFixture k(20, 20, 20, {MCX_medium(0.f, 0.f, 1.f, 1.f), MCX_medium(0.f, 10.f, 0.9f, 1.37f)}, 200);
    k.g.tend = 1e-11f;                                                        // ~2 mm of travel
    k.g.rtstep = 1.f / 1e-11f;
    MCX_detect det;
    std::pair<double, double> e = k.run(true, false, det);
    CHECK_NEAR(e.first, e.second, 1e-3);                                     // mua=0: weight is only carried off
    CHECK_NEAR(k.deposited(), 0.0, 1e-6);
}

TEST(kernel_is_reproducible) {
    KernelFixture k(20, 20, 20, {MCX_medium(0.f, 0.f, 1.f, 1.f), MCX_medium(0.01f, 2.f, 0.8f, 1.37f)}, 500);
    MCX_detect det;
    double a = k.run(true, false, det).first, b = k.run(true, false, det).first;
    CHECK_NEAR(a, b, 1e-6 * a);
}

// ================================================================ end-to-end, through the public C API
TEST(run_json_writes_jnifti_and_jdata) {
    const char* input = R"({"Session":{"ID":"umcx_unit_run","Photons":2000,"RNGSeed":1},"Forward":{"T0":0,"T1":5e-9,"Dt":1e-9},
        "Domain":{"Dim":[20,20,20],"Media":[{"mua":0,"mus":0,"g":1,"n":1},{"mua":0.01,"mus":1,"g":0.9,"n":1.37}]},
        "Shapes":[{"Grid":{"Tag":1,"Size":[20,20,20]}}],
        "Optode":{"Source":{"Type":"pencil","Pos":[10,10,0],"Dir":[0,0,1]},"Detector":[{"Pos":[10,5,0],"R":2}]}})";
    std::string buf(input);
    CHECK(MCX_run_json(&buf[0]) == 0);

    std::ifstream fvol("umcx_unit_run.bnii", std::ios::binary), fdet("umcx_unit_run_detp.jdb", std::ios::binary);
    CHECK(fvol.good() && fdet.good());

    if (fvol.good() && fdet.good()) {
        json vol = json::from_bjdata(std::vector<uint8_t>((std::istreambuf_iterator<char>(fvol)), std::istreambuf_iterator<char>()));
        json det = json::from_bjdata(std::vector<uint8_t>((std::istreambuf_iterator<char>(fdet)), std::istreambuf_iterator<char>()));
        CHECK(vol["NIFTIData"]["_ArraySize_"] == json({20, 20, 20, 5}));
        CHECK(vol["NIFTIData"]["_ArrayType_"] == "single");
        CHECK(vol["NIFTIData"]["_ArrayData_"].size() == 20 * 20 * 20 * 5);
        CHECK(det["MCXData"]["Info"]["MediaNum"] == 2);
        CHECK(det["MCXData"]["Info"]["TotalPhoton"] == 2000);
        CHECK(det["MCXData"]["Info"]["DetectedPhoton"].get<int>() > 0);
        CHECK(det["MCXData"]["PhotonRawData"]["_ArraySize_"][1] == 3);       // detid + 2 media partial paths
    }

    std::remove("umcx_unit_run.bnii");
    std::remove("umcx_unit_run_detp.jdb");
}

TEST(run_rejects_labels_without_optical_properties) {
    std::string buf(R"({"Session":{"Photons":10,"DoSaveVolume":0,"DoPartialPath":0},"Forward":{"T0":0,"T1":5e-9,"Dt":5e-9},
        "Domain":{"Dim":[4,4,4],"Media":[[0,0,1,1],[0.01,1,0.9,1.37]]},"Shapes":[{"Grid":{"Tag":5,"Size":[4,4,4]}}],
        "Optode":{"Source":{"Pos":[2,2,0],"Dir":[0,0,1]}}})");
    CHECK_THROWS(MCX_run_json(&buf[0]));
}

TEST(run_rejects_zero_source_direction) {                                       // found by fuzzing: used to hang
    std::string buf(R"({"Session":{"Photons":10,"DoSaveVolume":0,"DoPartialPath":0},"Forward":{"T0":0,"T1":5e-9,"Dt":5e-9},
        "Domain":{"Dim":[4,4,4],"Media":[[0,0,1,1],[0.01,1,0.9,1.37]]},"Shapes":[{"Grid":{"Tag":1,"Size":[4,4,4]}}],
        "Optode":{"Source":{"Pos":[0,2,0],"Dir":[0,0,0]}}})");
    CHECK_THROWS(MCX_run_json(&buf[0]));
}

TEST(run_normalizes_source_direction) {
    std::string a(R"({"Session":{"ID":"umcx_unit_dir","Photons":500,"RNGSeed":3,"DoSaveVolume":0,"DoPartialPath":0},"Forward":{"T0":0,"T1":5e-9,"Dt":5e-9},
        "Domain":{"Dim":[10,10,10],"Media":[[0,0,1,1],[0.01,1,0.9,1.37]]},"Shapes":[{"Grid":{"Tag":1,"Size":[10,10,10]}}],
        "Optode":{"Source":{"Pos":[5,5,0],"Dir":[0,0,5]}}})");
    CHECK(MCX_run_json(&a[0]) == 0);
}

TEST(run_cmd_c_api) {
    std::vector<std::string> args = {"umcx", "--bench", "cube60", "-n", "100", "-S", "0", "-d", "0"};
    std::vector<char*> argv;

    for (auto& a : args) {
        argv.push_back(&a[0]);
    }

    CHECK(MCX_run_cmd(argv.data(), (int)argv.size()) == 0);
}

TEST(run_short_source_param_is_zero_padded) {
    std::string buf(R"({"Session":{"Photons":100,"DoSaveVolume":0,"DoPartialPath":0},"Forward":{"T0":0,"T1":5e-9,"Dt":5e-9},
        "Domain":{"Dim":[10,10,10],"Media":[[0,0,1,1],[0.01,1,0.9,1.37]]},"Shapes":[{"Grid":{"Tag":1,"Size":[10,10,10]}}],
        "Optode":{"Source":{"Type":"cone","Pos":[5,5,0],"Dir":[0,0,1],"Param1":[0.3]}}})");
    CHECK(MCX_run_json(&buf[0]) == 0);
}

// ---------------------------------------------------------------- driver
int main(int argc, char* argv[]) {
    int nfailedtests = 0;
#ifdef _OPENMP
    omp_set_num_threads(std::min(4, omp_get_max_threads()));   // the tests are small: more threads only add overhead
#endif

    for (auto& t : registry()) {
        if (argc > 1 && std::string(t.first).find(argv[1]) == std::string::npos) {
            continue;
        }

        int before = g_failed;
        printf("[ RUN  ] %s\n", t.first);

        try {
            t.second();
        } catch (const std::exception& e) {
            g_failed++;
            printf("  FAIL unexpected exception: %s\n", e.what());
        }

        printf("[ %s ] %s\n", (g_failed == before ? " OK " : "FAIL"), t.first);
        nfailedtests += (g_failed != before);
    }

    printf("\n%d checks, %d failed, %d test(s) failed\n", g_checks, g_failed, nfailedtests);
    return nfailedtests ? 1 : 0;
}
