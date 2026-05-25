const API_BASE = 'http://localhost:8080';

// 正则校验：只允许英文字母和数字
function isValidInput(str) {
    return /^[A-Za-z0-9]+$/.test(str);
}

function showMessage(msg, isError = true) {
    const div = document.getElementById('message');
    div.textContent = msg;
    div.className = isError ? 'error' : 'success';
    setTimeout(() => div.textContent = '', 3000);
}

document.getElementById('regBtn').onclick = async () => {
    const username = document.getElementById('regUser').value;
    const password = document.getElementById('regPass').value;

    // 非空检查
    if (!username || !password) {
        showMessage('用户名/密码不能为空');
        return;
    }

    // 新增：合法性检查
    if (!isValidInput(username) || !isValidInput(password)) {
        showMessage('用户名和密码只能包含英文字母和数字');
        return;
    }

    try {
        const res = await fetch(`${API_BASE}/api/register`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ username, password })
        });
        const data = await res.json();
        if (data.success) {
            showMessage('注册成功！2秒后跳转到登录页...', false);
            setTimeout(() => window.location.href = 'login.html', 2000);
        } else {
            let errMsg = data.error;
            if (errMsg && errMsg.includes('UNIQUE constraint failed')) {
                errMsg = '用户名已存在，请换一个';
            }
            showMessage('注册失败：' + errMsg);
        }
    } catch (e) {
        showMessage('网络错误');
    }
};