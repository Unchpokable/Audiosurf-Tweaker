#ifndef WNDPROC_HANDLER_H
#define WNDPROC_HANDLER_H

#ifdef _WIN32
#include <Windows.h>
#endif

#ifdef _WIN32

#include "Backend_global.h"

class BACKEND_EXPORT CopyDataWrapper
{
public:
    CopyDataWrapper(const COPYDATASTRUCT* cds);

    inline ULONG_PTR data_id() const
    {
        return m_data_id;
    }

    inline std::size_t size() const
    {
        return m_data.size();
    }

    inline const std::uint8_t* data() const
    {
        return m_data.data();
    }

private:
    ULONG_PTR m_data_id;
    std::vector<std::uint8_t> m_data;
};

#endif

class BACKEND_EXPORT WndProcHandler : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit WndProcHandler(QObject* parent = nullptr);

    virtual bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;
#ifdef _WIN32
    void send_copy_data(HWND hwnd, const std::vector<std::uint8_t>& cds_data);
#endif

signals:
    void onWmCopyData(const CopyDataWrapper& cds);
};

Q_DECLARE_METATYPE(WndProcHandler);
Q_DECLARE_METATYPE(WndProcHandler*);

#endif // WNDPROC_HANDLER_H
