const API_BASE = 'http://localhost:8080';
const token = localStorage.getItem('token');
if (!token) {
    window.location.href = 'login.html';
}

const attacks = [
    "小波", "波", "龙拳", "能量柱", "闪电五连鞭",
    "十字劈", "直射", "叉字劈", "如来佛掌", "奥特曼激光"
];
const defenses = [
    "L型防御", "屏障型防御", "平行防御", "x型防御",
    "正叉字型防御", "倒叉字型防御"
];

let currentAction = null;
let countdownInterval = null;

function stopCountdown() {
    if (countdownInterval) {
        clearInterval(countdownInterval);
        countdownInterval = null;
    }
}

function startCountdown(seconds, onTimeout) {
    stopCountdown();
    let remaining = seconds;
    const timerText = document.getElementById('timerText');
    const timerProgress = document.getElementById('timerProgress');
    if (timerProgress) {
        timerProgress.max = seconds;
        timerProgress.value = seconds;
    }
    if (timerText) timerText.innerText = `倒计时: ${remaining}s`;
    countdownInterval = setInterval(() => {
        remaining--;
        if (timerText) timerText.innerText = `倒计时: ${remaining}s`;
        if (timerProgress) timerProgress.value = remaining;
        if (remaining <= 0) {
            stopCountdown();
            if (onTimeout) onTimeout();
        }
    }, 1000);
}

async function apiCall(method, url, body = null) {
    const options = {
        method: method,
        headers: {
            'Authorization': `Bearer ${token}`,
            'Content-Type': 'application/json'
        }
    };
    if (body) options.body = JSON.stringify(body);
    const response = await fetch(API_BASE + url, options);
    if (response.status === 401) {
        localStorage.removeItem('token');
        window.location.href = 'login.html';
        throw new Error('Unauthorized');
    }
    if (!response.ok) {
        const text = await response.text();
        throw new Error(text);
    }
    return await response.json();
}

function splitResult(fullText) {
    let playerPart = "";
    let aiActionPart = "";
    if (!fullText) return { playerPart, aiActionPart };

    const aiIndex = fullText.search(/AI:/);
    if (aiIndex === -1) {
        playerPart = fullText;
        aiActionPart = "（无动作记录）";
        return { playerPart, aiActionPart };
    }

    playerPart = fullText.substring(0, aiIndex).trim();
    let afterAI = fullText.substring(aiIndex + 3).trim();
    let endIndex = afterAI.search(/[。！？]/);
    if (endIndex !== -1) {
        aiActionPart = afterAI.substring(0, endIndex + 1).trim();
        let rest = afterAI.substring(endIndex + 1).trim();
        if (rest) playerPart += " " + rest;
    } else {
        aiActionPart = afterAI;
    }

    if (!playerPart) playerPart = "（无玩家事件）";
    if (!aiActionPart) aiActionPart = "（无AI动作）";

    return { playerPart, aiActionPart };
}

async function startGame() {
    try {
        await apiCall('POST', '/game/cbd/start');
        await updateState();
        document.getElementById('playerEvents').innerHTML = '游戏已开始，选择你的动作！';
        document.getElementById('aiEvents').innerHTML = '';
        enableButtons(true);
        startCountdown(5, () => {
            performAction({ action: 0 });
        });
    } catch (e) {
        console.error(e);
        document.getElementById('playerEvents').innerHTML = '开始游戏失败: ' + e.message;
        document.getElementById('aiEvents').innerHTML = '';
    }
}

function enableButtons(enable) {
    const btns = ['gainBtn', 'attackBtn', 'defenseBtn', 'blackholeBtn', 'whiteholeBtn', 'goldenBtn', 'bombBtn', 'releaseBlackholeBtn'];
    btns.forEach(id => {
        const btn = document.getElementById(id);
        if (btn) btn.disabled = !enable;
    });
}

