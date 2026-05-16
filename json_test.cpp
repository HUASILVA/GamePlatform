#include <iostream>
#include "include/nlohmann/json.hpp"

using json = nlohmann::json;

int main() {
    json j;
    j["name"] = "Alice";
    j["score"] = 100;
    j["items"] = {"apple", "banana"};

    std::cout << j.dump(4) << std::endl;  // 4 表示缩进空格数
    return 0;
}