#include "../../include/dependencies/crow.hpp"

namespace ZyncDependencies {

std::string getCrowTemplate() {
    return R"(#pragma once

#include <crow.h>
#include <string>
#include <functional>
#include <map>
#include <sstream>
#include <type_traits>

namespace ZyncCrow {

    inline std::string toJson(const std::string& str) { return str; }
    inline std::string toJson(const char* str) { return std::string(str); }

    template <typename K, typename V>
    std::string toJson(const std::map<K, V>& m) {
        std::ostringstream ss;
        ss << "{";
        bool first = true;
        for (const auto& [k, v] : m) {
            if (!first) ss << ", ";
            ss << "\"" << k << "\": ";
            if constexpr (std::is_same_v<V, std::string> || std::is_same_v<V, const char*>) {
                ss << "\"" << v << "\"";
            } else {
                ss << v;
            }
            first = false;
        }
        ss << "}";
        return ss.str();
    }

    class App {
    private:
        crow::SimpleApp app;
    public:
        App() = default;

        template <typename F>
        void get(const std::string& route, F handler) {
            app.route_dynamic(route)([handler]() {
                crow::response res;
                res.set_header("Content-Type", "application/json");
                res.body = toJson(handler());
                return res;
            });
        }

        template <typename F>
        void post(const std::string& route, F handler) {
            app.route_dynamic(route).methods(crow::HTTPMethod::POST)([handler]() {
                crow::response res;
                res.set_header("Content-Type", "application/json");
                res.body = toJson(handler());
                return res;
            });
        }

        void run(int port) {
            app.port(port).multithreaded().run();
        }
    };
}
)";
}

}