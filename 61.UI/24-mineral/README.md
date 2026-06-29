# 24 · 矿物标本 / Mineral

> 维多利亚博物馆抽屉柜 + 矿物标本卡片 + 拉丁文学名。一份装订成"编目"的后台。

## 设计立场
灵感：自然博物馆抽屉柜里的矿物标本卡 + Wes Anderson 的对称装订美学。每张 KPI 是一张**矿物标本卡**：上半截是矿物本体（带渐变 + 半透明几何"晶面"），下半截是手写标签纸（汉字粗体 + 拉丁文学名 italic + 数值）。每个角色（owner/admin/dev/read）也对应一种真实矿物（蓝铜矿 Azurite、孔雀石 Malachite、黄铁矿 Pyrite、紫水晶 Amethyst），头像是六边形剪裁。

整页是"编目体"：边栏顶部"Northwind Cabinet · Cat. № 01 / Vol. XII"、章节叫 "Sectio I / II / III"、表头叫 "Catalogus Vocationum"、合计写 "Summa"、底栏写 "Folio SHEET 01 / XII"。

## 配色
| 角色 | 取值（矿物名） |
| --- | --- |
| 标本纸 | `#F3EFE3` + 噪点 |
| 次纸 | `#E8E2D1` |
| 主文字 | `#1A1810` |
| 实线 | `#1A1810` |
| 蓝铜矿 Azurite | `#2E5A8A` |
| 孔雀石 Malachite | `#3E7855` |
| 黄铁矿 Pyrite | `#C5A23A` |
| 朱砂 Cinnabar | `#A8392C` |
| 紫水晶 Amethyst | `#6B4C8C` |
| 蔷薇辉石 Rose | `#C8728E` |
| 赤铁矿 Hematite | `#5C2A28` |

所有彩色都有真实矿物对应。

## 字体
- 显示字 / H1 / KPI 数字 / 名字：**Cormorant Garamond**（带 italic 的优雅衬线）
- 中文：**Source Han Serif SC**
- 标签 / Eyebrow / 编号：**Inter** 极小号 + 字间距大
- 数据：JetBrains Mono

H1 形式："本月运行 · 一份编目。Specimen Sheet № 01 — Aestus mensis Iunii MMXXVI"，用了**罗马数字 + 拉丁文** Aestus mensis Iunii MMXXVI（"二零二六年六月的盛况"）。

## 布局
- 230px 标本柜边栏：顶部一张"标本柜立牌"（Northwind Cabinet · Cat. № 01 / Vol. XII），章节标签是 Cormorant italic "Sectio I 总览"，导航项前小色方块代表矿物
- Hero 正文 H1 + Latin 副标 + 右上"Cabinet of Northwind / 06—19—2026 / Sheet 01 / Folio XII / Prod"
- 4 张 KPI 标本卡：
  - 上半截**矿物色 + 渐变 + 半透明白色晶面 SVG**（screen 混合）
  - 右上角小标签 `SP-01 / SP-02 / SP-03 / SP-04`
  - 下半截标签纸：粗体汉字 + Cormorant italic 拉丁名 + 大数字 + delta
- 主区两栏：流水编目（端点旁注 vocatio）+ 名册（每个角色对应一种矿物 + 右侧学名）+ 账目（合计写 Summa）
- 账目进度条用 45° 蓝铜矿/孔雀石双色斜纹（两种矿物相互嵌入）

## 签名元素
1. **KPI = 矿物标本卡**：上半矿物色 + 半透明几何晶面，下半标签纸
2. **拉丁文学名**：每个角色右侧标"Azurite / Malachite / Pyrite / Amethyst"
3. **六边形头像**（`clip-path:polygon(20% 0,80% 0,100% 35%,80% 100%,20% 100%,0 35%)`）—— 仿矿物晶体的几何
4. **罗马数字编号** `№ 01 / VOL. XII` + 拉丁文章节 `Sectio I / II / III`

## 适用场景
博物馆 / 图书馆 / 古董拍卖 / 标本店 / 高端独立咖啡 / 葡萄酒电商等需要"博学 + 雅致 + 严谨"的客户。**不适合**：高 QPS 数据狗 / 运维。
