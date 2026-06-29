# 05 · Qt 框架 Hook（全版本完整指南：Qt 3 / Qt 4 / Qt 5 / Qt 6）

> 本篇覆盖 Qt 自 Qt 3 至 Qt 6.x 所有版本的"hook"机制，按 API 分类讲解，每个 API 标注：**起始版本 / 弃用版本 / 移除版本 / 签名变更点 / 跨版本写法**。
> 涉及：事件过滤器、原生事件过滤器（含 Qt 4 的 `winEventFilter` / `x11EventFilter` / `macEventFilter` → Qt 5 统一为 `QAbstractNativeEventFilter` 的演进）、消息处理器（`qInstallMsgHandler` → `qInstallMessageHandler`）、`QInternal::registerCallback`、事件分发器、信号-槽 monitoring、QObject 元对象 hook、QML/Quick hook、平台差异。
> 假设读者：会 C++ + Qt 基础，需要写跨多个 Qt 版本兼容的代码（如长期维护的工业软件、跨 Qt4→Qt6 迁移项目）。

---

## 0. 版本路线图速览

| Qt 版本 | 发布 | 关键 hook 相关变化 |
|---|---|---|
| Qt 3.x | 2001–2005 | `QApplication==setGlobalMouseTracking`、`QObject==installEventFilter` 已有；`qInstallMsgHandler` 签名 `void(*)(QtMsgType, const char*)` |
| Qt 4.0 (2005) | 重写 | `QCoreApplication::winEventFilter` / `x11EventFilter` / `macEventFilter` 平台分裂 API；新增 `QAbstractEventDispatcher`；信号-槽元对象系统重做 |
| Qt 4.6 | 2009 | `QAbstractEventDispatcher::EventFilter` typedef (函数指针式) |
| Qt 5.0 (2012) | QPA 重构 | **废弃** `winEventFilter`/`x11EventFilter`/`macEventFilter`；引入 `QAbstractNativeEventFilter`（`bool nativeEventFilter(const QByteArray&, void*, long*)`）；`qInstallMsgHandler` → `qInstallMessageHandler`，签名变为 `(QtMsgType, const QMessageLogContext&, const QString&)` |
| Qt 5.4 | 2014 | `QLoggingCategory` 稳定；message pattern (`qSetMessagePattern`) |
| Qt 5.5 | 2015 | `QSignalBlocker` RAII；`QAbstractEventDispatcher::installNativeEventFilter` |
| Qt 5.10 | 2017 | `QInternal::registerCallback` 仍可用但被进一步内部化 |
| Qt 6.0 (2020) | 大破大立 | `nativeEventFilter` 第 3 参数 `long *` **改为** `qintptr *`；移除 `QSignalMapper` 部分重载；元对象格式变化；移除 `QRegExp` |
| Qt 6.2 LTS | 2021 | `QAbstractEventDispatcherV2` 引入 |
| Qt 6.5 LTS | 2023 | 日志相关 `QtLogging` 头独立；`QLoggingCategory` API 扩展 |
| Qt 6.8 LTS | 2024 | `QPointer` 跨线程更安全；事件循环 V2 完善 |
| Qt 6.10+ | 2025+ | 持续完善 V2 dispatcher 接口 |

**核心兼容宏**：

```cpp
[[include]] <QtGlobal>
// QT_VERSION 是十六进制三段，例：Qt 6.5.0 == 0x060500
[[if]] QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt 6+
[[elif]] QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    // Qt 5
[[else]]
    // Qt 4
[[endif]]
```

---

## 1. 概念地图（跨版本）

| 名称 | 拦截目标 | Qt 3 | Qt 4 | Qt 5 | Qt 6 |
|---|---|---|---|---|---|
| `installEventFilter` (对象级) | QObject 事件 | ✓ | ✓ | ✓ | ✓ |
| `installEventFilter` (装到 qApp，全局) | 全应用 QEvent | ✓ | ✓ | ✓ | ✓ |
| 重写 `QObject::event` | 单对象 QEvent | ✓ | ✓ | ✓ | ✓ |
| 重写 `mousePressEvent`/`keyPressEvent` 等 | 特定类型 | ✓ | ✓ | ✓ | ✓ |
| `QApplication::winEventFilter` | Windows MSG | – | ✓ | **deprecated** | 已移除 |
| `QWidget::winEvent` | Widget 级 MSG | ✓ | ✓ | **deprecated** | 已移除 |
| `QApplication::x11EventFilter` | X11 XEvent | – | ✓ | **deprecated** | 已移除 |
| `QWidget::x11Event` | Widget X11 | – | ✓ | **deprecated** | 已移除 |
| `QApplication::macEventFilter` | NSEvent | – | ✓ | **deprecated** | 已移除 |
| `QCoreApplication::installNativeEventFilter` + `QAbstractNativeEventFilter` | 全平台原生消息 | – | – | ✓ (5.0+) | ✓ (签名小改) |
| `QWindow::nativeEvent` | Qt Quick/QWindow 级原生 | – | – | ✓ (5.0+) | ✓ |
| `QWidget::nativeEvent` | QWidget 级原生 | – | – | ✓ (5.0+) | ✓ |
| `qInstallMsgHandler` | qDebug/qWarning 等 | ✓ | ✓ | **deprecated**（5.0 起改名） | 已移除 |
| `qInstallMessageHandler` | 同上 + 上下文 | – | – | ✓ (5.0+) | ✓ |
| `qSetMessagePattern` | 日志格式 | – | – | ✓ (5.0+) | ✓ |
| `QLoggingCategory` | 分类日志 | – | – | ✓ (5.2+) | ✓ |
| `QAbstractEventDispatcher::installNativeEventFilter` | 线程级原生 | – | – | ✓ (5.0+) | ✓ |
| `QAbstractEventDispatcher::EventFilter` (函数指针) | dispatcher 内 | – | ✓ (4.6+) | **deprecated** | 已移除 |
| `QInternal::registerCallback` | 内部 callback（绘制、shutdown）| – | ✓ | ✓ (有意保留) | ✓ |
| `QSignalSpy` | 信号触发 | – | ✓ | ✓ | ✓ |
| `QSignalBlocker` (RAII) | 临时阻塞信号 | – | – | ✓ (5.5+) | ✓ |
| `QMetaObject::Connection` (RAII handle) | 显式 disconnect | – | – | ✓ (5.0+) | ✓ |
| `qmlRegisterType` / Quick 端 hook | QML 集成 | – | – | ✓ | ✓ |

