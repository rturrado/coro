#pragma once

#include "coro_sequence.h"

#include <asio.hpp>
#include <chrono>
#include <exception>
#include <fmt/core.h>
#include <thread>  // sleep_for


namespace rtc::coro::client_server_asio {
    constinit const int port{ 1234 };

    inline asio::awaitable<void> serve(asio::ip::tcp::socket socket) {
        std::string data{};
        for (auto&& n : coro_sequence()) {
            int data[]{ n };

            co_await asio::async_write(socket, asio::buffer(data), asio::use_awaitable);
            fmt::print("[serve] Written {} bytes: {}\n", sizeof(data), data[0]);

            using namespace std::chrono_literals;
            std::this_thread::sleep_for(1s);

            if (n > 3) {
                co_return;
            }
        }
    }

    inline asio::awaitable<void> server(asio::io_context& io_ctx) {
        fmt::print("[server] Starting\n");
        fmt::print("[server] Accepting a connection from a client...\n");
        asio::ip::tcp::acceptor acceptor{ io_ctx, { asio::ip::tcp::v4(), port } };
        fmt::print("[server] Accepted a connection from a client\n");
        fmt::print("[server] Spawning serve...\n");
        co_spawn(
            io_ctx,
            serve(co_await acceptor.async_accept(asio::use_awaitable)),
            asio::detached
        );
    }

    inline asio::awaitable<void> client(asio::io_context& io_ctx) {
        fmt::print("[client] Starting\n");
        asio::ip::tcp::resolver resolver{ io_ctx };
        asio::ip::tcp::resolver::query query{ "localhost", std::to_string(port) };
        asio::ip::tcp::resolver::iterator endpoint_iterator{ resolver.resolve(query) };
        asio::ip::tcp::socket socket{ io_ctx };
        co_await asio::async_connect(socket, endpoint_iterator, asio::use_awaitable);
        fmt::print("[client] Connected to server\n");

        for (;;) {
            int data[1]{};
            std::size_t n{ co_await asio::async_read(socket, asio::buffer(data), asio::use_awaitable) };
            fmt::print("[client] Received {} bytes: {}\n", sizeof(data), data[0]);

            if (data[0] > 3) {
                co_return;
            }
        }
    }
}  // namespace rtc::coro::client_server_asio


inline void test_client_server_asio() {
    using namespace rtc::coro::client_server_asio;

    try {
        asio::io_context io_ctx{};

        asio::signal_set signals(io_ctx, SIGINT, SIGTERM);
        signals.async_wait([&io_ctx](auto, auto) { io_ctx.stop(); });

        fmt::print("Press CTRL + c to finish at anytime...\n\n");
        co_spawn(io_ctx, server(io_ctx), asio::detached);
        co_spawn(io_ctx, client(io_ctx), asio::detached);
        io_ctx.run();
    }
    catch (const std::exception& e) {
        fmt::print("Error: {}\n", e.what());
    }
}
