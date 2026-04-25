let currentLang = localStorage.getItem('jvav-lang') || 'zh';

const pageTitles = {
    'zh': {
        'index.html': 'JVAV - 程序设计语言',
        'features.html': '特性 - JVAV',
        'stdlib.html': '标准库 - JVAV',
        'toolchain.html': '工具链 - JVAV',
        'about.html': '关于 - JVAV'
    },
    'en': {
        'index.html': 'JVAV - The JVAV Programming Language',
        'features.html': 'Features - JVAV',
        'stdlib.html': 'Standard Library - JVAV',
        'toolchain.html': 'Toolchain - JVAV',
        'about.html': 'About - JVAV'
    }
};

function applyLang(lang) {
    currentLang = lang;
    localStorage.setItem('jvav-lang', lang);
    document.documentElement.lang = lang === 'zh' ? 'zh-CN' : 'en';
    document.querySelectorAll('[data-lang]').forEach(function(el) {
        if (el.getAttribute('data-lang') === currentLang) {
            el.classList.add('active');
        } else {
            el.classList.remove('active');
        }
    });
    var path = window.location.pathname.split('/').pop() || 'index.html';
    if (pageTitles[lang] && pageTitles[lang][path]) {
        document.title = pageTitles[lang][path];
    }
}

function toggleLang() {
    applyLang(currentLang === 'zh' ? 'en' : 'zh');
}

function openModal() {
    document.getElementById('modal').classList.add('active');
    document.body.style.overflow = 'hidden';
}

function closeModal() {
    document.getElementById('modal').classList.remove('active');
    document.body.style.overflow = '';
}

function closeModalOutside(event) {
    if (event.target === document.getElementById('modal')) {
        closeModal();
    }
}

document.addEventListener('keydown', function(e) {
    if (e.key === 'Escape') closeModal();
});

// Initialize from localStorage or default to Chinese
document.addEventListener('DOMContentLoaded', function() {
    applyLang(currentLang);
});
