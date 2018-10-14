#include "nxUtils.h"
#include <time.h>

namespace NXFramework
{

std::string FixedWidth(int value, int width)
{
	char buffer[10];
	snprintf(buffer, sizeof(buffer), "%.*d", width, value);
	return buffer;
}

char* ClockGetCurrentTime(void)
{
	static char buffer[10];

    time_t unixTime = time(NULL);
	struct tm* timeStruct = localtime((const time_t *)&unixTime);
	int hours   = (timeStruct->tm_hour);
	int minutes = timeStruct->tm_min;

	bool amOrPm = false;
	if (hours < 12)		amOrPm = true;
	if (hours == 0)		hours  = 12;
	else
    if (hours > 12)		hours  = hours - 12;

	snprintf(buffer, 10, "%2i:%02i %s", hours, minutes, amOrPm ? "AM" : "PM");
	return buffer;
}

bool GetBatteryStatus(ChargerType& state, u32& percent)
{
	if (R_FAILED(psmGetChargerType(&state)))
    {
        LOG("psmGetChargerType failed...\n");
		state = (ChargerType)0;
    }
	if (R_FAILED(psmGetBatteryChargePercentage(&percent)))
	{
        LOG("psmGetBatteryChargePercentage failed...\n");
        return false;
	}
	return true;
}

}
