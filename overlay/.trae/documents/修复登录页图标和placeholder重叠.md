## 问题分析
从截图可以看到：
- 用户图标和"用户名"placeholder文字重叠
- 密码图标和"密码"placeholder文字重叠

## 原因
虽然设置了 `padding-left: 3.5rem`，但 placeholder 文字的位置可能受其他样式影响。

## 修复方案
1. 增加 `.form-input` 的 `padding-left` 到更大值（4rem 或更大）
2. 确保 placeholder 也能被正确推开
3. 或者调整图标位置，使用 flex 布局替代 absolute 定位