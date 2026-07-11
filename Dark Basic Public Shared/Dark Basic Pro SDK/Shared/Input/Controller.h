#pragma once

#include <windows.h>

class CController
{
public:
    CController();
    ~CController();

    void Release();
    void SetForceFeedback() { bHasForceFeedback = true; }
    bool HasForceFeedback() const { return bHasForceFeedback; }

    void SetName(char* sNewName);
    const char* Name() const { return sName; }

    bool HasX, HasY, HasZ, HasRx, HasRy, HasRz;
    int  SliderCount;
    int  ButtonCount;
    int  PovCount;
    int  Deadzone;

    operator const void *() const { return reinterpret_cast<const void*>(this); }
private:
    bool                    bHasForceFeedback;
    char                    sName[256];
};
