#include "CBDGame.h"
#include <cstdlib>
#include <ctime>
#include <sstream>
#include <algorithm>

CBDGame::CBDGame() {
    attacks = {
        {"小波", 1, 1}, {"波", 2, 2}, {"龙拳", 3, 3}, {"能量柱", 4, 4},
        {"闪电五连鞭", 4, 2}, {"十字劈", 5, 5}, {"直射", 8, 8},
        {"叉字劈", 7, 7}, {"如来佛掌", 10, 10}, {"奥特曼激光", 20, 11}
    };
    defenses = {
        {"L型防御", {"小波", "波", "龙拳"}, 4},
        {"屏障型防御", {"小波", "波", "能量柱", "如来佛掌"}, -1},
        {"平行防御", {"龙拳"}, -1},
        {"x型防御", {"直射"}, -1},
        {"正叉字型防御", {"十字劈"}, -1},
        {"倒叉字型防御", {"叉字劈"}, -1}
    };
    std::srand(static_cast<unsigned>(std::time(nullptr)));
}

void CBDGame::init() {
    human = {2, 0, ACTION_GAIN, -1, 1, -1, false, 0, 0, false};
    ai = {2, 0, ACTION_GAIN, -1, 1, -1, false, 0, 0, false};
    gameOver = false;
    winner = 0;
    lastResult = "";
}

bool CBDGame::setPlayerAction(ActionType type, int attackId, int multiplier, int defenseId) {
    if (gameOver) return false;
    int cost = 0;
    if (type == ACTION_ATTACK) {
        if (attackId < 0 || attackId >= (int)attacks.size()) return false;
        if (multiplier < 1 || multiplier > 5) return false;
        cost = attacks[attackId].cost * multiplier;
    } else if (type == ACTION_BLACKHOLE) {
        cost = 2;
        if (human.hasBlackhole) return false; // 已有存储黑洞不能再用
    } else if (type == ACTION_WHITEHOLE) {
        cost = 4;
    } else if (type == ACTION_GOLDEN) {
        cost = 1;
        if (human.goldenUsed) return false;   // 一局一次
    } else if (type == ACTION_BOMB) {
        cost = 5;
    } else if (type == ACTION_DEFENSE) {
        cost = 0;
        // 防御不需要额外检查
    } else if (type == ACTION_GAIN) {
        cost = 0;
    } else {
        return false;   // 不认识的动作类型
    }
    if (cost > human.energy) return false;
    human.action = type;
    human.attackId = attackId;
    human.multiplier = multiplier;
    human.defenseId = defenseId;
    return true;
}

void CBDGame::setAIAction() {
    if (gameOver) return;
    // 简单AI：先考虑特殊动作（权重低，暂不实现智能，只偶尔使用）
    // 为了测试，让AI有概率使用特殊动作（可注释）
    // 这里保持AI只用基础动作，确保阶段三功能可手动测试
    std::vector<int> affordable;
    for (size_t i = 0; i < attacks.size(); ++i) {
        if (attacks[i].cost <= ai.energy) affordable.push_back(i);
    }
    if (!affordable.empty()) {
        int idx = std::rand() % affordable.size();
        int attackId = affordable[idx];
        int mult = 1 + (std::rand() % 3);
        if (attacks[attackId].cost * mult <= ai.energy) {
            ai.action = ACTION_ATTACK;
            ai.attackId = attackId;
            ai.multiplier = mult;
            return;
        }
    }
    ai.action = ACTION_GAIN;
    ai.attackId = -1;
    ai.multiplier = 1;
}

int CBDGame::getAttackTotalDamage(int attackId, int multiplier) const {
    return attacks[attackId].damage * multiplier;
}

bool CBDGame::canDefend(const Defense& def, const Attack& atk, int totalDamage) const {
    bool nameMatch = false;
    for (const auto& name : def.canDefend) {
        if (name == atk.name) { nameMatch = true; break; }
    }
    if (!nameMatch) return false;
    if (def.maxDefendDamage != -1 && totalDamage > def.maxDefendDamage) return false;
    return true;
}

