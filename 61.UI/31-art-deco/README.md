# 31 · 装饰艺术 / Art-Deco

> 1920s 大都会 Art Deco：深酒红近黑底 + 烫金双线对称几何 + Cinzel 衬线大写 + 法语标签。

## 设计立场
灵感：纽约克莱斯勒大厦电梯门、Erté 海报、大都会博物馆 Deco 展厅。每个面板都是一个**对称镶金的双线框**：
- 1.5px 主金边 + 5px 内嵌 1px 暗金细线，形成 Deco 经典双框
- 上下中点压一组对称小花饰（◆ + ● + ◆ 钻石 + 圆点）
- 标题 Cinzel 大写 + 字间距 0.32em
- 大数字用 180° 烫金 3 段渐变（亮金 → 金 → 暗金）填充文字
- 全页**法语 + 英文 + 中文**混排：`Maison de Northwind / Vol. XII / Six · Dix-Neuf / Journal d'Accès / Membres / Compte / Forfait / Dépassement / Reçu`

与 22·鎏金的明确区分：
- 22·鎏金：深松石绿 + 朱砂 + 中式器物 + 汉字数词，**东方器物**；
- 31·Art-Deco：深酒红 + 烫金双线对称 + 法语 + Cinzel/Cormorant，**西方 1920s 高级沙龙**；
- 角色徽章 22 用圆形朱印，31 用六边形勋章 `clip-path:polygon(50% 0,93% 25%,93% 75%,50% 100%,7% 75%,7% 25%)`，气质完全不同。

## 配色
| 角色 | 取值 |
| --- | --- |
| 酒红近黑 | `#1A1418` → `#100C10` |
| 面板 | `#241A20` / `#291E26` / `#2E212A` |
| 香槟米字 | `#EFE4CB` |
| 次字 | `#B89E7A` |
| 弱字 | `#7A6A52` |
| **烫金** | `#C6A560` / 亮 `#E5C886` / 暗 `#8E7136` |
| 波尔多红 | `#7A2435` / `#A03A4B` |

## 字体
- 标题 / 章节 / KPI 标签：**Cinzel**（致敬古罗马碑刻的大写衬线，weight 600）
- 数字 / 名字 / 副标：**Cormorant Garamond**（italic 大量使用）
- 中文：思源宋体
- 数据：JetBrains Mono

字号严格分层：H1 46px Cinzel + 渐变金；KPI 38px Cormorant; section title 14px Cinzel 字间距 0.18em；正文 14.5px Cormorant italic。

## 布局
- 240px 边栏，品牌上下都是金色"●"端点圆环；分区标题`Total / Atelier / Maison` 左右各一条 20px 金色短线；导航项前 ◆ 钻石点，编号用罗马数字 I-IX
- Hero 是一个对称的 Deco 边框，顶部和底部各嵌一组 `dot + diamond + dot` 花饰；H1 用渐变金 Cinzel 46px "Vol. XII / *Six · Dix-Neuf*"
- 4 张 KPI 都是 Deco 框 + 顶部 3 颗金点
- 主区两栏：日志 / Membres / Compte，每个 panel 顶部和底部各嵌一颗金钻
- 账目合计有 4 角金色 deco corner
- footer 居中"◆ Maison de Northwind · MMXXVI · Folio XII ◆"

## 签名元素
1. **Deco 双线边框**：1.5px 主线 + 5px 内嵌 1px 细线
2. **对称花饰 `.deco-cap`**：上下中点压一组金色 dot/diamond/dot
3. **罗马数字编号** I-IX
4. **法语标签** `Journal d'Accès / Membres / Compte / Forfait Growth / Dépassement / Reçu / Inviter`
5. **六边形勋章头像**

## 适用场景
高端品牌后台（钟表 / 珠宝 / 时装 / 酒店 / 私人会所 / 米其林餐厅）、私人银行、艺术拍卖、奢侈品 CRM。**不适合**：技术工具、运维、年轻 C 端。
