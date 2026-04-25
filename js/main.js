let currentLang = 'zh';

function toggleLang() {
    currentLang = currentLang === 'zh' ? 'en' : 'zh';
    document.querySelectorAll('[data-lang]').forEach(function(el) {
        if (el.getAttribute('data-lang') === currentLang) {
            el.classList.add('active');
        } else {
            el.classList.remove('active');
        }
    });
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

// Initialize Chinese as default
document.addEventListener('DOMContentLoaded', function() {
    document.querySelectorAll('[data-lang="zh"]').forEach(function(el) {
        el.classList.add('active');
    });
});