---

## 2. 事件系统底层原理（所有版本通用）

### 2.1 派发流程

无论 Qt 4/5/6，事件派发顺序都是这条链：

```
OS message
  ↓
QAbstractNativeEventFilter (Qt 5+) / xxxEventFilter (Qt 4)
  ↓ (translate to QEvent)
QCoreApplication::notify()
  ↓
qApp->eventFilter()  ← 全局对象 event filter
  ↓
target->eventFilter()  ← 对象级（按 install 反序，后装的先收）
  ↓
target->event()  ← 对象自己的事件处理
  ↓
target->mousePressEvent() / keyPressEvent() / paintEvent() / ... ← 类型分发
```

每一层都可以"吃掉"事件（return true）让它不往下传。

### 2.2 简化模型代码

```cpp
// 简化模型（Qt 内部）
bool QCoreApplication::notify(QObject *receiver, QEvent *event) {
    // 1. 全局 event filter（装在 qApp 上的）
    for (auto *f : qApp_eventFilters) {
        if (f->eventFilter(receiver, event)) return true;
    }
    // 2. 进入对象自身派发
    return receiver->event(event);
}

bool QObject::event(QEvent *e) {
    // 对象级 event filter
    for (auto *f : eventFilters) {
        if (f->eventFilter(this, e)) return true;
    }
    return defaultDispatch(e);   // 走 virtual 子类实现
}
```

直接派发（绕过队列同步调用）：`QCoreApplication::sendEvent(receiver, event)`。
入队派发：`QCoreApplication::postEvent(receiver, event, priority)`，最终在该线程的事件循环里被 take 出来调 notify。

### 2.3 QEvent 类型

`QEvent==type()` 返回 `QEvent==Type` 枚举。常见类型（自 Qt 4 起基本未变）：

| 值 | 何时发出 |
|---|---|
| `MouseButtonPress` / `Release` / `Move` / `DblClick` | 鼠标 |
| `KeyPress` / `KeyRelease` / `ShortcutOverride` | 键盘 |
| `Wheel` | 滚轮 |
| `Resize` / `Move` | 窗口几何变化 |
| `Paint` / `UpdateRequest` | 重绘 |
| `Show` / `Hide` / `ShowToParent` / `HideToParent` | 显隐 |
| `Close` | 关闭请求 |
| `FocusIn` / `FocusOut` / `FocusAboutToChange` (Qt 5.3+) | 焦点 |
| `Timer` | startTimer 触发 |
| `ChildAdded` / `ChildRemoved` / `ChildPolished` | 子对象增删 |
| `LanguageChange` | installTranslator 后 |
| `Enter` / `Leave` | 鼠标进出 |
| `TouchBegin` / `TouchUpdate` / `TouchEnd` | 触摸 (Qt 4.6+) |
| `Gesture` (Qt 4.6+) | 手势 |
| `User` | 用户自定义起点 |

注册自定义事件：

```cpp
class MyEvent : public QEvent {
public:
    static const QEvent::Type type;
    int payload;
    MyEvent(int p) : QEvent(type), payload(p) {}
};
// Qt 4.4+
const QEvent==Type MyEvent==type =
    static_cast<QEvent==Type>(QEvent==registerEventType());
```

> Qt 3 / 早期 Qt 4 没有 `registerEventType`，需要手工选一个 `QEvent::User + N` 的整数，自己负责不冲突。

---

## 3. installEventFilter —— 对象级 event filter（Qt 3 起）

### 3.1 基本用法（全版本一致）

```cpp
[[include]] <QObject>
[[include]] <QEvent>
[[include]] <QKeyEvent>
[[include]] <QDebug>

class KeyLogger : public QObject {
public:
    KeyLogger(QObject *parent = nullptr) : QObject(parent) {}
protected:
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::KeyPress) {
            auto *ke = static_cast<QKeyEvent*>(event);
            qDebug() << watched->objectName() << "key:" << ke->key();
        }
        return false;   // 不消费
    }
};

// 安装
auto *logger = new KeyLogger(parentForLifetime);
edit->installEventFilter(logger);
// 移除
edit->removeEventFilter(logger);
```

### 3.2 装到 `qApp` —— 全局

```cpp
qApp->installEventFilter(new KeyLogger(qApp));
```

任何 QObject 收到的所有事件都会先过这个 filter，**在所有对象级 filter 之前**。

### 3.3 安装顺序

多个 filter 装到同一目标时，**最后安装的最先收到事件**（LIFO）。

```cpp
target->installEventFilter(a);
target->installEventFilter(b);
// 派发顺序：b → a → target->event()
```

### 3.4 Qt 4/5/6 通用注意

| 注意点 | 详细 |
|---|---|
| filter 必须是 QObject | `eventFilter` 是虚函数 |
| filter 必须比 watched 活得长 | 否则崩。通常把 filter 设为 watched 的 parent |
| 跨线程不允许 | filter 必须和 watched 在同一线程 |
| 返回 true 吃掉事件 | 比如吃掉 `KeyPress`，widget 不会再收到 |
| 性能 | 装在 qApp 上每个事件都过一遍，hot path 注意快速 type-check 返回 |

