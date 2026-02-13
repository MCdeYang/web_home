## 项目概述
使用 GitHub Galaxy 设计风格重新设计家庭智能终端前端，保持现有 JavaScript 逻辑和后端接口不变。

## 设计规范
- **主色调**: 翡翠绿 (#00d26a) 
- **辅色调**: 流金 (#ffd700)
- **背景**: 4K深空星空效果，动态星云
- **组件风格**: 玻璃态(Glassmorphism)卡片，发光边框
- **动画**: 粒子连线、悬浮效果、呼吸光晕

## 修改文件清单

### 1. 已创建新文件
- `/www/assets/galaxy-theme.css` - Galaxy主题核心样式

### 2. 需要修改的HTML文件（8个）
1. `index.html` - 主页面
2. `login.html` - 登录页
3. `control.html` - 家居控制
4. `photos.html` - 家庭相册（保持翻转逻辑）
5. `disk.html` - 私人网盘
6. `notes.html` - 备忘录
7. `system.html` - 系统数据
8. `settings.html` - 设置

### 3. 修改策略
- 保留所有 `<script>` 标签内的 JavaScript 代码不变
- 保留所有 API 接口调用不变
- 替换 CSS 样式和 HTML 结构
- 添加 Galaxy 主题 class 和样式
- 照片翻转效果保持原有实现

### 4. 视觉特性
- 动态粒子背景（Canvas实现）
- 玻璃态卡片设计
- 翡翠绿发光边框
- 悬浮动画效果
- 高级渐变文字
- 响应式布局

### 5. 预览方式
修改完成后启动本地Web服务器，提供预览URL

请确认此计划后，我将开始逐个修改文件。