async function updateState() {
    try {
        const state = await apiCall('GET', '/game/cbd/state');
        document.getElementById('playerHp').innerText = state.player_hp;
        document.getElementById('playerEnergy').innerText = state.player_energy;
        document.getElementById('aiHp').innerText = state.ai_hp;
        document.getElementById('aiEnergy').innerText = state.ai_energy;
        if (state.last_result) {
            const { playerPart, aiActionPart } = splitResult(state.last_result);
            document.getElementById('playerEvents').innerHTML = playerPart;
            document.getElementById('aiEvents').innerHTML = aiActionPart;
        }
        if (state.player_hp <= 0 || state.ai_hp <= 0) {
            enableButtons(false);
            stopCountdown();
        } else {
            if (!countdownInterval) {
                startCountdown(5, () => {
                    performAction({ action: 0 });
                });
            }
        }
    } catch (e) {
        console.error(e);
    }
}

async function performAction(actionData) {
    stopCountdown();
    try {
        const result = await apiCall('POST', '/game/cbd/action', actionData);
        document.getElementById('playerHp').innerText = result.player_hp;
        document.getElementById('playerEnergy').innerText = result.player_energy;
        document.getElementById('aiHp').innerText = result.ai_hp;
        document.getElementById('aiEnergy').innerText = result.ai_energy;
        if (result.last_result) {
            const { playerPart, aiActionPart } = splitResult(result.last_result);
            document.getElementById('playerEvents').innerHTML = playerPart;
            document.getElementById('aiEvents').innerHTML = aiActionPart;
        }
        if (result.game_over) {
            enableButtons(false);
            stopCountdown();
        } else {
            startCountdown(5, () => {
                performAction({ action: 0 });
            });
        }
        document.getElementById('attackOptions').style.display = 'none';
        document.getElementById('defenseOptions').style.display = 'none';
        currentAction = null;
    } catch (e) {
        console.error(e);
        document.getElementById('playerEvents').innerHTML = '动作提交失败: ' + e.message;
        document.getElementById('aiEvents').innerHTML = '';
        startCountdown(5, () => {
            performAction({ action: 0 });
        });
    }
}

document.getElementById('gainBtn').onclick = async () => {
    await performAction({ action: 0 });
};
document.getElementById('attackBtn').onclick = () => {
    currentAction = 'attack';
    const select = document.getElementById('attackSelect');
    select.innerHTML = '';
    attacks.forEach((name, idx) => {
        const opt = document.createElement('option');
        opt.value = idx;
        opt.text = name;
        select.appendChild(opt);
    });
    document.getElementById('attackOptions').style.display = 'block';
    document.getElementById('defenseOptions').style.display = 'none';
};
document.getElementById('defenseBtn').onclick = () => {
    currentAction = 'defense';
    const select = document.getElementById('defenseSelect');
    select.innerHTML = '';
    defenses.forEach((name, idx) => {
        const opt = document.createElement('option');
        opt.value = idx;
        opt.text = name;
        select.appendChild(opt);
    });
    document.getElementById('defenseOptions').style.display = 'block';
    document.getElementById('attackOptions').style.display = 'none';
};

document.getElementById('blackholeBtn').onclick = async () => {
    await performAction({ action: 3 });
};
document.getElementById('whiteholeBtn').onclick = async () => {
    await performAction({ action: 4 });
};
document.getElementById('goldenBtn').onclick = async () => {
    await performAction({ action: 5 });
};
document.getElementById('bombBtn').onclick = async () => {
    await performAction({ action: 6 });
};
document.getElementById('releaseBlackholeBtn').onclick = async () => {
    await performAction({ action: 7 });
};

document.getElementById('confirmAttack').onclick = async () => {
    if (currentAction !== 'attack') return;
    const attackId = parseInt(document.getElementById('attackSelect').value);
    let multiplier = parseInt(document.getElementById('multiplier').value);
    if (isNaN(multiplier) || multiplier < 1) multiplier = 1;
    await performAction({ action: 1, attackId: attackId, multiplier: multiplier });
};
document.getElementById('cancelAttack').onclick = () => {
    document.getElementById('attackOptions').style.display = 'none';
    currentAction = null;
};

document.getElementById('confirmDefense').onclick = async () => {
    if (currentAction !== 'defense') return;
    const defenseId = parseInt(document.getElementById('defenseSelect').value);
    await performAction({ action: 2, defenseId: defenseId });
};
document.getElementById('cancelDefense').onclick = () => {
    document.getElementById('defenseOptions').style.display = 'none';
    currentAction = null;
};

document.getElementById('resetBtn').onclick = async () => {
    stopCountdown();
    await startGame();
};

startGame();