### 3.5 完整示例：阻止 ESC 关闭 Dialog（Qt 4/5/6 都能编）

```cpp
class EscBlocker : public QObject {
public:
    EscBlocker(QObject *p = 0) : QObject(p) {}
protected:
    bool eventFilter(QObject *watched, QEvent *event) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *ke = static_cast<QKeyEvent*>(event);
            if (ke->key() == Qt::Key_Escape) {
                qDebug() << "ESC blocked on" << watched->objectName();
                return true;
            }
        }
        return false;
    }
};

QDialog *dlg = new QDialog;
dlg->installEventFilter(new EscBlocker(dlg));
dlg->exec();
```

> Qt 3 写法：`bool eventFilter(QObject*, QEvent*)` 没有 override 关键字（C++03），其它一致。

---

## 4. 原生事件过滤器（最重的版本差异区）

要拦截 Qt 还没翻译成 `QEvent` 的原生 OS 消息（Windows `MSG`、X11 `XEvent`/`xcb_generic_event_t`、macOS `NSEvent`），API 在 Qt 4 / Qt 5 / Qt 6 完全不同。

### 4.1 Qt 4：平台分裂 API

Qt 4 提供 3 个平台特化的虚函数（直接在 `QApplication` / `QWidget` 子类里 override），编译时按宏切换：

```cpp
class MyApp : public QApplication {
public:
    MyApp(int &argc, char **argv) : QApplication(argc, argv) {}

[[if]] defined(Q_OS_WIN)
    // Qt 4 Windows：拦截 MSG，translation 之前
    bool winEventFilter(MSG *msg, long *result) {
        if (msg->message == WM_HOTKEY) {
            qDebug() << "hotkey id=" << msg->wParam;
            if (result) *result = 0;
            return true;       // true = 吃掉
        }
        return false;
    }
[[endif]]

[[if]] defined(Q_WS_X11)        // 注意：Qt 4 是 Q_WS_X11（Window System），不是 Q_OS_LINUX
    bool x11EventFilter(XEvent *event) {
        if (event->type == KeyPress) { /* ... */ }
        return false;
    }
[[endif]]

[[if]] defined(Q_OS_MAC)
    bool macEventFilter(EventHandlerCallRef caller, EventRef event) {
        return false;
    }
[[endif]]
};
```

或者在 `QWidget` 子类里：

```cpp
class MyWidget : public QWidget {
protected:
[[if]] defined(Q_OS_WIN)
    bool winEvent(MSG *msg, long *result) override { ... }
[[endif]]
[[if]] defined(Q_WS_X11)
    bool x11Event(XEvent *e) override { ... }
[[endif]]
};
```

**这套 API 在 Qt 5.0 起被废弃，Qt 6 完全移除。** 跨版本代码必须做条件编译。

### 4.2 Qt 5：统一的 `QAbstractNativeEventFilter`（5.0 起）

Qt 5 引入 QPA（Qt Platform Abstraction），原生事件过滤统一为：

```cpp
class QAbstractNativeEventFilter {
public:
    virtual ~QAbstractNativeEventFilter();
    // Qt 5: long *result
    virtual bool nativeEventFilter(const QByteArray &eventType,
                                   void *message, long *result) = 0;
};
```

`eventType` 字符串值（按平台分）：

| 平台 | eventType | message 类型 | result 用途 |
|---|---|---|---|
| Windows | `"windows_generic_MSG"` | 发到顶层窗口的 `MSG*` | `LRESULT` 返回值 |
| Windows | `"windows_dispatcher_MSG"` | 系统级 `MSG*`（如 `WM_HOTKEY`、热键、`WM_TIMER`）| 同上 |
| X11 (xcb) | `"xcb_generic_event_t"` | `xcb_generic_event_t*` | 未用 |
| macOS | `"mac_generic_NSEvent"` | `NSEvent*` | 未用 |
| Wayland | `"wayland_xdg_shell_v6_surface"` 等 | 平台特定 | 未用 |

安装/卸载：

```cpp
qApp->installNativeEventFilter(filter);
qApp->removeNativeEventFilter(filter);

// 或绑到当前线程的 dispatcher（只该线程生效）：
QAbstractEventDispatcher::instance()->installNativeEventFilter(filter);
```

### 4.3 Qt 6：`nativeEventFilter` 第 3 参数改为 `qintptr *`

Qt 6 把第 3 参数类型 `long *` 改为 `qintptr *`，原因：在 Windows x64 上 `LRESULT` 是 64 位但 `long` 仍是 32 位，Qt 5 时代有截断隐患。

```cpp
// Qt 6 签名
virtual bool nativeEventFilter(const QByteArray &eventType,
                               void *message, qintptr *result) = 0;
```

### 4.4 跨版本兼容写法

最常见的兼容宏：

```cpp
[[include]] <QtGlobal>
[[if]] QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    typedef qintptr NativeEventResult;
[[else]]
    typedef long    NativeEventResult;
[[endif]]

class HotkeyFilter : public QAbstractNativeEventFilter {
public:
    bool nativeEventFilter(const QByteArray &eventType,
                           void *message,
                           NativeEventResult *result) override {
        [[ifdef]] Q_OS_WIN
        if (eventType == "windows_generic_MSG"
         || eventType == "windows_dispatcher_MSG") {
            MSG *msg = static_cast<MSG*>(message);
            if (msg->message == WM_HOTKEY) {
                qDebug() << "hotkey" << msg->wParam;
                if (result) *result = 0;
                return true;
            }
        }
        [[endif]]
        return false;
    }
};
```

如果你的代码要同时支持 Qt 4（无 `QAbstractNativeEventFilter`），就只能再加一层条件：

