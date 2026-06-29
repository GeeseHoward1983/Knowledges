# 29 · 蒸汽波 / Vaporwave

> 80s 落日 + 透视网格 + 霓虹粉青黄渐变文字。SYNTHWAVE / OUTRUN 美学的后台。

## 设计立场
灵感：1980s 蒸汽波 / Synthwave / Outrun 海报；A E S T H E T I C 文化里的"落日 + 棕榈 + 网格"母题。整页固定背景：
- **天空渐变**：紫罗兰夜 → 紫 → 粉 → 橙 → 米油（180° 5 段）
- **横条切割的太阳**：50% 居中的圆形径向辉光（黄 → 粉）+ 用 `mask:radial-gradient` 裁出圆盘，叠加横向"百叶窗"条纹（80s Sunset 标志性）
- **透视网格地板**：底部 55vh 的网格用 `transform:perspective(420px) rotateX(56deg)` 透视变形，靠 mask 渐隐
- 所有面板都漂在这层风景上面（半透紫黑底 + backdrop-filter 模糊 + 1.5px 霓虹描边）

与 04·子夜霓虹的明确区分：
- 04 的霓虹是**紫红/紫/青三色描边**，主体仍是现代 UI；
- 29 是**真景片**——落日 + 透视网格地板是占据整页底层的固定景观，大数字用**白→黄→粉→青 4 段渐变文字**，气质是 1985 Tokyo by Night 海报。
- 字体上 29 大量用 **Courier New 900 weight + VT323**，避开普通几何无衬线。

## 配色
| 角色 | 取值 |
| --- | --- |
| 天空 5 段 | `#3D1A6B → #7434C8 → #FF6FC2 → #FFA85C → #FFE9CB` |
| 网格粉 | `#FF66C4` |
| 网格青 | `#5DE3FF` |
| 主文字 | `#FDEEFA` |
| 霓虹粉 | `#FF3DAA` |
| 霓虹青 | `#1EE0FF` |
| 霓虹黄 | `#FFEB3D` |
| 热珊瑚 | `#FF7068` |

## 字体
- 显示字 / KPI 大数字 / H1：**Courier New 900 weight**（粗等宽，是 80s 街头海报标志字）
- 中文：Microsoft YaHei 兜底
- 数据 / 时间戳 / 表格：**VT323**
- 没有任何"圆润"字体

H1 用 180° 4 段渐变（白 → 黄 → 粉 → 青）+ text-shadow 模拟霓虹辉光。

## 布局
- 240px 边栏：rgba 紫黑半透 + backdrop blur + 霓虹粉描边 + 1px 内反光 + 4px 外辉光
- toolbar：LIVE chip + PROD chip + 搜索（霓虹粉下划线）+ ⌘K chip
- Hero：超大 64px 渐变 H1 `2,418,902 / requests` + 右上角青色霓虹圆环 SVG + 一段亚字间距 0.32em 的 Eyebrow
- KPI 4 张面板：粉/青/黄/珊瑚 4 色霓虹描边 + 内 1px 反光 + 外 box-shadow 辉光
- 主区两栏：粉描边 access log + 青描边 roster + 粉描边 billing
- 账单总计是**霓虹黄边框 + 黄色 28px 大数字 + 黄色辉光**

## 签名元素
1. **横条切割太阳** `body::before + ::after`，第二层用 `repeating-linear-gradient` 横条 + `mask:radial-gradient` 裁圆 —— 是 80s Synthwave Sun 的经典做法
2. **透视网格地板**：`transform:perspective(420px) rotateX(56deg)` + 双层 `repeating-linear-gradient`（粉竖线 + 青横线）
3. **粉/青/黄 4 段渐变文字 H1** + `text-shadow` 6 层柔光
4. **品牌名 "·VAPOR·CLOUD·"** 字间距 0.26em + 青色辉光，是 vaporwave 标志的"双点 + 空格"字法

## 适用场景
游戏 / 音乐 / Web3 / 二次元 / NFT / 独立潮牌 / 街头文化 SaaS 控制台。**不适合**：金融、医疗、政企、严肃 B2B——会被认为"不专业"。