bool CBDGame::isAttackDefended(int attackerId, int defenderId, int totalDamage) const {
    const Player& defender = (defenderId == 0) ? human : ai;
    if (defender.action != ACTION_DEFENSE) return false;
    int defId = defender.defenseId;
    const Attack& atk = (attackerId == 0) ? attacks[human.attackId] : attacks[ai.attackId];
    const Defense& def = defenses[defId];
    return canDefend(def, atk, totalDamage);
}

void CBDGame::applyDamage(int target, int damage) {
    if (target == 0) {
        human.hp -= damage;
        if (human.hp < 0) human.hp = 0;
    } else {
        ai.hp -= damage;
        if (ai.hp < 0) ai.hp = 0;
    }
}

void CBDGame::kill(int target) {
    if (target == 0) human.hp = 0;
    else ai.hp = 0;
}

int CBDGame::resolveTurn() {
    if (gameOver) return winner;

    std::stringstream ss;

    // 提前声明所有变量和 lambda，避免 goto 跨过初始化
    bool humanAttackAbsorbed = false;
    bool aiAttackAbsorbed = false;
    bool bombHandled = false;

    int humanEnergyBefore = human.energy;
    int aiEnergyBefore = ai.energy;

    auto isLaser = [&](int atkId) -> bool {
        return attacks[atkId].name == "奥特曼激光";
    };

    // 扣除费用（攻击、黑洞、白洞、金身、爆破）
    auto deductCost = [&](Player& p) {
        int cost = 0;
        if (p.action == ACTION_ATTACK) {
            cost = attacks[p.attackId].cost * p.multiplier;
        } else if (p.action == ACTION_BLACKHOLE) {
            cost = 2;
        } else if (p.action == ACTION_WHITEHOLE) {
            cost = 4;
        } else if (p.action == ACTION_GOLDEN) {
            cost = 1;
        } else if (p.action == ACTION_BOMB) {
            cost = 5;
        }
        p.energy -= cost;
    };
    deductCost(human);
    deductCost(ai);

    // 金身标记
    bool humanGolden = (human.action == ACTION_GOLDEN);
    bool aiGolden = (ai.action == ACTION_GOLDEN);
    if (human.action == ACTION_ATTACK && human.attackId == 8) humanGolden = true;
    if (ai.action == ACTION_ATTACK && ai.attackId == 8) aiGolden = true;
    if (human.action == ACTION_GOLDEN) human.goldenUsed = true;
    if (ai.action == ACTION_GOLDEN) ai.goldenUsed = true;

    // 处理黑洞释放
    auto processBlackhole = [&](Player& p, bool isHuman) {
        if (p.hasBlackhole && p.storedTurn > 0) {
            p.storedTurn--;
            if (p.storedTurn == 0) {
                if (p.energy >= 1) {
                    p.energy -= 1;
                    int target = isHuman ? 1 : 0;
                    applyDamage(target, p.storedDamage);
                    ss << (isHuman ? "玩家" : "AI") << " 的黑洞释放，造成 " << p.storedDamage << " 点伤害。";
                } else {
                    ss << (isHuman ? "玩家" : "AI") << " 黑洞释放时费用不足，黑洞消失。";
                }
                p.hasBlackhole = false;
                p.storedDamage = 0;
            } else {
                ss << (isHuman ? "玩家" : "AI") << " 的黑洞还有 " << p.storedTurn << " 回合释放。";
            }
        }
    };
    processBlackhole(human, true);
    processBlackhole(ai, false);

    // 破除黑洞/白洞的标记
    bool humanAttackPierce = false;
    bool aiAttackPierce = false;

    if (human.action == ACTION_ATTACK) {
        int atkId = human.attackId;
        if (atkId == 4 || atkId == 8) {
            if (ai.action == ACTION_BLACKHOLE || ai.action == ACTION_WHITEHOLE) {
                humanAttackPierce = true;
                if (ai.action == ACTION_BLACKHOLE) {
                    ai.hasBlackhole = false;
                    ai.storedDamage = 0;
                    ai.storedTurn = 0;
                }
            }
        }
    }
    if (ai.action == ACTION_ATTACK) {
        int atkId = ai.attackId;
        if (atkId == 4 || atkId == 8) {
            if (human.action == ACTION_BLACKHOLE || human.action == ACTION_WHITEHOLE) {
                aiAttackPierce = true;
                if (human.action == ACTION_BLACKHOLE) {
                    human.hasBlackhole = false;
                    human.storedDamage = 0;
                    human.storedTurn = 0;
                }
            }
        }
    }

    // 爆破处理（优先级最高）
    if (human.action == ACTION_BOMB || ai.action == ACTION_BOMB) {
        if (human.action == ACTION_BOMB && ai.action == ACTION_BOMB) {
            ss << "双方同时爆破，互相抵消。 ";
            bombHandled = true;
        } else if (human.action == ACTION_BOMB) {
            if (ai.action == ACTION_DEFENSE || ai.action == ACTION_BLACKHOLE || ai.action == ACTION_WHITEHOLE) {
                if (!aiGolden) {
                    kill(1);
                    ss << "AI 处于防御/黑洞/白洞状态，被爆破机制杀！ ";
                } else {
                    ss << "AI 处于金身状态，爆破无效。 ";
                }
                bombHandled = true;
            } else if (ai.action == ACTION_ATTACK) {
                kill(0);
                ss << "AI 正在攻击，玩家被爆破反杀！ ";
                bombHandled = true;
            } else if (ai.action == ACTION_GAIN) {
                ss << "AI 正在获取费用，爆破无效果。 ";
                bombHandled = true;
            }
        } else if (ai.action == ACTION_BOMB) {
            if (human.action == ACTION_DEFENSE || human.action == ACTION_BLACKHOLE || human.action == ACTION_WHITEHOLE) {
                if (!humanGolden) {
                    kill(0);
                    ss << "玩家处于防御/黑洞/白洞状态，被爆破机制杀！ ";
                } else {
                    ss << "玩家处于金身状态，爆破无效。 ";
                }
                bombHandled = true;
            } else if (human.action == ACTION_ATTACK) {
                kill(1);
                ss << "玩家正在攻击，AI 被爆破反杀！ ";
                bombHandled = true;
            } else if (human.action == ACTION_GAIN) {
                ss << "玩家正在获取费用，爆破无效果。 ";
                bombHandled = true;
            }
        }
        if (human.hp <= 0 || ai.hp <= 0) {
            goto after_combat;
        }
    }

    // 吸收处理（排除破除攻击和奥特曼激光）
    // 玩家吸收AI的攻击
    if (!aiAttackPierce && (human.action == ACTION_BLACKHOLE || human.action == ACTION_WHITEHOLE) && ai.action == ACTION_ATTACK && !isLaser(ai.attackId)) {
        int dmg = getAttackTotalDamage(ai.attackId, ai.multiplier);
        if (human.action == ACTION_BLACKHOLE) {
            int limit = humanEnergyBefore * 3;
            if (dmg > limit) {
                kill(0);
                ss << "玩家黑洞吸收失败，受到过量伤害，玩家死亡！";
                goto after_combat;
            } else {
                human.hasBlackhole = true;
                human.storedDamage = dmg;
                human.storedTurn = 3;
                ss << "玩家黑洞成功吸收 " << dmg << " 点伤害，将在3回合后释放。";
                aiAttackAbsorbed = true;
            }
        } else { // 白洞
            int limit = humanEnergyBefore * 2;
            if (dmg > limit) {
                applyDamage(0, dmg);
                ss << "玩家白洞吸收失败，受到 " << dmg << " 点伤害。";
            } else {
                human.energy += dmg;
                human.energy += 4;
                ss << "玩家白洞成功吸收，获得 " << dmg << " 费用（并返还4费）。";
                aiAttackAbsorbed = true;
            }
        }
    }
    // AI吸收玩家的攻击
    if (!humanAttackPierce && (ai.action == ACTION_BLACKHOLE || ai.action == ACTION_WHITEHOLE) && human.action == ACTION_ATTACK && !isLaser(human.attackId)) {
        int dmg = getAttackTotalDamage(human.attackId, human.multiplier);
        if (ai.action == ACTION_BLACKHOLE) {
            int limit = aiEnergyBefore * 3;
            if (dmg > limit) {
                kill(1);
                ss << "AI黑洞吸收失败，AI死亡！";
                goto after_combat;
            } else {
                ai.hasBlackhole = true;
                ai.storedDamage = dmg;
                ai.storedTurn = 3;
                ss << "AI黑洞成功吸收 " << dmg << " 点伤害，将在3回合后释放。";
                humanAttackAbsorbed = true;
            }
        } else { // 白洞
            int limit = aiEnergyBefore * 2;
            if (dmg > limit) {
                applyDamage(1, dmg);
                ss << "AI白洞吸收失败，受到 " << dmg << " 点伤害。";
            } else {
                ai.energy += dmg;
                ai.energy += 4;
                ss << "AI白洞成功吸收，获得 " << dmg << " 费用（并返还4费）。";
                humanAttackAbsorbed = true;
            }
        }
    }

    // 普通攻击与防御结算（未被吸收且未被爆破终止）
    if (!bombHandled) {
        if (human.action == ACTION_ATTACK && !humanAttackAbsorbed && ai.action == ACTION_ATTACK && !aiAttackAbsorbed) {
            int dmgH = getAttackTotalDamage(human.attackId, human.multiplier);
            int dmgA = getAttackTotalDamage(ai.attackId, ai.multiplier);
            if (dmgH > dmgA) {
                applyDamage(1, dmgH - dmgA);
                ss << "玩家攻击伤害 " << dmgH << " vs AI " << dmgA << "，AI 受到 " << (dmgH - dmgA) << " 伤害。";
            } else if (dmgA > dmgH) {
                applyDamage(0, dmgA - dmgH);
                ss << "AI 攻击伤害 " << dmgA << " vs 玩家 " << dmgH << "，玩家受到 " << (dmgA - dmgH) << " 伤害。";
            } else {
                ss << "双方攻击伤害相等，互相抵消。";
            }
        } else {
            if (human.action == ACTION_ATTACK && !humanAttackAbsorbed) {
                int dmg = getAttackTotalDamage(human.attackId, human.multiplier);
                if (isLaser(human.attackId)) {
                    applyDamage(1, dmg);
                    ss << "玩家奥特曼激光造成 " << dmg << " 点伤害（无法防御）。";
                } else {
                    if (aiGolden) {
                        ss << "AI 金身免疫攻击。";
                    } else if (ai.action == ACTION_DEFENSE && isAttackDefended(0, 1, dmg)) {
                        ss << "AI 防御成功，未受伤。";
                    } else {
                        applyDamage(1, dmg);
                        ss << "玩家攻击造成 " << dmg << " 点伤害。";
                    }
                }
            }
            if (ai.action == ACTION_ATTACK && !aiAttackAbsorbed) {
                int dmg = getAttackTotalDamage(ai.attackId, ai.multiplier);
                if (isLaser(ai.attackId)) {
                    applyDamage(0, dmg);
                    ss << "AI 奥特曼激光造成 " << dmg << " 点伤害（无法防御）。";
                } else {
                    if (humanGolden) {
                        ss << "玩家金身免疫攻击。";
                    } else if (human.action == ACTION_DEFENSE && isAttackDefended(1, 0, dmg)) {
                        ss << "玩家防御成功，未受伤。";
                    } else {
                        applyDamage(0, dmg);
                        ss << "AI 攻击造成 " << dmg << " 点伤害。";
                    }
                }
            }
        }
    }

after_combat:
    // 获取费用动作
    if (human.action == ACTION_GAIN) {
        human.energy += 2;
        if (human.energy > 30) human.energy = 30;
    }
    if (ai.action == ACTION_GAIN) {
        ai.energy += 2;
        if (ai.energy > 30) ai.energy = 30;
    }

    // 检查胜负
    if (human.hp <= 0) {
        gameOver = true;
        winner = 2;
        ss << " 玩家战败，AI 获胜！";
    } else if (ai.hp <= 0) {
        gameOver = true;
        winner = 1;
        ss << " 玩家胜利！";
    }

    lastResult = ss.str();

    // 重置动作
    human.action = ACTION_GAIN;
    human.attackId = -1;
    human.defenseId = -1;
    human.multiplier = 1;
    ai.action = ACTION_GAIN;
    ai.attackId = -1;
    ai.defenseId = -1;
    ai.multiplier = 1;

    return winner;
}

int CBDGame::getPlayerHp() const { return human.hp; }
int CBDGame::getPlayerEnergy() const { return human.energy; }
int CBDGame::getAiHp() const { return ai.hp; }
int CBDGame::getAiEnergy() const { return ai.energy; }
std::string CBDGame::getLastResult() const { return lastResult; }