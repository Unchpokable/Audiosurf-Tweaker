#include "wndproc_handler.h"

bool WndProcHandler::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef _WIN32
    if(eventType == "windows_generic_MSG") {
        MSG* msg = static_cast<MSG*>(message);
        if(msg->message == WM_COPYDATA) {
            auto cds = reinterpret_cast<COPYDATASTRUCT*>(msg->lParam);

            emit onWmCopyData(cds);

            *result = TRUE;
            return true;
        }
    }
#endif
    return false;
}
