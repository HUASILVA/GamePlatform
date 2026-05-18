// #define CPPHTTPLIB_OPENSSL_SUPPORT
#include "GuessGame.h"          // 猜数字游戏类
#include <unordered_map>        // 用于存储用户游戏实例
#include <random>               // 随机数生成（虽然 GuessGame 里已经有了，但为了其他部分也可能需要）
#include <tuple>                // 用于 guess 返回的 tuple
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

        // 创建 scores 表（用于存储用户游戏最高分）
    const char* sql_scores = "CREATE TABLE IF NOT EXISTS scores ("
                             "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                             "user_id INTEGER, "
                             "game_type TEXT, "
                             "best_score INTEGER, "
                             "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP);";
    errMsg = nullptr;   // 重新使用 errMsg 变量（之前已经释放或为空）
    int rc = sqlite3_exec(db, sql_scores, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "建表错误(scores): " << errMsg << std::endl;
        sqlite3_free(errMsg);
    } else {
        std::cout << "scores 表已准备就绪" << std::endl;
    }

    sqlite3_close(db);
}


// 存储每个用户的猜数字游戏实例（key = 用户名）
std::unordered_map<std::string, GuessGame> guessGames;

// 从请求头中提取 token，并返回对应的用户名（如果 token 无效则返回空字符串）
std::string getUserFromToken(const httplib::Request& req) {
    std::string auth = req.get_header_value("Authorization");
    if (auth.substr(0, 7) != "Bearer ") return "";
    std::string token = auth.substr(7);
    auto it = token_store.find(token);   // token_store 是已有的 token -> username 映射
    if (it == token_store.end()) return "";
    return it->second;
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
        // ========== 猜数字游戏路由 ==========

    // 开始新游戏
    svr.Post("/game/guess/start", [&](const httplib::Request& req, httplib::Response& res) {
        std::string username = getUserFromToken(req);
        if (username.empty()) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        auto& game = guessGames[username];
        game.init();
        json result;
        result["remaining"] = game.getRemaining();
        result["message"] = "New game started. Guess a number between 1 and 100.";
        res.set_content(result.dump(), "application/json");
    });

    // 提交猜测
    svr.Post("/game/guess/play", [&](const httplib::Request& req, httplib::Response& res) {
        std::string username = getUserFromToken(req);
        if (username.empty()) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        auto it = guessGames.find(username);
        if (it == guessGames.end()) {
            res.status = 400;
            res.set_content(R"({"error":"No active game, please start first"})", "application/json");
            return;
        }
        auto& game = it->second;
        try {
            auto body = json::parse(req.body);
            int guessNum = body["guess"];
            std::string resultStr;
            bool finished;
            int score;
            std::tie(resultStr, finished, score) = game.guess(guessNum);
            json result;
            result["result"] = resultStr;
            result["remaining"] = game.getRemaining();
            result["finished"] = finished;
            if (finished) {
                if (resultStr == "win") {
                    result["score"] = score;
                    // TODO: 后续调用积分接口
                } else if (resultStr == "lose") {
                    result["score"] = 0;
                }
                guessGames.erase(it);
            }
            res.set_content(result.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(R"({"error":"Invalid JSON"})", "application/json");
        }
    });

    // 获取当前游戏状态
    svr.Get("/game/guess/state", [&](const httplib::Request& req, httplib::Response& res) {
        std::string username = getUserFromToken(req);
        if (username.empty()) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        auto it = guessGames.find(username);
        json result;
        if (it == guessGames.end()) {
            result["active"] = false;
        } else {
            result["active"] = true;
            result["remaining"] = it->second.getRemaining();
            result["finished"] = it->second.isFinished();
        }
        res.set_content(result.dump(), "application/json");
    });


        // ========== 用户注销接口 ==========
    svr.Delete("/api/user", [&](const httplib::Request& req, httplib::Response& res) {
        std::string username = getUserFromToken(req);
        if (username.empty()) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }

        sqlite3* db;
        int rc = sqlite3_open("game.db", &db);
        if (rc != SQLITE_OK) {
            res.status = 500;
            res.set_content(R"({"error":"Database open failed"})", "application/json");
            return;
        }

        // 1. 获取用户 id
        int user_id = -1;
        std::string id_sql = "SELECT id FROM users WHERE username = '" + username + "';";
        auto id_callback = [](void* data, int argc, char** argv, char** colName) -> int {
            if (argc > 0 && argv[0]) *((int*)data) = atoi(argv[0]);
            return 0;
        };
        sqlite3_exec(db, id_sql.c_str(), id_callback, &user_id, nullptr);
        if (user_id == -1) {
            sqlite3_close(db);
            res.status = 404;
            res.set_content(R"({"error":"User not found"})", "application/json");
            return;
        }

        // 2. 开启事务
        char* errMsg = nullptr;
        sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg);
        if (errMsg) {
            sqlite3_free(errMsg);
            sqlite3_close(db);
            res.status = 500;
            res.set_content(R"({"error":"Failed to start transaction"})", "application/json");
            return;
        }

        // 3. 删除 scores 表中的记录（如果表存在）
        std::string del_scores = "DELETE FROM scores WHERE user_id = " + std::to_string(user_id) + ";";
        rc = sqlite3_exec(db, del_scores.c_str(), nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            sqlite3_close(db);
            res.status = 500;
            res.set_content(R"({"error":"Failed to delete scores"})", "application/json");
            sqlite3_free(errMsg);
            return;
        }

        // 4. 删除 users 表中的记录
        std::string del_user = "DELETE FROM users WHERE id = " + std::to_string(user_id) + ";";
        rc = sqlite3_exec(db, del_user.c_str(), nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            sqlite3_close(db);
            res.status = 500;
            res.set_content(R"({"error":"Failed to delete user"})", "application/json");
            sqlite3_free(errMsg);
            return;
        }

        // 5. 提交事务
        rc = sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errMsg);
        if (rc != SQLITE_OK) {
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            sqlite3_close(db);
            res.status = 500;
            res.set_content(R"({"error":"Failed to commit transaction"})", "application/json");
            sqlite3_free(errMsg);
            return;
        }
        sqlite3_close(db);

        // 6. 清除内存中的 token（删除所有属于该用户的 token）
        for (auto it = token_store.begin(); it != token_store.end(); ) {
            if (it->second == username) {
                it = token_store.erase(it);
            } else {
                ++it;
            }
        }

        // 7. 清除内存中的游戏状态
        guessGames.erase(username);

        // 8. 返回成功
        json result;
        result["success"] = true;
        result["message"] = "Account deleted successfully";
        res.set_content(result.dump(), "application/json");
    });



    svr.set_mount_point("/", "./www/html");
    svr.set_mount_point("/css", "./www/css");    // 新增
    svr.set_mount_point("/js", "./www/js");      // 新增



    svr.listen("localhost", 8080);

    return 0;
}