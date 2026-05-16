// #define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include "include/nlohmann/json.hpp"
#include "include/sqlite3/sqlite3.h"
#include <iostream>
#include <random>
#include <unordered_map>
#include <ctime>

using json = nlohmann::json;

// Token 存储（内存中，服务器重启会丢失）
std::unordered_map<std::string, std::string> token_store; // token -> username

// 生成随机 token
std::string generate_token() {
    static const char* chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static std::mt19937 rng(std::time(nullptr));
    static std::uniform_int_distribution<int> dist(0, 61);
    std::string token;
    for (int i = 0; i < 32; ++i) {
        token += chars[dist(rng)];
    }
    return token;
}

// 初�?�化数据库表
void init_db() {
    sqlite3* db;
    if (sqlite3_open("game.db", &db) != SQLITE_OK) {
        std::cerr << "无法打开数据库，注册功能不可�?" << std::endl;
        return;
    }
    const char* sql = "CREATE TABLE IF NOT EXISTS users ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "username TEXT UNIQUE, "
                      "password TEXT);";
    char* errMsg = nullptr;
    sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    if (errMsg) {
        std::cerr << "建表错�??: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    } else {
        std::cout << "数据库表已准备就�?" << std::endl;
    }
    sqlite3_close(db);
}

int main() {
    init_db();

    httplib::Server svr;

    // 根路�?
    svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        res.set_content("Hello from C++ Game Platform!", "text/plain");
    });

    // JSON 状态接�?
    svr.Get("/api/status", [](const httplib::Request& req, httplib::Response& res) {
        json j;
        j["status"] = "ok";
        j["players"] = 0;
        j["games"] = {"pushbox", "guess"};
        res.set_content(j.dump(), "application/json");
    });

    // 注册接口
    svr.Post("/api/register", [](const httplib::Request& req, httplib::Response& res) {
        json result;
        try {
            auto body = json::parse(req.body);
            std::string username = body["username"];
            std::string password = body["password"];

            sqlite3* db;
            int rc = sqlite3_open("game.db", &db);
            if (rc != SQLITE_OK) {
                result["success"] = false;
                result["error"] = "Database open failed";
                res.set_content(result.dump(), "application/json");
                return;
            }

            std::string sql = "INSERT INTO users (username, password) VALUES ('" 
                              + username + "', '" + password + "');";
            char* errMsg = nullptr;
            rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK) {
                result["success"] = false;
                result["error"] = errMsg;
                sqlite3_free(errMsg);
            } else {
                result["success"] = true;
            }
            sqlite3_close(db);
        } catch (const std::exception& e) {
            result["success"] = false;
            result["error"] = e.what();
        }
        res.set_content(result.dump(), "application/json");
    });

    // 登录接口
    svr.Post("/api/login", [](const httplib::Request& req, httplib::Response& res) {
        json result;
        try {
            auto body = json::parse(req.body);
            std::string username = body["username"];
            std::string password = body["password"];

            sqlite3* db;
            int rc = sqlite3_open("game.db", &db);
            if (rc != SQLITE_OK) {
                result["success"] = false;
                result["error"] = "Database open failed";
                res.set_content(result.dump(), "application/json");
                return;
            }

            std::string sql = "SELECT password FROM users WHERE username = '" + username + "';";
            std::string stored_pw;
            auto callback = [](void* data, int argc, char** argv, char** colName) -> int {
                if (argc > 0 && argv[0]) *((std::string*)data) = argv[0];
                return 0;
            };
            rc = sqlite3_exec(db, sql.c_str(), callback, &stored_pw, nullptr);
            sqlite3_close(db);

            if (rc == SQLITE_OK && stored_pw == password) {
                std::string token = generate_token();
                token_store[token] = username;
                result["success"] = true;
                result["token"] = token;
            } else {
                result["success"] = false;
                result["error"] = "Invalid username or password";
            }
        } catch (const std::exception& e) {
            result["success"] = false;
            result["error"] = e.what();
        }
        res.set_content(result.dump(), "application/json");
    });

    // 受保护接口：获取当前用户信息
    svr.Get("/api/me", [](const httplib::Request& req, httplib::Response& res) {
        std::string auth = req.get_header_value("Authorization");
        std::string token;
        if (auth.substr(0, 7) == "Bearer ") {
            token = auth.substr(7);
        } else {
            res.status = 401;
            res.set_content(R"({"error":"Missing or invalid Authorization header"})", "application/json");
            return;
        }

        auto it = token_store.find(token);
        if (it == token_store.end()) {
            res.status = 401;
            res.set_content(R"({"error":"Invalid token"})", "application/json");
            return;
        }

        json result;
        result["username"] = it->second;
        res.set_content(result.dump(), "application/json");
    });

    std::cout << "Server running at http://localhost:8080" << std::endl;
    svr.listen("localhost", 8080);

    return 0;
}