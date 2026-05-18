/**
 * @file guess.js
 * @description 猜数字游戏前端逻辑，支持无限猜测，每次猜错扣5分，猜中获得剩余分数。
 * @requires API_BASE - 后端服务器地址
 */

const API_BASE = 'http://localhost:8080';
let token = localStorage.getItem('token');

// 未登录则跳转到登录页
if (!token) {
    window.location.href = 'login.html';
}

/**
 * 封装的API请求函数，自动添加认证头
 * @param {string} method - HTTP方法 (GET, POST)
 * @param {string} url - 请求路径 (例如 '/game/guess/start')
 * @param {object|null} body - 请求体对象，可选
 * @returns {Promise<object>} 解析后的JSON响应
 */
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
    return await response.json();
}

/**
 * 开始新游戏：重置游戏状态，重置分数显示
 */
async function startGame() {
    try {
        const data = await apiCall('POST', '/game/guess/start');
        document.getElementById('message').innerText = data.message;
        // 剩余次数不再显示，但后端可能返回 -1，忽略
        const remainingSpan = document.getElementById('remaining');
        if (remainingSpan) remainingSpan.innerText = data.remaining === -1 ? '不限' : data.remaining;
        // 重置分数显示为100
        document.getElementById('score').innerText = '100';
        // 启用输入框和按钮
        document.getElementById('guessInput').disabled = false;
        document.getElementById('guessBtn').disabled = false;
    } catch (e) {
        console.error(e);
        alert('开始游戏失败，请重试');
    }
}

/**
 * 提交猜测数字
 */
async function makeGuess() {
    const input = document.getElementById('guessInput');
    const guess = parseInt(input.value);
    if (isNaN(guess) || guess < 1 || guess > 100) {
        alert('请输入1~100之间的数字');
        return;
    }
    try {
        const data = await apiCall('POST', '/game/guess/play', { guess: guess });
        
        // 更新剩余次数（如果后端返回）
        const remainingSpan = document.getElementById('remaining');
        if (remainingSpan && data.remaining !== undefined) {
            remainingSpan.innerText = data.remaining === -1 ? '不限' : data.remaining;
        }
        
        // 构建提示消息
        let msg = `你猜了 ${guess}，结果：`;
        if (data.result === 'high') msg += '太高了';
        else if (data.result === 'low') msg += '太低了';
        else if (data.result === 'win') msg += `恭喜猜中！得分：${data.score}`;
        else if (data.result === 'game_over') msg += '游戏已结束，请开始新游戏。';
        document.getElementById('message').innerText = msg;
        
        // 更新分数显示
        if (data.finished) {
            // 游戏结束（猜中），显示最终得分
            document.getElementById('score').innerText = data.score;
            // 禁用输入框和按钮
            document.getElementById('guessInput').disabled = true;
            document.getElementById('guessBtn').disabled = true;
        } else {
            // 未结束时，更新当前分数（每猜错一次扣5分后剩余的分数）
            if (data.currentScore !== undefined) {
                document.getElementById('score').innerText = data.currentScore;
            }
        }
        // 清空输入框
        input.value = '';
    } catch (e) {
        console.error(e);
        alert('提交猜测失败');
    }
}

/**
 * 退出登录
 */
document.getElementById('logoutBtn').onclick = () => {
    localStorage.removeItem('token');
    window.location.href = 'login.html';
};

// 绑定按钮事件
document.getElementById('startBtn').onclick = startGame;
document.getElementById('guessBtn').onclick = makeGuess;

/**
 * 页面加载时检查是否有进行中的游戏（用于页面刷新恢复状态）
 */
async function checkState() {
    try {
        const data = await apiCall('GET', '/game/guess/state');
        if (data.active && !data.finished) {
            document.getElementById('message').innerText = '游戏进行中，继续猜！';
            const remainingSpan = document.getElementById('remaining');
            if (remainingSpan && data.remaining !== undefined) {
                remainingSpan.innerText = data.remaining === -1 ? '不限' : data.remaining;
            }
            // 恢复当前分数（如果后端返回）
            if (data.currentScore !== undefined) {
                document.getElementById('score').innerText = data.currentScore;
            } else {
                // 兼容旧版，默认100
                document.getElementById('score').innerText = '100';
            }
            document.getElementById('guessInput').disabled = false;
            document.getElementById('guessBtn').disabled = false;
        } else if (data.active && data.finished) {
            document.getElementById('message').innerText = '上一局已结束，点击开始新游戏。';
            document.getElementById('guessInput').disabled = true;
            document.getElementById('guessBtn').disabled = true;
            // 如果游戏已结束但分数未显示，尝试获取最终分数（可能需要额外接口）
        } else {
            document.getElementById('message').innerText = '尚未开始游戏，点击开始新游戏。';
            document.getElementById('score').innerText = '100';
        }
    } catch (e) {
        console.error(e);
    }
}

checkState();