// 禁用 HTTPS 支持（本项目只使用 HTTP，避免 OpenSSL 依赖）
// #define CPPHTTPLIB_OPENSSL_SUPPORT

#include "CBDGame.h"
#include "GuessGame.h"          // 猜数字游戏类的头文件（声明了 GuessGame 类）
#include <unordered_map>        // 哈希表容器，用于存储 token->用户名 和 用户名->游戏实例
#include <random>               // 随机数生成（用于生成 token 和猜数字的目标数）
#include <tuple>                // std::tuple 和 std::tie，用于 guess 函数返回多个值
#include "httplib.h"            // C++ HTTP 库（Header-only），提供 Server 类和路由功能
#include "include/nlohmann/json.hpp"  // JSON 库，用于解析请求体和构造响应体
#include "include/sqlite3/sqlite3.h" // SQLite C 语言接口，用于操作数据库
#include <iostream>             // 标准输入输出，用于控制台打印日志
#include <ctime>                // 时间函数，为随机数生成提供种子
#include <regex>                //检验接口

using json = nlohmann::json;    // 简化 JSON 类型名称，后续直接使用 json

// ---------- 全局变量 ----------
// 内存中的 token 存储（服务器重启后所有 token 失效，用户需重新登录）
// 键: token 字符串（例如 "aB3dE9..."）; 值: 用户名
std::unordered_map<std::string, std::string> token_store;
// 存储每个用户的处波挡游戏实例（单人模式，玩家 vs AI）
std::unordered_map<std::string, CBDGame> cbdGames;
/**
 * 生成一个随机的 32 位字符串（包含数字、大写字母、小写字母）
 * 用于用户登录成功后颁发 token。
 * 使用静态随机数引擎和分布，保证每次调用生成的随机性。
 * @return 随机 token 字符串
 */
std::string generate_token() {
    // 可用字符集：0-9, A-Z, a-z 共 62 个字符
    static const char* chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    // 静态随机数引擎（Mersenne Twister）以当前时间作为种子初始化一次
    static std::mt19937 rng(std::time(nullptr));
    // 静态均匀分布，生成 [0, 61] 的整数
    static std::uniform_int_distribution<int> dist(0, 61);
    std::string token;
    for (int i = 0; i < 32; ++i) {
        token += chars[dist(rng)];   // 随机选择一个字符并追加
    }
    return token;
}

/**
 * 检查字符串是否只包含英文字母（大小写）和数字
 * @param str 待检查的字符串
 * @return true 如果只包含 A-Z a-z 0-9，否则 false
 */
bool isAlphanumeric(const std::string& str) {
    static std::regex pattern("^[A-Za-z0-9]+$");
    return std::regex_match(str, pattern);
}
/**
 * 初始化数据库：打开 game.db，创建 users 表和 scores 表（如果不存在）。
 * 该函数在服务器启动时调用一次。
 * 注意：SQLite 是文件型数据库，game.db 文件会在当前工作目录下生成。
 */
void init_db() {
    sqlite3* db;
    // 打开数据库文件，如果文件不存在则自动创建
    if (sqlite3_open("game.db", &db) != SQLITE_OK) {
        std::cerr << "Failed to open database, registration unavailable" << std::endl;
        return;
    }
    // ---------- 创建 users 表 ----------
    // 字段说明：
    // id: 自增主键，唯一标识用户
    // username: 用户名，唯一，不能重复
    // password: 密码（当前明文存储，后续可改为哈希）
    const char* sql = "CREATE TABLE IF NOT EXISTS users ("
                      "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                      "username TEXT UNIQUE, "
                      "password TEXT);";
    char* errMsg = nullptr;
    sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
    if (errMsg) {
        std::cerr << "creat table is false (users): " << errMsg << std::endl;
        sqlite3_free(errMsg);
    } else {
        std::cout << "users table is ready" << std::endl;
    }
    // ---------- 创建 scores 表（用于存储游戏最高分）----------
    // 字段说明：
    // id: 自增主键
    // user_id: 关联 users 表的 id
    // game_type: 游戏类型，如 "guess"、"pushbox"
    // best_score: 该用户在此游戏中的历史最高分
    // updated_at: 最后一次更新分数的时间戳（自动记录）
    const char* sql_scores = "CREATE TABLE IF NOT EXISTS scores ("
                             "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                             "user_id INTEGER, "
                             "game_type TEXT, "
                             "best_score INTEGER, "
                             "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP);";
    errMsg = nullptr;
    int rc = sqlite3_exec(db, sql_scores, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "creat table is false(scores): " << errMsg << std::endl;
        sqlite3_free(errMsg);
    } else {
        std::cout << "scores table is ready" << std::endl;
    }

    sqlite3_close(db);  // 关闭数据库连接
}

