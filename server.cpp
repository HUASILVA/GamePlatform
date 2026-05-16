// #define CPPHTTPLIB_OPENSSL_SUPPORT   // 保持注释，不使用 SSL
#include "httplib.h"
#include "include/nlohmann/json.hpp"   // 新增 JSON 库头文件
#include <iostream>

using json = nlohmann::json;

int main() {
    httplib::Server svr;

    // 原有的根路径
    svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        res.set_content("Hello from C++ Game Platform!", "text/plain");
    });

    // 新增 JSON 接口
    svr.Get("/api/status", [](const httplib::Request& req, httplib::Response& res) {
        json j;
        j["status"] = "ok";
        j["players"] = 0;
        j["games"] = {"pushbox", "guess"};
        res.set_content(j.dump(), "application/json");
    });

    std::cout << "Server running at http://localhost:8080" << std::endl;
    svr.listen("localhost", 8080);
    return 0;
}