```cpp
[[if]] QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    // Qt 5/6: QAbstractNativeEventFilter
[[else]]
    // Qt 4: 用 winEventFilter / x11EventFilter / macEventFilter
[[endif]]
```

### 4.5 完整示例：Windows 全局热键（兼容 Qt 5 + Qt 6）

```cpp
// hotkey_filter.h
[[pragma]] once
[[include]] <QAbstractNativeEventFilter>
[[include]] <QtGlobal>

[[if]] QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
using NativeResult = qintptr;
[[else]]
using NativeResult = long;
[[endif]]

class HotkeyFilter : public QAbstractNativeEventFilter {
public:
    bool nativeEventFilter(const QByteArray &eventType,
                           void *message, NativeResult *result) override;
};
```

```cpp
// hotkey_filter.cpp
[[include]] "hotkey_filter.h"
[[include]] <QDebug>
[[ifdef]] Q_OS_WIN
#  include <windows.h>
[[endif]]

bool HotkeyFilter::nativeEventFilter(const QByteArray &eventType,
                                     void *message, NativeResult *result) {
[[ifdef]] Q_OS_WIN
    if (eventType == "windows_generic_MSG"
     || eventType == "windows_dispatcher_MSG") {
        MSG *msg = static_cast<MSG*>(message);
        if (msg->message == WM_HOTKEY) {
            qDebug() << "Hotkey id =" << msg->wParam;
            if (result) *result = 0;
            return true;
        }
    }
[[else]]
    Q_UNUSED(eventType); Q_UNUSED(message); Q_UNUSED(result);
[[endif]]
    return false;
}
```

```cpp
// main.cpp
[[include]] <QApplication>
[[include]] "hotkey_filter.h"
[[ifdef]] Q_OS_WIN
#  include <windows.h>
[[endif]]

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    HotkeyFilter filter;
    qApp->installNativeEventFilter(&filter);

[[ifdef]] Q_OS_WIN
    // Ctrl+Alt+T
    if (!RegisterHotKey(NULL, 1, MOD_CONTROL | MOD_ALT, 'T'))
        qWarning() << "RegisterHotKey failed";
[[endif]]
    return app.exec();
}
```

CMake：

```cmake
cmake_minimum_required(VERSION 3.16)
project(hotkey)
set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
find_package(Qt6 COMPONENTS Widgets QUIET)
if (NOT Qt6_FOUND)
    find_package(Qt5 5.0 REQUIRED COMPONENTS Widgets)
endif()
add_executable(hotkey main.cpp hotkey_filter.cpp hotkey_filter.h)
if (Qt6_FOUND)
    target_link_libraries(hotkey PRIVATE Qt6::Widgets)
else()
    target_link_libraries(hotkey PRIVATE Qt5::Widgets)
endif()
```

### 4.6 QWindow / QWidget 上的原生事件 hook（Qt 5+）

如果想在某一个 `QWindow` 或 `QWidget` 上拦原生事件而不是全应用：

```cpp
// QWidget 子类
class MyW : public QWidget {
protected:
[[if]] QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override
[[else]]
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override
[[endif]]
    {
        // ... 同上
        return false;
    }
};

// QWindow 子类
class MyWin : public QWindow {
protected:
[[if]] QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override
[[else]]
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override
[[endif]]
    {
        return false;
    }
};
```

QWidget 的 `nativeEvent` 只在该 widget 是 native window（即调用了 `winId()` 或被自动 native 化的顶层）时才会被调用。

### 4.7 何时用 native event filter，何时用普通 event filter

- 普通 `eventFilter` 只能拦 Qt 已翻译成 QEvent 的事件。但 Qt 不翻译 `WM_HOTKEY`、`WM_DEVICECHANGE`、自定义 `RegisterWindowMessage` 等。
- 反过来，鼠标键盘事件 Qt 已经翻好了，**用 native filter 拦只会带来麻烦**——会破坏 widget 默认行为。
- 跨平台的"普通输入"：用 `eventFilter`。
- 平台原生的特殊消息：用 `nativeEventFilter`。

---

## 5. 消息处理器（qDebug / qWarning 等的输出 hook）

### 5.1 Qt 3 / Qt 4：`qInstallMsgHandler`

签名（注意：旧版只有两个参数）：

```cpp
typedef void (*QtMsgHandler)(QtMsgType, const char *);
QtMsgHandler qInstallMsgHandler(QtMsgHandler);
```

Qt 4 例：

```cpp
void myMsg(QtMsgType type, const char *msg) {
    switch (type) {
        case QtDebugMsg:    fprintf(stderr, "[DBG] %s\n", msg); break;
        case QtWarningMsg:  fprintf(stderr, "[WRN] %s\n", msg); break;
        case QtCriticalMsg: fprintf(stderr, "[CRT] %s\n", msg); break;
        case QtFatalMsg:    fprintf(stderr, "[FAT] %s\n", msg); abort();
    }
}
// Qt 4.6+ 增加 QtInfoMsg (5.5+ 正式)
qInstallMsgHandler(myMsg);
```

### 5.2 Qt 5 起：`qInstallMessageHandler`（带上下文）

```cpp
typedef void (*QtMessageHandler)(QtMsgType, const QMessageLogContext &, const QString &);
QtMessageHandler qInstallMessageHandler(QtMessageHandler);
```

`QMessageLogContext` 包含 `file`/`line`/`function`/`category`。

Qt 5/6 例（推荐写法）：

