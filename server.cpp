//#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include <iostream>

int main() {
    httplib::Server svr;
    svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        res.set_content("Hello from C++ Game Platform!", "text/plain");
    });
    std::cout << "Server running at http://localhost:8080" << std::endl;
    svr.listen("localhost", 8080);
    return 0;
}