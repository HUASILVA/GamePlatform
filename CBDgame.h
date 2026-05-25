#ifndef CBD_GAME_H
#define CBD_GAME_H

#include <string>
#include <vector>

struct Attack {
    std::string name;
    int cost;
    int damage;
};

struct Defense {
    std::string name;
    std::vector<std::string> canDefend;
    int maxDefendDamage; // -1 表示无限
};

class CBDGame {
public:
    CBDGame();
    void init();

    enum ActionType {
        ACTION_GAIN,
        ACTION_ATTACK,
        ACTION_DEFENSE,
        ACTION_BLACKHOLE,
        ACTION_WHITEHOLE,
        ACTION_GOLDEN,   // 新增
        ACTION_BOMB      // 新增
    };

    bool setPlayerAction(ActionType type, int attackId = -1, int multiplier = 1, int defenseId = -1);
    void setAIAction();
    int resolveTurn();

    int getPlayerHp() const;
    int getPlayerEnergy() const;
    int getAiHp() const;
    int getAiEnergy() const;
    std::string getLastResult() const;

private:
    struct Player {
        int hp;
        int energy;
        ActionType action;
        int attackId;
        int multiplier;
        int defenseId;
        // 黑洞状态
        bool hasBlackhole;
        int storedDamage;
        int storedTurn;
        // 金身
        bool goldenUsed;     // 是否已使用过金身（一局一次）
    };
    Player human;
    Player ai;
    std::string lastResult;
    bool gameOver;
    int winner; // 1:人类, 2:AI

    std::vector<Attack> attacks;
    std::vector<Defense> defenses;

    int getAttackTotalDamage(int attackId, int multiplier) const;
    bool canDefend(const Defense& def, const Attack& atk, int totalDamage) const;
    bool isAttackDefended(int attackerId, int defenderId, int totalDamage) const;
    void applyDamage(int target, int damage);
    void kill(int target);
};

#endif