// 全局变量：存储每个用户的猜数字游戏实例
// 键：用户名（string）；值：GuessGame 对象
// 这样每个登录用户都有自己独立的游戏状态，互不干扰
std::unordered_map<std::string, GuessGame> guessGames;

/**
 * 从 HTTP 请求中提取 Bearer Token，并验证其有效性，返回对应的用户名。
 * @param req HTTP 请求对象（包含请求头）
 * @return 用户名（如果 token 无效或未提供，返回空字符串）
 */
std::string getUserFromToken(const httplib::Request& req) {
    // 获取 Authorization 请求头的值，格式如 "Bearer xxxxxxx"
    std::string auth = req.get_header_value("Authorization");
    // 检查前缀是否为 "Bearer "，如果不是则返回空
    if (auth.substr(0, 7) != "Bearer ") return "";
    // 截取 token 部分（从第7个字符开始）
    std::string token = auth.substr(7);
    // 在 token_store 中查找该 token
    auto it = token_store.find(token);
    if (it == token_store.end()) return "";  // 未找到
    return it->second;  // 返回对应的用户名
}

/**
 * 内部函数：提交用户游戏分数（如果比历史最高分高则更新）
 * @param username 用户名
 * @param game_type 游戏类型（例如 "guess"）
 * @param score 本次得分
 * @return true 表示成功（或新纪录），false 表示失败
 */
bool submit_score(const std::string& username, const std::string& game_type, int score) {
    sqlite3* db;
    if (sqlite3_open("game.db", &db) != SQLITE_OK) {
        std::cerr << "submit_score: can not open data table" << std::endl;
        return false;
    }

    // 获取用户 id
    int user_id = -1;
    std::string id_sql = "SELECT id FROM users WHERE username = '" + username + "';";
    auto id_callback = [](void* data, int argc, char** argv, char** colName) -> int {
        if (argc > 0 && argv[0]) *((int*)data) = atoi(argv[0]);
        return 0;
    };
    sqlite3_exec(db, id_sql.c_str(), id_callback, &user_id, nullptr);
    if (user_id == -1) {
        sqlite3_close(db);
        return false;
    }

    // 查询当前最高分
    std::string select_sql = "SELECT best_score FROM scores WHERE user_id = " + std::to_string(user_id) +
                             " AND game_type = '" + game_type + "';";
    int current_best = -1;
    auto select_callback = [](void* data, int argc, char** argv, char** colName) -> int {
        if (argc > 0 && argv[0]) *((int*)data) = atoi(argv[0]);
        return 0;
    };
    sqlite3_exec(db, select_sql.c_str(), select_callback, &current_best, nullptr);

    bool success = false;
    if (current_best == -1 || score > current_best) {
        std::string upsert_sql;
        if (current_best == -1) {
            upsert_sql = "INSERT INTO scores (user_id, game_type, best_score) VALUES (" +
                         std::to_string(user_id) + ", '" + game_type + "', " + std::to_string(score) + ");";
        } else {
            upsert_sql = "UPDATE scores SET best_score = " + std::to_string(score) +
                         ", updated_at = CURRENT_TIMESTAMP WHERE user_id = " + std::to_string(user_id) +
                         " AND game_type = '" + game_type + "';";
        }
        char* errMsg = nullptr;
        int rc = sqlite3_exec(db, upsert_sql.c_str(), nullptr, nullptr, &errMsg);
        if (rc == SQLITE_OK) success = true;
        else {
            std::cerr << "submit_score false: " << errMsg << std::endl;
            sqlite3_free(errMsg);
        }
    } else {
        success = true; // 未破纪录，但操作成功
    }
    sqlite3_close(db);
    return success;
}


