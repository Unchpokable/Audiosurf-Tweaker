#include "window/native_window.hxx"

int main(int argc, char** argv)
{
    as::wnd::initialize(L"ASBRIDGE_PROXYWND_ASDGBSA12455");
    as::wnd::shutdown();
    return 0;
}
