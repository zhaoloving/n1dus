#pragma once

#include <common.h>

namespace NXFramework
{
std::string FixedWidth(int value, int width);
char*       ClockGetCurrentTime(void);
bool        GetBatteryStatus(ChargerType& state, u32& percent);

}
