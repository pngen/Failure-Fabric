#pragma once
// Minimal deterministic test framework (no external deps).
#include <cstdio>
#include <string>
#include <vector>
#include <sstream>
#include <functional>
#include <type_traits>
#include <ostream>
#include <utility>

namespace fft {
struct Case { const char* name; std::function<void()> fn; };
inline std::vector<Case>& registry() { static std::vector<Case> r; return r; }
struct Registrar { Registrar(const char* n, std::function<void()> f) { registry().push_back({n, std::move(f)}); } };
struct Failure { std::string msg; };

template <typename T>
inline std::string pp(const T& v) {
  if constexpr (std::is_enum_v<T>) {
    return std::to_string(static_cast<std::underlying_type_t<T>>(v));
  } else if constexpr (std::is_same_v<T, std::string>) {
    return v;
  } else if constexpr (std::is_same_v<T, const char*>) {
    return std::string(v);
  } else if constexpr (std::is_arithmetic_v<T>) {
    if constexpr (std::is_same_v<T, bool>) return v ? "true" : "false";
    else return std::to_string(v);
  } else {
    return "<unprintable>";
  }
}

inline int run_all() {
  int failed = 0, passed = 0;
  for (auto& c : registry()) {
    try { c.fn(); ++passed; std::printf("  PASS  %s\n", c.name); }
    catch (const ::fft::Failure& f) { ++failed; std::printf("  FAIL  %s : %s\n", c.name, f.msg.c_str()); }
    catch (const std::exception& e) { ++failed; std::printf("  FAIL  %s : std::exception: %s\n", c.name, e.what()); }
    catch (...) { ++failed; std::printf("  FAIL  %s : unknown exception\n", c.name); }
  }
  std::printf("== %d passed, %d failed ==\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
}

#define FFT_CONCAT2(a,b) a##b
#define FFT_CONCAT(a,b) FFT_CONCAT2(a,b)
#define TEST(name) \
  static void FFT_CONCAT(fft_fn_, __LINE__)(); \
  static ::fft::Registrar FFT_CONCAT(fft_reg_, __LINE__)(#name, &FFT_CONCAT(fft_fn_, __LINE__)); \
  static void FFT_CONCAT(fft_fn_, __LINE__)()

#define CHECK(cond) do { if (!(cond)) throw ::fft::Failure{ std::string("CHECK failed: ") + #cond + std::string(" at line ") + std::to_string(__LINE__) }; } while(0)
#define CHECK_EQ(a,b) do { auto _a=(a); auto _b=(b); if (!(_a==_b)) { std::ostringstream _o; _o<<"CHECK_EQ failed: "<<#a<<" != "<<#b<<" ("<<::fft::pp(_a)<<" vs "<<::fft::pp(_b)<<") at line "<<__LINE__; throw ::fft::Failure{_o.str()}; } } while(0)
#define CHECK_NE(a,b) do { auto _a=(a); auto _b=(b); if ((_a==_b)) { std::ostringstream _o; _o<<"CHECK_NE failed: "<<#a<<" == "<<#b<<" ("<<::fft::pp(_a)<<") at line "<<__LINE__; throw ::fft::Failure{_o.str()}; } } while(0)
#define CHECK_THROWS(cond) do { bool _thrown=false; try { (void)(cond); } catch(...) { _thrown=true; } if(!_thrown) throw ::fft::Failure{ std::string("CHECK_THROWS failed: ")+#cond }; } while(0)