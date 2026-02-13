## 修改计划

### 1. 修改 system.html
**文件**: `system.html`
- 将硬编码的 `--jade-primary`, `--jade-light`, `--jade-glow` 替换为通用 CSS 变量
- 使用 `--accent-color`, `--accent-glow` 等变量

### 2. 为所有主题添加 system 页面样式
**文件**: 
- `galaxy-theme.css` (默认主题)
- `sky-cartoon-theme.css` (天空卡通)
- `dark-luxury-theme.css` (黑蓝奢华)
- `colorful-theme.css` (彩色活力)
- `moonlight-theme.css` (月光写实)
- `chinese-knot-theme.css` (中国结)

**每个主题添加:**
- `.theme .page-title` - 标题渐变
- `.theme .system-card` - 卡片样式
- `.theme .system-card:hover` - 悬停效果
- `.theme .system-icon` - 图标颜色
- `.theme .progress-ring-fill` - 进度环颜色