#include "../../include/dependencies/crow.hpp"

namespace ZyncDependencies {

std::string getCrowTemplate() {
    return R"(#pragma once

#include <crow.h>
#include <crow/mustache.h>
#include <string>
#include <functional>
#include <map>
#include <vector>
#include <sstream>
#include <type_traits>
#include <tuple>
#include <fstream>

namespace ZyncCrow {

    template <typename T>
    std::string toJson(const T& val);

    inline std::string toJson(const std::string& str) {
        return "\"" + str + "\"";
    }

    inline std::string toJson(const char* str) {
        return "\"" + std::string(str) + "\"";
    }

    inline std::string toJson(bool b) {
        return b ? "true" : "false";
    }

    template <typename T>
    std::string toJson(const std::vector<T>& vec) {
        std::ostringstream ss;
        ss << "[";
        for (size_t i = 0; i < vec.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << toJson(vec[i]);
        }
        ss << "]";
        return ss.str();
    }

    template <typename K, typename V>
    std::string toJson(const std::map<K, V>& m) {
        std::ostringstream ss;
        ss << "{";
        bool first = true;
        for (const auto& [k, v] : m) {
            if (!first) ss << ", ";
            ss << "\"" << k << "\": " << toJson(v);
            first = false;
        }
        ss << "}";
        return ss.str();
    }

    template <typename T>
    std::string toJson(const T& val) {
        if constexpr (std::is_arithmetic_v<T>) {
            return std::to_string(val);
        } else {
            std::ostringstream ss;
            ss << val;
            return ss.str();
        }
    }

    struct Request {
        std::map<std::string, std::string> query;
        std::string body;
        std::string method;

        std::string getQuery(const std::string& key, const std::string& fallback = "") const {
            auto it = query.find(key);
            if (it != query.end()) return it->second;
            return fallback;
        }

        std::string getBody() const {
            return body;
        }

        std::string getJson(const std::string& key, const std::string& fallback = "") const {
            auto x = crow::json::load(body);
            if (!x || !x.has(key)) return fallback;
            if (x[key].t() == crow::json::type::String) {
                return x[key].s();
            } else if (x[key].t() == crow::json::type::Number) {
                return std::to_string(x[key].i());
            }
            return fallback;
        }
    };

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

    template <typename Tuple>
    struct TupleDropFirst;

    template <typename First, typename... Rest>
    struct TupleDropFirst<std::tuple<First, Rest...>> {
        using type = std::tuple<Rest...>;
    };

    template <>
    struct TupleDropFirst<std::tuple<>> {
        using type = std::tuple<>;
    };

    template <typename Tuple>
    struct TupleFirst;

    template <typename First, typename... Rest>
    struct TupleFirst<std::tuple<First, Rest...>> {
        using type = First;
    };

    template <>
    struct TupleFirst<std::tuple<>> {
        using type = void;
    };

    class App;

    class Group {
    private:
        App& app;
        std::string prefix;

        std::string normalizePath(const std::string& subRoute) const {
            if (subRoute.empty() || subRoute == "/") return prefix;
            std::string sub = subRoute;
            if (sub.front() != '/') sub = "/" + sub;
            return prefix + sub;
        }

    public:
        Group(App& parentApp, std::string basePrefix)
            : app(parentApp), prefix(std::move(basePrefix)) {
            if (prefix.empty()) prefix = "/";
            if (prefix.front() != '/') prefix = "/" + prefix;
            while (prefix.length() > 1 && prefix.back() == '/') {
                prefix.pop_back();
            }
        }

        template <typename F>
        void get(const std::string& route, F handler);

        template <typename F>
        void post(const std::string& route, F handler);

        template <typename F>
        void put(const std::string& route, F handler);

        template <typename F>
        void del(const std::string& route, F handler);

        template <typename F>
        void html(const std::string& route, F handler);

        Group group(const std::string& subPrefix) {
            return Group(app, normalizePath(subPrefix));
        }
    };

    class App {
    public:
        crow::SimpleApp server;

        App() = default;

        static Request parseRequest(const crow::request& req) {
            Request zReq;
            zReq.body = req.body;
            zReq.method = crow::method_name(req.method);
            if (req.url_params.keys().size() > 0) {
                for (const auto& key : req.url_params.keys()) {
                    zReq.query[key] = req.url_params.get(key);
                }
            }
            return zReq;
        }

        template <bool IsHtml, typename F, typename... Args>
        auto makeCrowCallback(F handler, std::tuple<Args...>) {
            using FullArgsTuple = typename HandlerTraits<F>::ArgsTuple;
            using FirstArg = typename TupleFirst<FullArgsTuple>::type;

            if constexpr (std::is_same_v<std::decay_t<FirstArg>, Request>) {
                return [handler](const crow::request& req, Args... args) {
                    crow::response res;
                    res.set_header("Access-Control-Allow-Origin", "*");
                    Request zReq = parseRequest(req);
                    auto result = handler(zReq, args...);
                    
                    if constexpr (IsHtml) {
                        res.set_header("Content-Type", "text/html; charset=utf-8");
                        res.body = result;
                    } else {
                        using RetT = typename HandlerTraits<F>::ReturnType;
                        if constexpr (std::is_same_v<RetT, void>) {
                            res.set_header("Content-Type", "application/json");
                            res.body = "{\"status\":\"ok\"}";
                        } else if constexpr (std::is_same_v<RetT, std::string> || std::is_same_v<RetT, const char*>) {
                            if (result.find("<html") != std::string::npos || result.find("<!DOCTYPE") != std::string::npos || result.find("<div") != std::string::npos || result.find("<h1") != std::string::npos) {
                                res.set_header("Content-Type", "text/html; charset=utf-8");
                                res.body = result;
                            } else {
                                res.set_header("Content-Type", "text/plain; charset=utf-8");
                                res.body = result;
                            }
                        } else {
                            res.set_header("Content-Type", "application/json");
                            res.body = toJson(result);
                        }
                    }
                    return res;
                };
            } else {
                return [handler](Args... args) {
                    crow::response res;
                    res.set_header("Access-Control-Allow-Origin", "*");
                    auto result = handler(args...);
                    
                    if constexpr (IsHtml) {
                        res.set_header("Content-Type", "text/html; charset=utf-8");
                        res.body = result;
                    } else {
                        using RetT = typename HandlerTraits<F>::ReturnType;
                        if constexpr (std::is_same_v<RetT, void>) {
                            res.set_header("Content-Type", "application/json");
                            res.body = "{\"status\":\"ok\"}";
                        } else if constexpr (std::is_same_v<RetT, std::string> || std::is_same_v<RetT, const char*>) {
                            if (result.find("<html") != std::string::npos || result.find("<!DOCTYPE") != std::string::npos || result.find("<div") != std::string::npos || result.find("<h1") != std::string::npos) {
                                res.set_header("Content-Type", "text/html; charset=utf-8");
                                res.body = result;
                            } else {
                                res.set_header("Content-Type", "text/plain; charset=utf-8");
                                res.body = result;
                            }
                        } else {
                            res.set_header("Content-Type", "application/json");
                            res.body = toJson(result);
                        }
                    }
                    return res;
                };
            }
        }

        template <typename F>
        void get(const std::string& route, F handler) {
            using FullArgs = typename HandlerTraits<F>::ArgsTuple;
            using FirstArg = typename TupleFirst<FullArgs>::type;
            if constexpr (std::is_same_v<std::decay_t<FirstArg>, Request>) {
                using RestArgs = typename TupleDropFirst<FullArgs>::type;
                server.route_dynamic(route)(makeCrowCallback<false>(handler, RestArgs{}));
            } else {
                server.route_dynamic(route)(makeCrowCallback<false>(handler, FullArgs{}));
            }
        }

        template <typename F>
        void post(const std::string& route, F handler) {
            using FullArgs = typename HandlerTraits<F>::ArgsTuple;
            using FirstArg = typename TupleFirst<FullArgs>::type;
            if constexpr (std::is_same_v<std::decay_t<FirstArg>, Request>) {
                using RestArgs = typename TupleDropFirst<FullArgs>::type;
                server.route_dynamic(route).methods(crow::HTTPMethod::POST)(makeCrowCallback<false>(handler, RestArgs{}));
            } else {
                server.route_dynamic(route).methods(crow::HTTPMethod::POST)(makeCrowCallback<false>(handler, FullArgs{}));
            }
        }

        template <typename F>
        void put(const std::string& route, F handler) {
            using FullArgs = typename HandlerTraits<F>::ArgsTuple;
            using FirstArg = typename TupleFirst<FullArgs>::type;
            if constexpr (std::is_same_v<std::decay_t<FirstArg>, Request>) {
                using RestArgs = typename TupleDropFirst<FullArgs>::type;
                server.route_dynamic(route).methods(crow::HTTPMethod::PUT)(makeCrowCallback<false>(handler, RestArgs{}));
            } else {
                server.route_dynamic(route).methods(crow::HTTPMethod::PUT)(makeCrowCallback<false>(handler, FullArgs{}));
            }
        }

        template <typename F>
        void del(const std::string& route, F handler) {
            using FullArgs = typename HandlerTraits<F>::ArgsTuple;
            using FirstArg = typename TupleFirst<FullArgs>::type;
            if constexpr (std::is_same_v<std::decay_t<FirstArg>, Request>) {
                using RestArgs = typename TupleDropFirst<FullArgs>::type;
                server.route_dynamic(route).methods(crow::HTTPMethod::DELETE)(makeCrowCallback<false>(handler, RestArgs{}));
            } else {
                server.route_dynamic(route).methods(crow::HTTPMethod::DELETE)(makeCrowCallback<false>(handler, FullArgs{}));
            }
        }

        template <typename F>
        void html(const std::string& route, F handler) {
            using FullArgs = typename HandlerTraits<F>::ArgsTuple;
            using FirstArg = typename TupleFirst<FullArgs>::type;
            if constexpr (std::is_same_v<std::decay_t<FirstArg>, Request>) {
                using RestArgs = typename TupleDropFirst<FullArgs>::type;
                server.route_dynamic(route)(makeCrowCallback<true>(handler, RestArgs{}));
            } else {
                server.route_dynamic(route)(makeCrowCallback<true>(handler, FullArgs{}));
            }
        }

        Group group(const std::string& prefix) {
            return Group(*this, prefix);
        }

        void run(int port) {
            server.port(port).multithreaded().run();
        }

        void run() {
            server.port(8080).multithreaded().run();
        }

        void listen(int port) {
            server.port(port).multithreaded().run();
        }
    };

    template <typename F>
    void Group::get(const std::string& route, F handler) {
        app.get(normalizePath(route), handler);
    }

    template <typename F>
    void Group::post(const std::string& route, F handler) {
        app.post(normalizePath(route), handler);
    }

    template <typename F>
    void Group::put(const std::string& route, F handler) {
        app.put(normalizePath(route), handler);
    }

    template <typename F>
    void Group::del(const std::string& route, F handler) {
        app.del(normalizePath(route), handler);
    }

    template <typename F>
    void Group::html(const std::string& route, F handler) {
        app.html(normalizePath(route), handler);
    }
}
)";
}

}