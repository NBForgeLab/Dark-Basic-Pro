#include "Controller.h"
#include <cstring>

CController::CController()
{
    bHasForceFeedback = false;
    sName[0] = 0;
    HasX = true;
    HasY = true;
    HasZ = true;
    HasRx = true;
    HasRy = true;
    HasRz = true;
    SliderCount = 0;
    ButtonCount = 10;
    PovCount = 1;
    Deadzone = 200;
}

CController::~CController()
{
}

void CController::Release()
{
}

void CController::SetName(char* sNewName)
{
    if (sNewName) {
        strncpy_s(sName, sizeof(sName), sNewName, _TRUNCATE);
    }
}
