#include "stdafx.h"
#include "Application.h"

int main(int argc, char** argv) {
    try {
        Application app(L"Point Cloud Viewer", 1280, 720);
        app.run();
    }
    catch (const std::exception& e) {
        ATL::CA2T msg(e.what());
        MessageBox(NULL, msg, L"Error", MB_ICONERROR);
        return -1;
    }

    return 0;
}
