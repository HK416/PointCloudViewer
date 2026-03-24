#include "stdafx.h"
#include "Application.h"

int main(int argc, char** argv) {
    std::unique_ptr<Application> app;
    try {
        app = std::make_unique<Application>(L"Point Cloud Viewer", 1280, 720);
    }
    catch (const std::exception& e) {
        ATL::CA2T msg(e.what());
        MessageBox(NULL, msg, L"Initialize Error", MB_ICONERROR);
        return -1;
    }

    try {
        app->run();
    } catch (const std::exception& e) {
        ATL::CA2T msg(e.what());
        MessageBox(NULL, msg, L"Runtime Error", MB_ICONERROR);
        return -1;
    }

    return 0;
}
