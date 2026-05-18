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