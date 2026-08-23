#pragma once

#include <crow.h>
#include <string>
#include <functional>

namespace crow_wrapper {

    class App {
    private:
        crow::SimpleApp app;

    public:
        App() = default;

        void get(const std::string& route, std::function<std::string()> handler) {
            CROW_ROUTE(app, route.c_str())(handler);
        }

        void post(const std::string& route, std::function<std::string()> handler) {
            CROW_ROUTE(app, route.c_str()).methods(crow::HTTPMethod::POST)(handler);
        }

        void run(int port) {
            app.port(port).multithreaded().run();
        }
    };

}
