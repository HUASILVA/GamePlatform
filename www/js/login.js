const API_BASE = 'http://localhost:8080';

function isValidInput(str) {
    return /^[A-Za-z0-9]+$/.test(str);
}

function showMessage(msg, isError = true) {
    const div = document.getElementById('message');
    div.textContent = msg;
    div.className = isError ? 'error' : 'success';
    setTimeout(() => div.textContent = '', 3000);
}

document.getElementById('loginBtn').onclick = async () => {
    const username = document.getElementById('loginUser').value;
    const password = document.getElementById('loginPass').value;

    if (!username || !password) {
        showMessage('用户名/密码不能为空');
        return;
    }

    // 新增合法性检查
    if (!isValidInput(username) || !isValidInput(password)) {
        showMessage('用户名和密码只能包含英文字母和数字');
        return;
    }

    try {
        const res = await fetch(`${API_BASE}/api/login`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ username, password })
        });
        const data = await res.json();
        if (data.success) {
            localStorage.setItem('token', data.token);
            showMessage('登录成功，跳转中...', false);
            setTimeout(() => window.location.href = 'index.html', 1000);
        } else {
            showMessage('登录失败：' + (data.error || '用户名或密码错误'));
        }
    } catch (e) {
        showMessage('网络错误');
    }
};