```cpp
[[include]] <QtLogging>     // Qt 6+；Qt 5 可包含 <QDebug>
[[include]] <QFile>
[[include]] <QMutex>
[[include]] <QTextStream>
[[include]] <QDateTime>

static QMutex g_logMutex;

void myMessageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg) {
    const char *level = "?";
    switch (type) {
        case QtDebugMsg:    level = "DEBUG"; break;
        case QtInfoMsg:     level = "INFO";  break;
        case QtWarningMsg:  level = "WARN";  break;
        case QtCriticalMsg: level = "CRIT";  break;
        case QtFatalMsg:    level = "FATAL"; break;
    }
    const QString line = QString("%1 [%2] %3:%4 (%5) %6\n")
        .arg(QDateTime==currentDateTime().toString(Qt==ISODateWithMs))
        .arg(level)
        .arg(ctx.file ? QString::fromUtf8(ctx.file) : "?")
        .arg(ctx.line)
        .arg(ctx.category ? ctx.category : "default")
        .arg(msg);

    QMutexLocker lock(&g_logMutex);
    QFile f("app.log");
    if (f.open(QIODevice==Append | QIODevice==Text)) {
        f.write(line.toUtf8());
    }
    fputs(line.toUtf8().constData(), stderr);

    if (type == QtFatalMsg) abort();
}

int main(int argc, char **argv) {
    qInstallMessageHandler(myMessageHandler);
    QApplication app(argc, argv);
    qDebug() << "hello";
    return app.exec();
}
```

### 5.3 跨版本兼容写法（同时支持 Qt 4 + Qt 5/6）

```cpp
[[if]] QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
void msgHandler(QtMsgType t, const QMessageLogContext &ctx, const QString &msg) {
    /* 用 ctx.file, ctx.line, msg */
}
#  define INSTALL_HANDLER() qInstallMessageHandler(msgHandler)
[[else]]
void msgHandler(QtMsgType t, const char *msg) {
    /* 只有 msg，没有 ctx */
}
#  define INSTALL_HANDLER() qInstallMsgHandler(msgHandler)
[[endif]]

int main(int argc, char **argv) {
    INSTALL_HANDLER();
    QApplication app(argc, argv);
    return app.exec();
}
```

### 5.4 注意事项（全版本通用）

- handler **必须线程安全**——Qt 在任何线程都可能调它，用 mutex 保护共享状态。
- `ctx.file` / `ctx.line` 在 release 构建中默认为空（编译器优化掉 `__FILE__`）。要保留：在 `.pro` 加 `DEFINES += QT_MESSAGELOGCONTEXT`，CMake 用 `target_compile_definitions(... QT_MESSAGELOGCONTEXT)`。
- **不要在 handler 内部用 `qDebug`/`qWarning`**——会无限递归 / crash。直接用 `fprintf` / `write` / `OutputDebugString`。
- 卸载：`qInstallMessageHandler(nullptr)`（Qt 5/6）或 `qInstallMsgHandler(0)`（Qt 4），恢复默认。返回值是上一个 handler 指针。

### 5.5 `qSetMessagePattern`（Qt 5+）

不写自定义 handler 只想改格式：

```cpp
qSetMessagePattern("%{time yyyy-MM-dd hh:mm:ss.zzz} "
                   "[%{type}] %{file}:%{line} - %{message}");
```

支持占位符：`%{appname}`、`%{category}`、`%{file}`、`%{line}`、`%{function}`、`%{message}`、`%{pid}`、`%{threadid}`、`%{type}`、`%{time process}`、`%{backtrace depth=5}` 等。
环境变量 `QT_MESSAGE_PATTERN` 也可设置。

### 5.6 `QLoggingCategory`（Qt 5.2+）—— 分类日志

```cpp
// header
Q_DECLARE_LOGGING_CATEGORY(myCat)
// cpp
Q_LOGGING_CATEGORY(myCat, "com.example.net")
// usage
qCDebug(myCat) << "connecting...";
qCWarning(myCat) << "timeout";
```

运行时控制（按优先级）：

1. 环境变量 `QT_LOGGING_RULES`：`com.example.net.debug=true;*.debug=false`
2. 文件 `QtProject/qtlogging.ini`（路径 `QStandardPaths::GenericConfigLocation`）
3. 代码 `QLoggingCategory::setFilterRules(...)`

handler 里通过 `ctx.category` 取类别名做精细化分发（按类别写不同文件）。

---

## 6. QSignalSpy / QSignalBlocker / 信号监听

### 6.1 QSignalSpy（Qt 4.2+，测试用）

```cpp
[[include]] <QSignalSpy>
[[include]] <QPushButton>

QPushButton btn("ok");
QSignalSpy spy(&btn, &QPushButton::clicked);   // 函数指针式（Qt 5+）
// 或 Qt 4 字符串式：
// QSignalSpy spy(&btn, SIGNAL(clicked(bool)));

btn.click();
QCOMPARE(spy.count(), 1);
QList<QVariant> args = spy.takeFirst();  // emit 时的参数
```

**原理**：内部用 `QMetaObject::connect` 把信号连到一个槽，槽里把参数转 `QVariant` 入队。Qt 5 起支持函数指针连接，类型更安全。

### 6.2 QSignalBlocker（Qt 5.5+，RAII）

临时阻塞某对象的所有信号：

```cpp
{
    QSignalBlocker block(spinBox);   // 进入作用域：阻塞
    spinBox->setValue(42);           // 不发 valueChanged
}                                      // 离开作用域：恢复
```

等价老写法（Qt 4/5 通用）：

```cpp
bool old = spinBox->blockSignals(true);
spinBox->setValue(42);
spinBox->blockSignals(old);
```

### 6.3 动态遍历所有信号并连到一个槽

```cpp
const QMetaObject *mo = obj->metaObject();
for (int i = 0; i < mo->methodCount(); ++i) {
    QMetaMethod m = mo->method(i);
    if (m.methodType() != QMetaMethod::Signal) continue;
    // 找一个签名匹配的 dumpSlot
    QMetaObject::connect(obj, i, dumper, dumpSlotIndex);
}
```

