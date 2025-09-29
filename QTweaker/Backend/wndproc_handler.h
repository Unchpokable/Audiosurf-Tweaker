#ifndef WNDPROC_HANDLER_H
#define WNDPROC_HANDLER_H

#ifdef _WIN32
#include <Windows.h>
#endif

class WndProcHandler : public QAbstractNativeEventFilter, public QObject
{
    Q_OBJECT

public:
    virtual bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

signals:
#ifdef _WIN32
    void onWmCopyData(const COPYDATASTRUCT* cds);
#endif
};

#endif // WNDPROC_HANDLER_H
