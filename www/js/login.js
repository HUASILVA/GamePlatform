const API_BASE = 'http://localhost:8080';

function showMessage(msg, isError=true) {
    const div = document.getElementById('message');
    div.textContent = msg;
    div.className = isError ? 'error' : 'success';
    setTimeout(() => div.textContent = '', 3000);
}

// 登录逻辑
document.getElementById('loginBtn').onclick = async () => {
    const username = document.getElementById('loginUser').value;
    const password = document.getElementById('loginPass').value;
    if (!username || !password) return showMessage('用户名/密码不能为空');
    try {
        const res = await fetch(`${API_BASE}/api/login`, {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({username, password})
        });
        const data = await res.json();
        if (data.success) {
            localStorage.setItem('token', data.token);
            showMessage('登录成功，跳转中...', false);
            setTimeout(() => window.location.href = 'index.html', 1000);
        } else {
            showMessage('登录失败：' + (data.error || '用户名或密码错误'));
        }
    } catch(e) {
        showMessage('网络错误');
    }
};