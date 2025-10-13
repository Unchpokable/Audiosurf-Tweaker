#include "precompiled.hxx"

#include "wndproc_handler.h"

#ifdef _WIN32

CopyDataWrapper::CopyDataWrapper(const COPYDATASTRUCT* cds)
{
    m_data_id = cds->dwData;

    m_data = std::vector(static_cast<uint8_t*>(cds->lpData), static_cast<uint8_t*>(cds->lpData) + cds->cbData);
}

#endif

WndProcHandler::WndProcHandler(QObject* parent) : QObject(parent)
{
}

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

#ifdef _WIN32
void WndProcHandler::send_copy_data(HWND hwnd, const std::vector<uint8_t>& cds_data)
{
    COPYDATASTRUCT cds;
    cds.cbData = cds_data.size();
    cds.lpData = (PVOID)cds_data.data();
    SendMessage(hwnd, WM_COPYDATA, (WPARAM)hwnd, (LPARAM)&cds);
}
#endif
