#ifndef GUESS_GAME_H
#define GUESS_GAME_H

#include <tuple>
#include <string>

class GuessGame {
public:
    GuessGame();
    void init();   // 重置游戏，分数重置为100，重新生成随机数
    std::tuple<std::string, bool, int> guess(int number);
    int getRemaining() const;  // 可返回剩余次数，但无限制时总是返回一个大数，或忽略
    bool isFinished() const;
    int getCurrentScore() const;
private:
    int target;
    int currentScore;   // 当前分数
    bool finished;
};

#endif