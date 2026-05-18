#include "GuessGame.h"
#include <random>
#include <string>

GuessGame::GuessGame() : target(0), currentScore(100), finished(false) {}

void GuessGame::init() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 100);
    target = dis(gen);
    currentScore = 100;
    finished = false;
}

std::tuple<std::string, bool, int> GuessGame::guess(int number) {
    if (finished) return {"game_over", true, 0};
    if (number == target) {
        finished = true;
        return {"win", true, currentScore};
    } else {
        // 猜错扣5分，不低于0
        currentScore -= 5;
        if (currentScore < 0) currentScore = 0;
        // 游戏不结束，可以继续猜
        std::string result = (number > target) ? "high" : "low";
        return {result, false, 0};
    }
}

int GuessGame::getRemaining() const {
    // 无限制，返回一个很大的数或 -1，前端可忽略
    return -1;
}

bool GuessGame::isFinished() const {
    return finished;
}

int GuessGame::getCurrentScore() const {
    return currentScore;
}