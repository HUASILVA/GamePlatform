const API_BASE = 'http://localhost:8080';
const token = localStorage.getItem('token');
if (!token) {
    window.location.href = 'login.html';
}

// 攻击列表（与后端一致）
const attacks = [
    "小波", "波", "龙拳", "能量柱", "闪电五连鞭",
    "十字劈", "直射", "叉字劈", "如来佛掌", "奥特曼激光"
];
// 防御列表
const defenses = [
    "L型防御", "屏障型防御", "平行防御", "x型防御",
    "正叉字型防御", "倒叉字型防御"
];

let currentAction = null; // 'attack' or 'defense'

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

async function startGame() {
    try {
        await apiCall('POST', '/game/cbd/start');
        await updateState();
        document.getElementById('lastResult').innerHTML = '游戏已开始，选择你的动作！';
        // 启用所有动作按钮
        enableButtons(true);
    } catch (e) {
        console.error(e);
        document.getElementById('lastResult').innerHTML = '开始游戏失败: ' + e.message;
    }
}

function enableButtons(enable) {
    const btns = ['gainBtn', 'attackBtn', 'defenseBtn', 'blackholeBtn', 'whiteholeBtn', 'goldenBtn', 'bombBtn'];
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
            document.getElementById('lastResult').innerHTML = state.last_result;
        }
        // 可选：如果游戏结束，禁用按钮
        if (state.player_hp <= 0 || state.ai_hp <= 0) {
            enableButtons(false);
        }
    } catch (e) {
        console.error(e);
    }
}

async function performAction(actionData) {
    try {
        const result = await apiCall('POST', '/game/cbd/action', actionData);
        document.getElementById('playerHp').innerText = result.player_hp;
        document.getElementById('playerEnergy').innerText = result.player_energy;
        document.getElementById('aiHp').innerText = result.ai_hp;
        document.getElementById('aiEnergy').innerText = result.ai_energy;
        document.getElementById('lastResult').innerHTML = result.last_result;

        if (result.game_over) {
            enableButtons(false);
        }
        // 关闭选择面板
        document.getElementById('attackOptions').style.display = 'none';
        document.getElementById('defenseOptions').style.display = 'none';
        currentAction = null;
    } catch (e) {
        console.error(e);
        document.getElementById('lastResult').innerHTML = '动作提交失败: ' + e.message;
    }
}

// 基础动作
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

// 特殊动作
document.getElementById('blackholeBtn').onclick = async () => {
    await performAction({ action: 3 }); // 黑洞
};
document.getElementById('whiteholeBtn').onclick = async () => {
    await performAction({ action: 4 }); // 白洞
};
document.getElementById('goldenBtn').onclick = async () => {
    await performAction({ action: 5 }); // 金身
};
document.getElementById('bombBtn').onclick = async () => {
    await performAction({ action: 6 }); // 爆破
};

// 确认攻击
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
// 确认防御
document.getElementById('confirmDefense').onclick = async () => {
    if (currentAction !== 'defense') return;
    const defenseId = parseInt(document.getElementById('defenseSelect').value);
    await performAction({ action: 2, defenseId: defenseId });
};
document.getElementById('cancelDefense').onclick = () => {
    document.getElementById('defenseOptions').style.display = 'none';
    currentAction = null;
};

// 重置游戏
document.getElementById('resetBtn').onclick = async () => {
    await startGame();
};

// 页面加载开始游戏
startGame();