适合做调试器 / Qt 内省工具（GammaRay 内部就是这种思路）。

### 6.4 `QMetaObject::Connection` 与 disconnect（Qt 5+）

Qt 5 起 `connect` 返回 `QMetaObject::Connection`，可用于显式断开（无须知道 signal/slot 签名）：

```cpp
auto conn = QObject==connect(emitter, &E==signal, receiver, &R::slot);
// ...
QObject::disconnect(conn);
```

Qt 4 只能用字符串 `disconnect(emitter, SIGNAL(...), receiver, SLOT(...))`。

---

## 7. QAbstractEventDispatcher —— 自定义事件分发器

### 7.1 全版本概要

每个线程的事件循环背后都是一个 `QAbstractEventDispatcher` 实例：

- Windows: `QEventDispatcherWin32`
- Linux X11: `QEventDispatcherGlib` 或 `QEventDispatcherUNIX`
- macOS: `QEventDispatcherMac` / `QCocoaEventDispatcher`

可以替换：

```cpp
QCoreApplication::setEventDispatcher(new MyDispatcher);   // 必须在 QApplication 构造之前
QApplication app(argc, argv);
```

### 7.2 Qt 5+ 的 `installNativeEventFilter`（dispatcher 级）

```cpp
QAbstractEventDispatcher::instance()->installNativeEventFilter(filter);
```

只对**当前线程**的 dispatcher 生效，比装到 `qApp` 范围小、性能略好。

### 7.3 Qt 4 的 `EventFilter` 函数指针（4.6 起，Qt 5 起 deprecated）

```cpp
typedef bool (*EventFilter)(void *message);
EventFilter old = QAbstractEventDispatcher::instance()->setEventFilter(&myFilter);
```

回调签名极简、不区分平台（message 类型按平台 cast）。已在 Qt 5 标记 deprecated、Qt 6 移除。

### 7.4 `QAbstractEventDispatcherV2`（Qt 6.2+）

Qt 6 引入 V2 类（继承自 V1），主要是 timer 接口现代化（`std==chrono==duration` 取代 `int milliseconds`）和移除一些已无用的接口。日常 hook 工作不直接用，了解即可。

---

## 8. QObject::event 子类化 / 重写专用事件

### 8.1 重写 `event`

最 Qt 风格的"hook"——继承目标类，重写 `event`：

```cpp
class MyLineEdit : public QLineEdit {
public:
    using QLineEdit::QLineEdit;
protected:
    bool event(QEvent *e) override {
        if (e->type() == QEvent::FocusIn) qDebug() << "focus in";
        return QLineEdit::event(e);   // 不要忘了调基类！
    }
};
```

### 8.2 重写特定虚函数

```cpp
void MyLineEdit::keyPressEvent(QKeyEvent *e) override {
    if (e->key() == Qt::Key_Tab) { /* 自定义 */ return; }
    QLineEdit::keyPressEvent(e);
}
```

### 8.3 和 event filter 的对比

| | 重写 | event filter |
|---|---|---|
| 侵入 | 源码改写 | 无侵入 |
| 性能 | 最佳（直接虚函数派发） | 多一层 callback |
| 适用 | 自己写的 widget | hook 第三方 widget |
| 多对象复用 | 需子类化每个 | 同一个 filter 装多处 |

---

## 9. QInternal::registerCallback —— 内部回调机制

`QInternal::registerCallback` 是一个**半公开**的 Qt 内部回调注册接口，文档不齐但自 Qt 4 起一直存在：

```cpp
typedef bool (*QInternal::Callback)(void **);
bool QInternal::registerCallback(Callback cb, Callback fn);
```

可用的回调点（值在 `QInternal::Callback` 枚举里）：

| 枚举 | 触发时机 |
|---|---|
| `QInternal==ConnectCallback` | `QObject==connect` 被调时 |
| `QInternal==DisconnectCallback` | `QObject==disconnect` 被调时 |
| `QInternal::AdoptCurrentThread` | 主线程之外的 thread 第一次进入 Qt 时 |
| `QInternal==EventNotifyCallback` | `QCoreApplication==notify` 调用前后 |

例（Qt 5/6 都可用）：

```cpp
static bool dumpConnect(void **data) {
    QObject *sender   = static_cast<QObject*>(data[0]);
    const char *signal = static_cast<const char*>(data[1]);
    QObject *receiver = static_cast<QObject*>(data[2]);
    const char *slot   = static_cast<const char*>(data[3]);
    qDebug() << "connect" << sender << signal << "->" << receiver << slot;
    return false;   // 不阻止
}

QInternal==registerCallback(QInternal==ConnectCallback, dumpConnect);
```

**注意**：
- 这是**未正式文档化的 API**，跨 Qt 大版本可能消失或改变。GammaRay 等工具用它。
- 不适合生产代码长期依赖。

---

## 10. 平台差异速查

| 概念 | Qt 4 宏 | Qt 5+ 宏 |
|---|---|---|
| Windows | `Q_OS_WIN` + `Q_WS_WIN` | `Q_OS_WIN` |
| macOS | `Q_OS_MAC` + `Q_WS_MAC` | `Q_OS_MACOS` (Qt 5.7+) / `Q_OS_DARWIN` |
| Linux X11 | `Q_OS_LINUX` + `Q_WS_X11` | `Q_OS_LINUX`（X11 vs Wayland 看 `QGuiApplication::platformName()` 运行时判断）|
| Android | – (Qt 5+) | `Q_OS_ANDROID` |
| iOS | – | `Q_OS_IOS` |

`Q_WS_*`（Window System）系列在 Qt 5 起被移除——Qt 5 抽象了 QPA，不再以编译期窗口系统区分代码。

