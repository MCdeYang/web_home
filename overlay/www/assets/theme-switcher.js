/**
 * 主题切换器 - Theme Switcher
 * 支持6种风格：galaxy(默认)、sky-cartoon、dark-luxury、colorful、moonlight、chinese-knot
 */

(function() {
  'use strict';

  // 主题配置
  const THEMES = {
    galaxy: {
      name: '翡翠流金',
      description: '深空科技感，翡翠绿配流金',
      css: '/assets/galaxy-theme.css',
      bodyClass: 'galaxy',
      preview: 'linear-gradient(135deg, #00d26a, #ffd700)'
    },
    'sky-cartoon': {
      name: '天空卡通',
      description: '天蓝配白色，可爱卡通风',
      css: '/assets/themes/sky-cartoon-theme.css',
      bodyClass: 'sky-cartoon',
      preview: 'linear-gradient(135deg, #4fc3f7, #ffffff)'
    },
    'dark-luxury': {
      name: '黑蓝奢华',
      description: '深蓝配银金，高端商务风',
      css: '/assets/themes/dark-luxury-theme.css',
      bodyClass: 'dark-luxury',
      preview: 'linear-gradient(135deg, #1a237e, #c0c0c0, #d4af37)'
    },
    colorful: {
      name: '彩色活力',
      description: '彩虹渐变，年轻活力风',
      css: '/assets/themes/colorful-theme.css',
      bodyClass: 'colorful',
      preview: 'linear-gradient(135deg, #9c27b0, #e91e63, #ff9800, #4caf50, #2196f3)'
    },
    moonlight: {
      name: '月光写实',
      description: '月光银配深灰，极简写实风',
      css: '/assets/themes/moonlight-theme.css',
      bodyClass: 'moonlight',
      preview: 'linear-gradient(135deg, #e0e0e0, #424242)'
    },
    'chinese-knot': {
      name: '中国结',
      description: '中国红配金色，传统喜庆风',
      css: '/assets/themes/chinese-knot-theme.css',
      bodyClass: 'chinese-knot',
      preview: 'linear-gradient(135deg, #c41e3a, #ffd700)'
    }
  };

  const STORAGE_KEY = 'smart-home-theme';
  let currentTheme = 'galaxy';

  /**
   * 获取保存的主题
   */
  function getSavedTheme() {
    try {
      return localStorage.getItem(STORAGE_KEY) || 'galaxy';
    } catch (e) {
      return 'galaxy';
    }
  }

  /**
   * 保存主题设置
   */
  function saveTheme(theme) {
    try {
      localStorage.setItem(STORAGE_KEY, theme);
    } catch (e) {
      console.warn('无法保存主题设置');
    }
  }

  /**
   * 加载主题CSS
   */
  function loadThemeCSS(themeName) {
    const theme = THEMES[themeName];
    if (!theme) return;

    // 移除旧的主题样式表
    const oldLinks = document.querySelectorAll('link[data-theme]');
    oldLinks.forEach(link => link.remove());

    // 如果不是默认主题，加载对应的CSS
    if (themeName !== 'galaxy') {
      const link = document.createElement('link');
      link.rel = 'stylesheet';
      link.href = theme.css;
      link.setAttribute('data-theme', themeName);
      document.head.appendChild(link);
    }
  }

  /**
   * 更新元素类名以匹配当前主题
   */
  function updateElementClasses(themeName) {
    const themePrefix = THEMES[themeName].bodyClass;
    
    // 按钮类名映射
    const buttonMappings = [
      { from: 'galaxy-btn', to: `${themePrefix}-btn` },
      { from: 'galaxy-btn-primary', to: `${themePrefix}-btn-primary` },
      { from: 'galaxy-btn-gold', to: `${themePrefix}-btn-accent` },
      { from: 'galaxy-home-btn', to: `${themePrefix}-home-btn` },
      { from: 'galaxy-card', to: `${themePrefix}-card` },
      { from: 'galaxy-input', to: `${themePrefix}-input` },
      { from: 'galaxy-switch', to: `${themePrefix}-switch` },
      { from: 'galaxy-container', to: `${themePrefix}-container` }
    ];

    // 为每个映射更新类名
    buttonMappings.forEach(mapping => {
      // 查找所有带有源类名的元素
      document.querySelectorAll(`.${mapping.from}`).forEach(el => {
        // 如果目标类名不同，添加目标类名
        if (mapping.from !== mapping.to) {
          el.classList.add(mapping.to);
        }
      });
    });
  }

  /**
   * 应用主题
   */
  function applyTheme(themeName) {
    const theme = THEMES[themeName];
    if (!theme) {
      console.warn('未知主题:', themeName);
      return;
    }

    currentTheme = themeName;

    // 移除所有主题body类
    Object.values(THEMES).forEach(t => {
      document.body.classList.remove(t.bodyClass);
    });

    // 添加新主题body类
    document.body.classList.add(theme.bodyClass);

    // 加载主题CSS
    loadThemeCSS(themeName);

    // 更新元素类名
    updateElementClasses(themeName);

    // 保存设置
    saveTheme(themeName);

    // 触发自定义事件
    window.dispatchEvent(new CustomEvent('themeChanged', { detail: { theme: themeName } }));
  }

  /**
   * 获取当前主题
   */
  function getCurrentTheme() {
    return currentTheme;
  }

  /**
   * 获取所有主题列表
   */
  function getAllThemes() {
    return THEMES;
  }

  /**
   * 创建设置面板UI
   */
  function createThemeSelector() {
    const container = document.createElement('div');
    container.className = 'theme-selector';
    container.innerHTML = `
      <div class="theme-grid">
        ${Object.entries(THEMES).map(([key, theme]) => `
          <div class="theme-option ${key === currentTheme ? 'active' : ''}" data-theme="${key}">
            <div class="theme-preview" style="background: ${theme.preview}"></div>
            <div class="theme-info">
              <div class="theme-name">${theme.name}</div>
              <div class="theme-desc">${theme.description}</div>
            </div>
          </div>
        `).join('')}
      </div>
    `;

    // 绑定点击事件
    container.querySelectorAll('.theme-option').forEach(option => {
      option.addEventListener('click', () => {
        const theme = option.dataset.theme;
        applyTheme(theme);
        
        // 更新选中状态
        container.querySelectorAll('.theme-option').forEach(o => o.classList.remove('active'));
        option.classList.add('active');
      });
    });

    return container;
  }

  /**
   * 初始化主题
   */
  function init() {
    const savedTheme = getSavedTheme();
    applyTheme(savedTheme);
  }

  // 暴露全局API
  window.ThemeSwitcher = {
    apply: applyTheme,
    getCurrent: getCurrentTheme,
    getAll: getAllThemes,
    createSelector: createThemeSelector,
    init: init
  };

  // 页面加载时初始化
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }
})();
