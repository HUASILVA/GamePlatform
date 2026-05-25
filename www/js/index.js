const API_BASE = 'http://localhost:8080';
const token = localStorage.getItem('token');
if (!token) {
    window.location.href = 'login.html';
}



async function loadUser() {
    try {
        const res = await fetch('http://localhost:8080/api/me', {
            headers: { 'Authorization': `Bearer ${token}` }
        });
        if (res.status === 401) {
            localStorage.removeItem('token');
            window.location.href = 'login.html';
            return;
        }
        const data = await res.json();
        document.getElementById('userInfo').textContent = `欢迎，${data.username}`;
    } catch(e) {
        document.getElementById('userInfo').textContent = '获取用户信息失败';
    }
}

document.getElementById('settingsBtn').onclick = () => {
    window.location.href = 'setting.html';
};

document.getElementById('logoutBtn').onclick = () => {
    localStorage.removeItem('token');
    window.location.href = 'login.html';
};

loadUser();

// 加载排行榜
async function loadRanking() {
    try {
        const response = await fetch(API_BASE + '/api/rank?game=guess');
        if (!response.ok) throw new Error('网络错误');
        const data = await response.json();
        const tbody = document.getElementById('rankBody');
        if (!tbody) return;
        tbody.innerHTML = '';
        if (data.length === 0) {
            tbody.innerHTML = '<tr><td colspan="4">暂无数据</td></tr>';
            return;
        }
        data.forEach((entry, index) => {
            const row = tbody.insertRow();
            row.insertCell(0).innerText = index + 1;
            row.insertCell(1).innerText = entry.username;
            row.insertCell(2).innerText = entry.score;
            // 格式化时间（去掉T和毫秒）
            let time = entry.updated_at.replace('T', ' ').substring(0, 19);
            row.insertCell(3).innerText = time;
        });
    } catch (e) {
        console.error('加载排行榜失败', e);
        const tbody = document.getElementById('rankBody');
        if (tbody) tbody.innerHTML = '<tr><td colspan="4">加载失败</td></tr>';
    }
}

// 页面加载时加载排行榜
loadRanking();
// 每10秒自动刷新排行榜
setInterval(loadRanking, 10000);