/**
 * 根据用户名获取用户ID
 * @param username 用户名
 * @return 用户ID，如果用户不存在或出错返回 -1
 */
int getUserId(const std::string& username) {
    sqlite3* db;
    if (sqlite3_open("game.db", &db) != SQLITE_OK) {
        std::cerr << "getUserId: Failed to open database" << std::endl;
        return -1;
    }
    std::string sql = "SELECT id FROM users WHERE username = '" + username + "';";
    int user_id = -1;
    auto callback = [](void* data, int argc, char** argv, char** colName) -> int {
        if (argc > 0 && argv[0]) {
            *((int*)data) = atoi(argv[0]);
        }
        return 0;
    };
    sqlite3_exec(db, sql.c_str(), callback, &user_id, nullptr);
    sqlite3_close(db);
    return user_id;
}

int main() {
    // 第一步：初始化数据库（创建表）
    init_db();

    // 创建 HTTP 服务器对象
    httplib::Server svr;

    // ========== 基础路由（无需认证） ==========

    // 根路径：返回简单的欢迎文字，用于测试服务器是否正常运行
    svr.Get("/", [](const httplib::Request& req, httplib::Response& res) {
        res.set_content("Hello from C++ Game Platform!", "text/plain");
    });

    // 状态接口：返回服务器状态和支持的游戏列表
    svr.Get("/api/status", [](const httplib::Request& req, httplib::Response& res) {
        json j;
        j["status"] = "ok";
        j["players"] = 0;                 // 当前在线玩家数（暂未实现统计功能）
        j["games"] = {"pushbox", "guess"}; // 游戏列表
        res.set_content(j.dump(), "application/json");
    });

    // ========== 用户注册接口 ==========
    // 方法：POST
    // 请求体 JSON 格式：{"username": "xxx", "password": "xxx"}
    // 响应 JSON：{"success": true/false, "error": "..."}
    svr.Post("/api/register", [](const httplib::Request& req, httplib::Response& res) {
        json result;
        try {
            // 解析请求体中的 JSON
            auto body = json::parse(req.body);
            std::string username = body["username"];
            std::string password = body["password"];

            //名字合法性检测
            if (!isAlphanumeric(username) || !isAlphanumeric(password)) {
                result["success"] = false;
                result["error"] = "Username and password must contain only letters and numbers";
                res.set_content(result.dump(), "application/json");
                return;
            }

             // 2. 长度检查（例如用户名3-20字符，密码6-20字符）
            if (username.length() < 3 || username.length() > 20) {
                result["success"] = false;
                result["error"] = "Username must be 3-20 characters";
                res.set_content(result.dump(), "application/json");
                return;
            }
            if (password.length() < 6 || password.length() > 20) {
                result["success"] = false;
                result["error"] = "Password must be 6-20 characters";
                res.set_content(result.dump(), "application/json");
                return;
            }

            // 打开数据库
            sqlite3* db;
            int rc = sqlite3_open("game.db", &db);
            if (rc != SQLITE_OK) {
                result["success"] = false;
                result["error"] = "Database open failed";
                res.set_content(result.dump(), "application/json");
                return;
            }

            // 构造 INSERT 语句（注意：直接拼接字符串存在 SQL 注入风险，演示项目暂可接受）
            std::string sql = "INSERT INTO users (username, password) VALUES ('" 
                              + username + "', '" + password + "');";
            char* errMsg = nullptr;
            rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
            if (rc != SQLITE_OK) {
                // 插入失败（通常是用户名重复）
                result["success"] = false;
                result["error"] = errMsg;
                sqlite3_free(errMsg);
            } else {
                result["success"] = true;
            }
            sqlite3_close(db);
        } catch (const std::exception& e) {
            // JSON 解析异常或其他标准异常
            result["success"] = false;
            result["error"] = e.what();
        }
        res.set_content(result.dump(), "application/json");
    });

    // ========== 用户登录接口 ==========
    // 方法：POST
    // 请求体 JSON：{"username": "xxx", "password": "xxx"}
    // 响应：成功时返回 {"success": true, "token": "xxxxxx"}，失败返回错误信息
    svr.Post("/api/login", [](const httplib::Request& req, httplib::Response& res) {
        json result;
        try {
            auto body = json::parse(req.body);
            std::string username = body["username"];
            std::string password = body["password"];


              // ========== 新增：字符合法性检查 ==========
            if (!isAlphanumeric(username) || !isAlphanumeric(password)) {
                result["success"] = false;
                result["error"] = "Invalid username or password format";
                res.set_content(result.dump(), "application/json");
                return;
            }
            sqlite3* db;
            int rc = sqlite3_open("game.db", &db);
            if (rc != SQLITE_OK) {
                result["success"] = false;
                result["error"] = "Database open failed";
                res.set_content(result.dump(), "application/json");
                return;
            }

            // 查询该用户的密码
            std::string sql = "SELECT password FROM users WHERE username = '" + username + "';";
            std::string stored_pw;
            // 回调函数：将查询到的密码（第一列）存入 stored_pw
            auto callback = [](void* data, int argc, char** argv, char** colName) -> int {
                if (argc > 0 && argv[0]) *((std::string*)data) = argv[0];
                return 0;
            };
            rc = sqlite3_exec(db, sql.c_str(), callback, &stored_pw, nullptr);
            sqlite3_close(db);

            if (rc == SQLITE_OK && stored_pw == password) {
                // 密码正确，生成 token 并存储
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

    // ========== 受保护接口：获取当前登录用户的信息 ==========
    // 方法：GET
    // 需要在请求头中携带 Authorization: Bearer <token>
    // 响应：{"username": "xxx"}
    svr.Get("/api/me", [](const httplib::Request& req, httplib::Response& res) {
        std::string auth = req.get_header_value("Authorization");
        std::string token;
        if (auth.substr(0, 7) == "Bearer ") {
            token = auth.substr(7);
        } else {
            // 未提供正确的 Authorization 头
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

    // ========== 猜数字游戏相关路由（需要认证） ==========

    // 开始新游戏
    // 方法：POST /game/guess/start
    // 响应：{"remaining": -1, "message": "..."}
    svr.Post("/game/guess/start", [&](const httplib::Request& req, httplib::Response& res) {
        std::string username = getUserFromToken(req);
        if (username.empty()) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        // 获取或创建该用户的游戏实例（operator[] 会自动构造默认对象）
        auto& game = guessGames[username];
        game.init();   // 初始化：随机生成目标数字，重置分数为100，游戏未结束
        json result;
        result["remaining"] = game.getRemaining();  // 本游戏不限次数，返回 -1
        result["message"] = "New game started. Guess a number between 1 and 100.";
        res.set_content(result.dump(), "application/json");
    });

    // 提交猜测
    // 方法：POST /game/guess/play
    // 请求体：{"guess": 50}
    // 响应：{"result": "high/low/win", "remaining": -1, "finished": true/false, "score": xxx（如果猜中）}
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
            // 调用 GuessGame 类的 guess 方法，返回 tuple (结果, 是否结束, 得分)
            std::tie(resultStr, finished, score) = game.guess(guessNum);
            json result;
            result["result"] = resultStr;
            result["remaining"] = game.getRemaining();
            result["finished"] = finished;
            if (finished) {
                if (resultStr == "win") {
                    result["score"] = score;
                    submit_score(username, "guess", score);
                    // TODO: 这里应调用提交分数的接口（将本次得分存入 scores 表，供排行榜使用）
                } else if (resultStr == "lose") {
                    result["score"] = 0;
                }
                // 游戏结束，从内存中删除该用户的游戏实例，释放资源
                guessGames.erase(it);
            }
            res.set_content(result.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(R"({"error":"Invalid JSON"})", "application/json");
        }
    });

    // 获取当前游戏状态（用于页面刷新时恢复游戏）
    // 方法：GET /game/guess/state
    // 响应：{"active": true/false, "remaining": -1, "finished": true/false}
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
            result["active"] = false;      // 没有进行中的游戏
        } else {
            result["active"] = true;
            result["remaining"] = it->second.getRemaining();
            result["finished"] = it->second.isFinished();
        }
        res.set_content(result.dump(), "application/json");
    });

    // ========== 用户注销账号接口 ==========
    // 方法：DELETE /api/user
    // 需要认证（带 token）
    // 删除用户及其关联的所有数据（scores 记录、token、游戏状态）
    // 响应：{"success": true, "message": "..."}
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

        // 1. 根据用户名获取用户 ID
       int user_id = getUserId(username);
       if (user_id == -1) {
            sqlite3_close(db);
            res.status = 404;
            res.set_content(R"({"error":"User not found"})", "application/json");
            return;
        }

        // 2. 开启事务（确保多个删除操作要么全部成功，要么全部回滚）
        char* errMsg = nullptr;
        sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, &errMsg);
        if (errMsg) {
            sqlite3_free(errMsg);
            sqlite3_close(db);
            res.status = 500;
            res.set_content(R"({"error":"Failed to start transaction"})", "application/json");
            return;
        }

        // 3. 删除 scores 表中该用户的所有记录（如果 scores 表已存在）
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

        // 4. 删除 users 表中的用户记录
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

        // 6. 清除内存中属于该用户的所有 token（可能有多个设备登录留下的多个 token）
        for (auto it = token_store.begin(); it != token_store.end(); ) {
            if (it->second == username) {
                it = token_store.erase(it);  // erase 返回下一个有效迭代器
            } else {
                ++it;
            }
        }

        // 7. 清除内存中的猜数字游戏实例（如果存在）
        guessGames.erase(username);

        // 8. 返回成功响应
        json result;
        result["success"] = true;
        result["message"] = "Account deleted successfully";
        res.set_content(result.dump(), "application/json");
    });


    // ========== 提交分数接口（供前端调用，也可内部调用） ==========
    svr.Post("/api/score", [&](const httplib::Request& req, httplib::Response& res) {
        std::string username = getUserFromToken(req);
        if (username.empty()) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        try {
            auto body = json::parse(req.body);
            std::string game_type = body["game"];
            int score = body["score"];
            bool ok = submit_score(username, game_type, score);
            json result;
            result["success"] = ok;
            res.set_content(result.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"Invalid JSON"})", "application/json");
        }
    });

    // ========== 获取排行榜接口 ==========
    svr.Get("/api/rank", [&](const httplib::Request& req, httplib::Response& res) {
        std::string game_type = req.get_param_value("game");
        if (game_type.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"Missing game parameter"})", "application/json");
            return;
        }

        sqlite3* db;
        if (sqlite3_open("game.db", &db) != SQLITE_OK) {
            res.status = 500;
            res.set_content(R"({"error":"Database open failed"})", "application/json");
            return;
        }

        std::string sql = "SELECT u.username, s.best_score, s.updated_at FROM scores s "
                          "JOIN users u ON s.user_id = u.id "
                          "WHERE s.game_type = '" + game_type + "' "
                          "ORDER BY s.best_score DESC LIMIT 10;";
        json result = json::array();
        auto callback = [](void* data, int argc, char** argv, char** colName) -> int {
            json* arr = (json*)data;
            if (argc >= 3) {
                json entry;
                entry["username"] = argv[0] ? argv[0] : "";
                entry["score"] = argv[1] ? std::stoi(argv[1]) : 0;
                entry["updated_at"] = argv[2] ? argv[2] : "";
                arr->push_back(entry);
            }
            return 0;
        };
        char* errMsg = nullptr;
        int rc = sqlite3_exec(db, sql.c_str(), callback, &result, &errMsg);
        if (rc != SQLITE_OK) {
            res.status = 500;
            res.set_content(R"({"error":"Query failed"})", "application/json");
            sqlite3_free(errMsg);
            sqlite3_close(db);
            return;
        }
        sqlite3_close(db);
        res.set_content(result.dump(), "application/json");
    });


    // ========== 处波挡游戏（单人 vs AI） ==========
    // 开始新游戏
    svr.Post("/game/cbd/start", [&](const httplib::Request& req, httplib::Response& res) {
        std::string username = getUserFromToken(req);
        if (username.empty()) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        auto& game = cbdGames[username];
        game.init();
        json result;
        result["message"] = "Game started. Your HP:2, Energy:0. AI HP:2, Energy:0";
        res.set_content(result.dump(), "application/json");
    });

    // 玩家提交动作
    svr.Post("/game/cbd/action", [&](const httplib::Request& req, httplib::Response& res) {
    std::string username = getUserFromToken(req);
    if (username.empty()) {
        res.status = 401;
        res.set_content(R"({"error":"Unauthorized"})", "application/json");
        return;
    }
    auto it = cbdGames.find(username);
    if (it == cbdGames.end()) {
        res.status = 400;
        res.set_content(R"({"error":"No active game, please start first"})", "application/json");
        return;
    }
    auto& game = it->second;
    try {
        auto body = json::parse(req.body);
        int actionType = body["action"]; // 0:获取费用, 1:攻击, 2:防御, 3:黑洞, 4:白洞, 5:金身, 6:爆破
        bool ok = false;
        if (actionType == 0) {
            ok = game.setPlayerAction(CBDGame::ACTION_GAIN);
        } else if (actionType == 1) {
            int attackId = body["attackId"];
            int multiplier = body.value("multiplier", 1);
            ok = game.setPlayerAction(CBDGame::ACTION_ATTACK, attackId, multiplier);
        } else if (actionType == 2) {
            int defenseId = body["defenseId"];
            ok = game.setPlayerAction(CBDGame::ACTION_DEFENSE, -1, 1, defenseId);
        } else if (actionType == 3) {
            ok = game.setPlayerAction(CBDGame::ACTION_BLACKHOLE);
        } else if (actionType == 4) {
            ok = game.setPlayerAction(CBDGame::ACTION_WHITEHOLE);
        } else if (actionType == 5) {
            ok = game.setPlayerAction(CBDGame::ACTION_GOLDEN);
        } else if (actionType == 6) {
            ok = game.setPlayerAction(CBDGame::ACTION_BOMB);
        } else {
            res.status = 400;
            res.set_content(R"({"error":"Invalid action type"})", "application/json");
            return;
        }
        if (!ok) {
            res.status = 400;
            res.set_content("{\"error\":\"Invalid action (cost too high or invalid parameters)\"}", "application/json");
            return;
        }
        game.setAIAction();
        int winner = game.resolveTurn();
        json result;
        result["player_hp"] = game.getPlayerHp();
        result["player_energy"] = game.getPlayerEnergy();
        result["ai_hp"] = game.getAiHp();
        result["ai_energy"] = game.getAiEnergy();
        result["last_result"] = game.getLastResult();
        result["game_over"] = (winner != 0);
        result["winner"] = winner;
        res.set_content(result.dump(), "application/json");
    } catch (const std::exception& e) {
        res.status = 400;
        json err;
        err["error"] = "Invalid JSON or action";
        res.set_content(err.dump(), "application/json");
    }
});

    // 获取当前游戏状态
    svr.Get("/game/cbd/state", [&](const httplib::Request& req, httplib::Response& res) {
        std::string username = getUserFromToken(req);
        if (username.empty()) {
            res.status = 401;
            res.set_content(R"({"error":"Unauthorized"})", "application/json");
            return;
        }
        auto it = cbdGames.find(username);
        if (it == cbdGames.end()) {
            res.status = 404;
            res.set_content(R"({"error":"No active game"})", "application/json");
            return;
        }
        auto& game = it->second;
        json result;
        result["player_hp"] = game.getPlayerHp();
        result["player_energy"] = game.getPlayerEnergy();
        result["ai_hp"] = game.getAiHp();
        result["ai_energy"] = game.getAiEnergy();
        result["last_result"] = game.getLastResult();
        res.set_content(result.dump(), "application/json");
    });


    

    // ========== 静态文件挂载（提供前端页面、样式、JavaScript 文件） ==========
    // 将 URL 路径映射到本地文件夹，使浏览器可以获取静态资源
    svr.set_mount_point("/", "./www/html");   // 访问 / 时，去 ./www/html 下找文件（如 login.html）
    svr.set_mount_point("/css", "./www/css"); // 访问 /css/xxx.css 时，去 ./www/css 找文件
    svr.set_mount_point("/js", "./www/js");   // 访问 /js/xxx.js 时，去 ./www/js 找文件

    // 启动 HTTP 服务器，监听本机的 8080 端口
    // 注意：listen 函数会阻塞当前线程，直到服务器停止
    svr.listen("localhost", 8080);
    
    return 0;
}