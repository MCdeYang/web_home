## 问题分析
padding-left 虽然设置了，但 placeholder 文字似乎不受其影响，仍然从输入框的最左边开始显示。

## 解决方案
使用 flex 布局重构表单组：
1. 将 `.form-group` 改为 flex 布局
2. 图标作为独立元素放在输入框左侧
3. 输入框不再使用 absolute 定位的图标
4. 这样 placeholder 会自然地从图标右侧开始

## 具体修改
1. `.form-group` - 改为 `display: flex; align-items: center;`
2. `.form-icon` - 改为静态定位，放在 flex 容器中
3. `.form-input` - 移除大 padding-left，改为 flex: 1
4. 密码切换按钮保持绝对定位在右侧