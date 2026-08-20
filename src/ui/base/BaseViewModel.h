#pragma once

#include <atomic>
#include <functional>
#include <QCoreApplication>
#include <QObject>
#include <QThread>
#include <type_traits>

/**
 * @brief ViewModel 基类模板，支持单向数据流 (UDF)
 *
 * 【架构规范】：
 * 1. 状态 (State) 负责持久化 UI 数据（如输入内容、表单常驻的 `errorMessage`），具有粘性；
 * 2. 一次性事件（如弹 Toast、页面跳转、瞬态成功提示等）切勿塞入 State，请使用独立的 Qt Signal 派发。
 */
template<typename Derived, typename State>
class BaseViewModel : public QObject {
public:
    explicit BaseViewModel(QObject *parent = nullptr) : QObject(parent) {
    }

    virtual ~BaseViewModel() = default;

    /**
     * @brief 获取当前不可变状态
     */
    const State &state() const { return m_state; }

    /**
     * @brief 发起新请求：递增令牌，使所有旧请求失效
     */
    quint64 beginRequest() { return ++m_requestId; }

    /**
     * @brief 回调安全检查：过期返回 false
     */
    bool isRequestCurrent(quint64 requestId) const {
        return requestId == m_requestId;
    }

    /**
     * @brief 使所有在途请求失效
     */
    void invalidateRequests() { ++m_requestId; }

    /**
     * @brief 观察状态：支持生命周期绑定、防重复订阅与粘性回放
     */
    template<typename Context, typename Slot>
    void observe(Context *context, Slot slot) {
        Q_ASSERT_X(QThread::currentThread() == qApp->thread(),
                   "ViewModel", "observe must be called on the main thread!");

        connect(static_cast<Derived *>(this), &Derived::stateChanged, context, slot,
                static_cast<Qt::ConnectionType>(Qt::AutoConnection | Qt::UniqueConnection));

        if constexpr (std::is_member_function_pointer_v<Slot>) {
            std::invoke(slot, context, m_state);
        } else {
            std::invoke(slot, m_state);
        }
    }

protected:
    State m_state;
    std::atomic<quint64> m_requestId{0};

    /**
     * @brief 局部更新状态并触发 UI 渲染
     * @param updater 状态更新函数，传入可修改的 State 引用
     */
    void updateState(const std::function<void(State &)> &updater) {
        Q_ASSERT_X(QThread::currentThread() == qApp->thread(),
                   "ViewModel", "updateState must be called on the main thread!");
        State next = m_state;
        updater(next);
        if (next == m_state) return; // 状态未变，跳过更新 (distinctUntilChanged)
        m_state = std::move(next);
        emitStateChanged();
    }

    /**
     * @brief 全量覆盖状态并触发 UI 渲染
     * @param newState 新的完整状态
     */
    void updateState(const State &newState) {
        Q_ASSERT_X(QThread::currentThread() == qApp->thread(),
                   "ViewModel", "updateState must be called on the main thread!");
        if (newState == m_state) return;
        m_state = newState;
        emitStateChanged();
    }

    /**
     * @brief 子类必须实现此方法以发射自定义的 stateChanged(State) 信号
     */
    virtual void emitStateChanged() = 0;
};