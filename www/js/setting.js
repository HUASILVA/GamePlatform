const API_BASE = 'http://localhost:8080';
let token = localStorage.getItem('token');
if (!token) {
    window.location.href = 'login.html';
}

async function deleteAccount() {
    const confirmDelete = confirm('确定要注销账号吗？此操作不可恢复，所有数据将被删除。');
    if (!confirmDelete) return;

    try {
        const response = await fetch(API_BASE + '/api/user', {
            method: 'DELETE',
            headers: {
                'Authorization': `Bearer ${token}`,
                'Content-Type': 'application/json'
            }
        });
        const data = await response.json();
        if (response.ok && data.success) {
            alert('账号已注销，即将跳转到登录页。');
            localStorage.removeItem('token');
            window.location.href = 'login.html';
        } else {
            document.getElementById('message').innerText = '注销失败：' + (data.error || '未知错误');
        }
    } catch (err) {
        document.getElementById('message').innerText = '网络错误，请重试。';
    }
}

document.getElementById('deleteAccountBtn').onclick = deleteAccount;