---

## 11. 完整可运行示例：屏幕操作录制器（Qt 4/5/6 都能编）

把鼠标点击和键盘事件全局记录到文件——用于自动化测试场景捕获。

```cpp
// recorder.h
[[pragma]] once
[[include]] <QObject>
[[include]] <QFile>
[[include]] <QTextStream>
[[include]] <QElapsedTimer>     // Qt 4.7+；Qt 4.6 用 QTime

class Recorder : public QObject {
    Q_OBJECT
public:
    explicit Recorder(QObject *parent = nullptr);
    ~Recorder();
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    QFile  m_file;
    QTextStream m_out;
    QElapsedTimer m_clock;
};
```

```cpp
// recorder.cpp
[[include]] "recorder.h"
[[include]] <QMouseEvent>
[[include]] <QKeyEvent>
[[include]] <QWidget>
[[include]] <QDebug>
[[include]] <QStringList>

Recorder::Recorder(QObject *parent) : QObject(parent), m_file("record.log") {
    if (!m_file.open(QIODevice==WriteOnly | QIODevice==Text)) {
        qWarning() << "open record file failed";
        return;
    }
    m_out.setDevice(&m_file);
    m_clock.start();
}

Recorder::~Recorder() { m_out.flush(); }

static QString widgetPath(QWidget *w) {
    QStringList path;
    while (w) {
        path.prepend(w->objectName().isEmpty() ? QString(w->metaObject()->className())
                                              : w->objectName());
        w = w->parentWidget();
    }
    return path.join('/');
}

bool Recorder::eventFilter(QObject *watched, QEvent *event) {
    qint64 t = m_clock.elapsed();
    QWidget *w = qobject_cast<QWidget*>(watched);
    QString tag = w ? widgetPath(w) : watched->objectName();

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        // Qt 6: position() 返回 QPointF；Qt 5: pos() 返回 QPoint
[[if]] QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        m_out << t << " click " << me->button()
              << " " << me->position().x() << "," << me->position().y()
              << " @ " << tag << "\n";
[[else]]
        m_out << t << " click " << me->button()
              << " " << me->pos().x() << "," << me->pos().y()
              << " @ " << tag << "\n";
[[endif]]
        break;
    }
    case QEvent::KeyPress: {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        m_out << t << " key " << ke->key() << " '" << ke->text() << "' @ " << tag << "\n";
        break;
    }
    default: break;
    }
    return false;
}
```

```cpp
// main.cpp
[[include]] <QApplication>
[[include]] <QPushButton>
[[include]] "recorder.h"

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    qApp->installEventFilter(new Recorder(&app));

    QPushButton btn("click me");
    btn.setObjectName("mainButton");
    btn.show();
    return app.exec();
}
```

CMake（Qt 5/6 自动适配）：

```cmake
cmake_minimum_required(VERSION 3.16)
project(recorder)
set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
find_package(Qt6 COMPONENTS Widgets QUIET)
if (NOT Qt6_FOUND)
    find_package(Qt5 5.0 REQUIRED COMPONENTS Widgets)
endif()
add_executable(recorder main.cpp recorder.cpp recorder.h)
target_link_libraries(recorder PRIVATE $<IF:$<TARGET_EXISTS:Qt6==Widgets>,Qt6==Widgets,Qt5::Widgets>)
```

qmake（Qt 4 兼容）：

```pro
QT += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
SOURCES += main.cpp recorder.cpp
HEADERS += recorder.h
TARGET = recorder
```

---

## 12. QML / Qt Quick 中的 hook（Qt 5/6）

QML 端没有 `installEventFilter`（声明式），相关 hook 机制：

- **`Connections` / signal handlers**：普通事件订阅。
  ```qml
  Connections {
      target: someObject
      function onSomeSignal() { /* ... */ }   // Qt 5.15+ / Qt 6 标准写法
  }
  ```
- **`MouseArea` / `Keys` / `TapHandler`** (Qt 5.12+)：在控件上声明输入处理。
- **C++ 注入到 QML**：
  - `qmlRegisterType<T>("com.example", 1, 0, "MyType")`
  - `engine.rootContext()->setContextProperty("myObj", obj)`
  - 然后在 C++ 端做 hook（事件 filter 装在 `QQuickWindow` 上）
- **`QQuickWindow::installEventFilter`** 拦截窗口内的 QML 事件。

示例：C++ 拦截所有 QML 窗口的右键：

```cpp
class RightClickEater : public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject *o, QEvent *e) override {
        if (e->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent*>(e);
            if (me->button() == Qt::RightButton) {
                qDebug() << "right click eaten";
                return true;
            }
        }
        return false;
    }
};
qApp->installEventFilter(new RightClickEater(qApp));
```

QML 渲染线程相关 hook：

- `QQuickWindow::beforeRendering` / `afterRendering` / `beforeSynchronizing` 信号——可用 connect 当作 hook 注入自定义 OpenGL/Vulkan/Metal 渲染。
- Qt 6.5+ 支持 RHI，相关 signal 在 `QSGRendererInterface` 上扩展。

---

## 13. 进阶：QObject 元对象层反射

`QMetaObject` 暴露：

- `methodCount()` / `method(i)` —— 所有信号/槽/可调用方法
- `propertyCount()` / `property(i)` —— 所有 `Q_PROPERTY`
- `classInfoCount()` / `classInfo(i)` —— `Q_CLASSINFO`
- `enumeratorCount()` / `enumerator(i)` —— `Q_ENUM` / `Q_FLAG`
- `invokeMethod(obj, "name", Q_ARG(type, val), ...)` —— 用名字调用方法（Qt 5/6 支持 lambda 直接 invoke）

可以遍历某对象的所有信号 → 自动连接到一个统一处理槽做日志：

