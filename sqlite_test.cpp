#include <iostream>
#include "include/sqlite3/sqlite3.h"   // 路径根据你放置的位置

int main() {
    sqlite3* db;
    int rc = sqlite3_open("game.db", &db);
    if (rc) {
        std::cerr << "无法打开数据库: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }
    std::cout << "数据库打开成功" << std::endl;

    // 创建 users 表
    const char* create_sql = "CREATE TABLE IF NOT EXISTS users ("
                             "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                             "username TEXT UNIQUE, "
                             "password TEXT);";
    char* errMsg = nullptr;
    rc = sqlite3_exec(db, create_sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "建表失败: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    } else {
        std::cout << "表 users 已准备就绪" << std::endl;
    }

    // 插入一条测试数据
    const char* insert_sql = "INSERT INTO users (username, password) VALUES ('testuser', '123456');";
    rc = sqlite3_exec(db, insert_sql, nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "插入失败: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    } else {
        std::cout << "插入测试用户成功" << std::endl;
    }

    // 查询并打印所有用户
    const char* select_sql = "SELECT id, username FROM users;";
    auto callback = [](void* data, int argc, char** argv, char** colName) -> int {
        for (int i = 0; i < argc; i++) {
            std::cout << colName[i] << " = " << (argv[i] ? argv[i] : "NULL") << std::endl;
        }
        std::cout << "---" << std::endl;
        return 0;
    };
    rc = sqlite3_exec(db, select_sql, callback, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "查询失败: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }

    sqlite3_close(db);
    return 0;
}