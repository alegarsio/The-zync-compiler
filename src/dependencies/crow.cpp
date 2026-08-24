#include "../../include/dependencies/crow.hpp"

namespace ZyncDependencies {

std::string getCrowTemplate() {
    return R"(#pragma once

#include <crow.h>
#include <crow/mustache.h>
#include <string>
#include <functional>
#include <map>
#include <sstream>
#include <type_traits>
#include <tuple>
#include <fstream>

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

    inline std::string render(const std::string& filename, const std::map<std::string, std::string>& ctx = {}) {
        crow::mustache::context c;
        for (const auto& [k, v] : ctx) {
            c[k] = v;
        }
        auto page = crow::mustache::load(filename);
        return page.render_string(c);
    }

    template <typename T>
    struct HandlerTraits : HandlerTraits<decltype(&T::operator())> {};

    template <typename C, typename R, typename... Args>
    struct HandlerTraits<R(C::*)(Args...) const> {
        using ReturnType = R;
        using ArgsTuple = std::tuple<Args...>;
        static constexpr size_t arity = sizeof...(Args);
    };

    template <typename C, typename R, typename... Args>
    struct HandlerTraits<R(C::*)(Args...)> {
        using ReturnType = R;
        using ArgsTuple = std::tuple<Args...>;
        static constexpr size_t arity = sizeof...(Args);
    };

    template <typename R, typename... Args>
    struct HandlerTraits<R(*)(Args...)> {
        using ReturnType = R;
        using ArgsTuple = std::tuple<Args...>;
        static constexpr size_t arity = sizeof...(Args);
    };

    class App {
    private:
        crow::SimpleApp app;

        template <bool IsHtml, typename F, typename... Args>
        auto makeCrowCallback(F handler, std::tuple<Args...>) {
            return [handler](Args... args) {
                crow::response res;
                if constexpr (IsHtml) {
                    res.set_header("Content-Type", "text/html; charset=utf-8");
                    res.body = handler(args...);
                } else {
                    using RetT = typename HandlerTraits<F>::ReturnType;
                    if constexpr (std::is_same_v<RetT, void>) {
                        handler(args...);
                        res.set_header("Content-Type", "application/json");
                        res.body = "{\"status\":\"ok\"}";
                    } else if constexpr (std::is_same_v<RetT, std::string> || std::is_same_v<RetT, const char*>) {
                        std::string val = handler(args...);
                        if (val.find("<html") != std::string::npos || val.find("<!DOCTYPE") != std::string::npos || val.find("<div") != std::string::npos || val.find("<h1") != std::string::npos) {
                            res.set_header("Content-Type", "text/html; charset=utf-8");
                            res.body = val;
                        } else {
                            res.set_header("Content-Type", "text/plain; charset=utf-8");
                            res.body = val;
                        }
                    } else {
                        res.set_header("Content-Type", "application/json");
                        res.body = toJson(handler(args...));
                    }
                }
                return res;
            };
        }

    public:
        App() = default;

        template <typename F>
        void get(const std::string& route, F handler) {
            using ArgsTuple = typename HandlerTraits<F>::ArgsTuple;
            app.route_dynamic(route)(makeCrowCallback<false>(handler, ArgsTuple{}));
        }

        template <typename F>
        void post(const std::string& route, F handler) {
            using ArgsTuple = typename HandlerTraits<F>::ArgsTuple;
            app.route_dynamic(route).methods(crow::HTTPMethod::POST)(makeCrowCallback<false>(handler, ArgsTuple{}));
        }

        template <typename F>
        void html(const std::string& route, F handler) {
            using ArgsTuple = typename HandlerTraits<F>::ArgsTuple;
            app.route_dynamic(route)(makeCrowCallback<true>(handler, ArgsTuple{}));
        }

        void run(int port) {
            app.port(port).multithreaded().run();
        }
    };
}
)";
}

}