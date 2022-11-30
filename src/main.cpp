#include "client_server_asio.h"
#include "coro_f.h"
#include "coro_g.h"
#include "MPP_MCpp/example_1.h"
#include "MPP_MCpp/example_2.h"
#include "MPP_MCpp/example_3a.h"
#include "MPP_MCpp/example_3b.h"
#include "MPP_MCpp/example_3c.h"
#include "MPP_MCpp/example_4a.h"
#include "MPP_MCpp/example_4b.h"
#include "MPP_MCpp/example_4c.h"
#include "MPP_MCpp/example_4d.h"
#include "my_coro.h"

#include <fmt/core.h>

using namespace rtc::coro;
using namespace rtc::coro::my_coro;
using namespace rtc::coro::mpp_mcpp::v1;
using namespace rtc::coro::mpp_mcpp::v2;
using namespace rtc::coro::mpp_mcpp::v3a;
using namespace rtc::coro::mpp_mcpp::v3b;
using namespace rtc::coro::mpp_mcpp::v3c;
using namespace rtc::coro::mpp_mcpp::v4a;
using namespace rtc::coro::mpp_mcpp::v4b;
using namespace rtc::coro::mpp_mcpp::v4c;
using namespace rtc::coro::mpp_mcpp::v4d;


int main() {
    fmt::print("[Testing coro_f...]\n\n"); test_coro_f();
    fmt::print("\n\n[Testing coro_g...]\n\n"); test_coro_g();
    fmt::print("\n\n[Testing client_server_asio...]\n\n"); test_client_server_asio();
    fmt::print("\n\n[Testing my_coro...]\n\n"); test_my_coro();
    fmt::print("\n\n[Testing example_1...]\n\n"); example_1();
    fmt::print("\n\n[Testing example_2...]\n\n"); example_2();
    fmt::print("\n\n[Testing example_3a...]\n\n"); example_3a();
    fmt::print("\n\n[Testing example_3b...]\n\n"); example_3b();
    fmt::print("\n\n[Testing example_3c...]\n\n"); example_3c();
    fmt::print("\n\n[Testing example_4a...]\n\n"); example_4a();
    fmt::print("\n\n[Testing example_4b...]\n\n"); example_4b();
    fmt::print("\n\n[Testing example_4c...]\n\n"); example_4c();
    fmt::print("\n\n[Testing example_4d...]\n\n"); example_4d();
}
