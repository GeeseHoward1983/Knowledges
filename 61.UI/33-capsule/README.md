# 33 · 太空舱 / Capsule

> NASA / SpaceX HUD 飞行驾驶舱美学：深空底 + 青色 HUD 描边 + 大胶囊圆角 + 圆形主表。

## 设计立场
灵感：龙飞船 Dragon Capsule 的触控驾驶舱、NASA Mission Control HUD、Tesla Model S 大圆角触控面板。整页核心视觉语言：
- 极大圆角（边栏 24px / 顶部 toolbar 99px 全胶囊 / KPI 18px / 主面板 22px / 账单合计 99px 胶囊）
- 深空底叠 SVG 星点 + 顶部青色辉光
- Hero 是一个 280px × 280px 的**圆形 HUD 主表盘**（同心圆 + 虚线 + 渐变描边 + 中心大数 72%）
- 所有"卡"都有 backdrop 暗紫渐变（capsule → capsule-2）+ 青色 0.14 透明度细描边 + 内 1px 反光
- 文案"CAPSULE · FLIGHT DECK · PAYLOAD · CREW · TELEMETRY"，把 SaaS 控制台翻译成航天系统术语

与 04·子夜霓虹、20·渐变流光、29·蒸汽波的明确区分：
- 04 是热紫红渐变文字 + 霓虹辉光，气质夜店；
- 20 是动画极光 blob 背景；
- 29 是落日 + 透视网格地板的 80s 蒸汽波；
- 33 是**冷青 HUD + 圆形主表 + 胶囊圆角**——纯工程冷静感，没有渐变色，唯一辉光来自青色 box-shadow。

## 配色
| 角色 | 取值 |
| --- | --- |
| 深空底 | `#070A14` → `#050811` |
| 舱体 | `#10182B` / `#162035` |
| 主文字 | `#E8EEF7` |
| 次字 | `#7F8DA8` |
| 弱字 | `#4E5872` |
| **HUD 青** | `#3CD0FF` / 亮 `#80E8FF` / 暗 `#1E7AA0` |
| OK 绿 | `#5CE090` |
| 警告 | `#FFA945` |
| 危险 | `#FF5C7A` |

## 字体
- 全页：**Space Mono**（NASA 自带的 SpaceX 风等宽字）
- UI 正文 / 名字 / 中文：Inter / PingFang SC

## 布局
- 230px 边栏胶囊（24px 圆角 + 顶部 1px 渐变 HUD 细线）：品牌"轨道环"（双圈 + 12 点位小球）+ 3 段 nav（flight deck / payload / crew）
- 顶部 99px 全胶囊 toolbar：STREAM 5s / CAPSULE · PROD / P95 184ms / UPTIME 17D + 搜索 + ⌘K
- Hero 是 280×280 圆形 HUD 仪表盘（同心环 + 380° 完成度的渐变 stroke + 中心 30px QUOTA 72%）+ 右侧 2×2 KPI 矩阵
- 主区两栏：TELEMETRY · ACCESS LOG + CREW · ROSTER + BILLING
- 账单合计是**全胶囊圆角的高亮青色 banner**（rounded 99px）

## 签名元素
1. **圆形 HUD 主表盘**：3 层同心圆 + 渐变 stroke（青→绿）+ 虚线辅环 + 中心写 QUOTA / 72% / 2.42M / 3.0M
2. **极大圆角胶囊**：toolbar 和账单合计都是 99px 圆角
3. **轨道头像** `border:1px solid hud + radial-gradient` + 中央汉字
4. **航天术语命名** FLIGHT DECK / PAYLOAD / CREW / TELEMETRY

## 适用场景
航空航天 / 自动驾驶 / 智能硬件 / 机器人 / 量化交易高速通道 / 5G NOC。**不适合**：传统行业、温情场景。
