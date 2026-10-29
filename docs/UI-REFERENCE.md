# 界面参考与独立实现

2026-10-28，按产品要求阅读 WeFlow 的聊天、头像及资料展示实现，参考其侧栏与会话分区、清晰的主次操作、资料分行和一致控件尺度。

- 参考仓库：https://github.com/hicccc77/WeFlow
- 阅读版本：`b4d334d6accc7b4d92cb0c54124eaf67d61e9807`
- 许可证：https://github.com/hicccc77/WeFlow/blob/main/LICENSE ，CC BY-NC-SA 4.0，含非商业及相同方式共享条款。
- 本次未把 WeFlow 的 React/SCSS 组件、图片、图标、品牌或源码复制到影聊。影聊继续使用已有 GPLv3 Qt/C++ 实现，新增布局和样式由本项目独立编写。此文件记录设计参考，不把 WeFlow 列作已分发的代码依赖。

影聊采用自己的蓝白配色与中文界面：背景白色、输入区浅灰蓝、主操作蓝色，24px 主要外边距、8px 控件间距，普通按钮统一高度，资料窗口使用头像与身份头部及分行详情。我的资料默认展示卡片，编辑才展开表单；同事资料不会显示为可编辑自己的身份。

原 BeeBEEP 及既有第三方声明继续保留于 `LICENSE.txt` 和 `THIRD_PARTY_NOTICES.md`。