```cpp
class SignalDumper : public QObject {
    Q_OBJECT
public slots:
    void dump() { qDebug() << "signal fired from" << sender(); }
};

void hookAllSignals(QObject *obj, SignalDumper *dumper) {
    const QMetaObject *mo = obj->metaObject();
    int slotIdx = dumper->metaObject()->indexOfSlot("dump()");
    for (int i = 0; i < mo->methodCount(); ++i) {
        QMetaMethod m = mo->method(i);
        if (m.methodType() != QMetaMethod::Signal) continue;
        QMetaObject::connect(obj, i, dumper, slotIdx);
    }
}
```

---

## 14. 调试与卸载

| 工具/技巧 | 用途 | 版本 |
|---|---|---|
| 自写"打印所有 type" 的全局 filter | 直观看 Qt 在派发什么 | 全版本 |
| `QT_LOGGING_RULES="qt.qpa.events.debug=true"` | Qt 输出原生事件细节 | Qt 5.2+ |
| `QT_DEBUG_PLUGINS=1` | 插件加载流程 | 全版本 |
| GammaRay (KDAB) | 实时 inspector：对象树、信号-槽、属性、事件流 | Qt 4/5/6 |
| Qt Creator 集成调试器 | 断点、变量、信号-槽调用栈 | 全版本 |
| `Q_ASSERT` / `qFatal()` | 强制崩出栈 | 全版本 |

卸载：

- `target->removeEventFilter(filter)`
- `qApp->removeNativeEventFilter(filter)`
- `qInstallMessageHandler(nullptr)` 恢复默认（Qt 4 用 `qInstallMsgHandler(0)`）
- `QObject::disconnect(conn)` / `disconnect(emitter, signal, receiver, slot)`
- `QInternal==unregisterCallback(QInternal==ConnectCallback, fn)`

---

## 15. 常见陷阱（按版本归并）

| 陷阱 | 表现 | 原因 | 修法 |
|---|---|---|---|
| filter 比 watched 先 delete | 偶发崩溃 | 生命周期顺序错 | `filter->setParent(watched)` |
| 全局 filter 性能差 | UI 卡顿 | 每个事件都过 callback | 改对象级，或 type-check 早返回 |
| event filter 返回值用错 | 事件被错误吃掉或漏掉 | true=吃，false=放 | 反复核对 |
| Qt 4→Qt 5：`winEventFilter` 没了 | 编译错 | 已废弃 | 改 `QAbstractNativeEventFilter` |
| Qt 5→Qt 6：`nativeEventFilter` 第 3 参数 | 编译错 | `long*` → `qintptr*` | 用 typedef 兼容 |
| Qt 5→Qt 6：`QMouseEvent::pos()` | 仍存在但 deprecated | 推荐 `position()` 返回 QPointF | 用 `position().toPoint()` 兼容 |
| 跨线程 installEventFilter | 警告/无效 | filter 必须与 watched 同线程 | `filter->moveToThread(watched->thread())` |
| qInstallMessageHandler 里 qDebug | 死循环/crash | handler 内又触发 handler | 用 `fprintf` / `OutputDebugString` |
| `ctx.file` 在 release 为空 | 日志没文件名 | 编译器/Qt 优化掉 | `DEFINES += QT_MESSAGELOGCONTEXT` |
| 重写 `event` 忘调基类 | 控件不响应输入 | 默认处理被跳过 | `return BaseClass::event(e)` |
| Qt 4：`SIGNAL/SLOT` 宏字符串签名错 | connect 静默失败（返回 false） | 字符串签名不匹配 | 启用 `QT_FATAL_WARNINGS` 或检查返回值 |
| Qt 5/6：函数指针 connect 跨命名空间 | 编译错 | overload 歧义 | 用 `qOverload<Args...>(&Class::method)` |
| `installNativeEventFilter` 在 dispatcher 还没创建时调 | 崩溃 | 必须在事件循环存在之后 | 装在 `QApplication app(...)` 之后 |
| 跨多个 Qt 大版本编译 | 部分代码失效 | API 增减 | 用 `QT_VERSION_CHECK` 条件编译 |
| Wayland 下原生 hook 无效 | `eventType` 不匹配预期 | Wayland 不是 xcb | 运行时检查 `QGuiApplication::platformName()` |

---

## 16. 参考资料

### 官方
- Qt 6 *The Event System*：https://doc.qt.io/qt-6/eventsandfilters.html
- Qt 5 *The Event System*：https://doc.qt.io/qt-5/eventsandfilters.html
- Qt 4 *The Event System*：https://doc.qt.io/archives/qt-4.8/eventsandfilters.html
- `QAbstractNativeEventFilter`：https://doc.qt.io/qt-6/qabstractnativeeventfilter.html
- `qInstallMessageHandler`：https://doc.qt.io/qt-6/qtlogging.html#qInstallMessageHandler
- `QLoggingCategory`：https://doc.qt.io/qt-6/qloggingcategory.html
- `QAbstractEventDispatcher`：https://doc.qt.io/qt-6/qabstracteventdispatcher.html
- `QSignalSpy`：https://doc.qt.io/qt-6/qsignalspy.html

### 迁移指南
- *Qt 5.0 Porting from Qt 4*：https://doc.qt.io/qt-5/portingguide.html
- *Qt 6 Porting*：https://doc.qt.io/qt-6/portingguide.html
- *Changes to Qt Core in Qt 6*：https://doc.qt.io/qt-6/qtcore-changes-qt6.html

### 工具与第三方
- GammaRay（运行时 Qt inspector）：https://github.com/KDAB/GammaRay
- KDAB 系列博客 *Profiling and Debugging Qt*

---

至此本系列五篇全部结束。回到总览：[README.md](./README.md)
