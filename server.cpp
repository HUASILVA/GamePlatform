// #define CPPHTTPLIB_OPENSSL_SUPPORT   // 如果不需要 HTTPS 就保持注释
#include "httplib.h"
#include "include/nlohmann/json.hpp"
#include "include/sqlite3/sqlite3.h"   // 注意路径根据你的实际位置调整
#include <iostream>
#include <random>
#include <unordered_map>

using json = nlohmann::json;

// 初始化数据库表（创建 users 表）
void init_db() {
    sqlite3* db;
    if (sqlite3_open("game.db", &db) != SQLITE_OK) {
        std::cerr << "无法打开数据库，注册功能不可用" << std::endl;
        return;
    }
    const char* sql = "CREATE TABLE IF NOT EXISTS users ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "username TEXT UNIQUE, "
                      "password TEXT);";
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "建表失败: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    } else {
        std::cout << "数据库表已准备就绪" << std::endl;
    }
    sqlite3_close(db);
}

int main() {
    // 初始化数据库
    init_db();

    httplib::Server svr;

    // ---------- 基础路由 ----------
    svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        res.set_content("Hello from C++ Game Platform!", "text/plain");
    });

    // ---------- JSON 状态接口 ----------
    svr.Get("/api/status", [](const httplib::Request& req, httplib::Response& res) {
        json j;
        j["status"] = "ok";
        j["players"] = 0;
        j["games"] = {"pushbox", "guess"};
        res.set_content(j.dump(), "application/json");
    });

    // ---------- 用户注册接口 ----------
    svr.Post("/api/register", [](const httplib::Request& req, httplib::Response& res) {
        json result;
        try {
            // 1. 解析请求体中的 JSON
            auto body = json::parse(req.body);
            std::string username = body["username"];
            std::string password = body["password"];

            // 2. 打开数据库
            sqlite3* db;
            int rc = sqlite3_open("game.db", &db);
            if (rc != SQLITE_OK) {
                result["success"] = false;
                result["error"] = "Database open failed";
                res.set_content(result.dump(), "application/json");
                return;
            }

            // 3. 执行 INSERT（注意：存在 SQL 注入风险，仅用于演示）
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

    std::cout << "Server running at http://localhost:8080" << std::endl;
    svr.listen("localhost", 8080);

    return 0;
}