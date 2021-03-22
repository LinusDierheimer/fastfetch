#include "fastfetch.h"

void ffPrintUptime(FFinstance* instance)
{

    uint32_t days    =  instance->state.sysinfo.uptime / 86400;
	uint32_t hours   = (instance->state.sysinfo.uptime - (days * 86400)) / 3600; 
    uint32_t minutes = (instance->state.sysinfo.uptime - (days * 86400) - (hours * 3600)) / 60;
    uint32_t seconds =  instance->state.sysinfo.uptime - (days * 86400) - (hours * 3600) - (minutes * 60);

    if(ffStrbufIsEmpty(&instance->config.uptimeFormat))
    {
        ffPrintLogoAndKey(instance, "Uptime");

        if(days == 0 && hours == 0 && minutes == 0)
        {
            printf("%u seconds\n", seconds);
        }
        else
        {
            if(days > 0)
                printf("%u day%s, ", days, days <= 1 ? "" : "s");
            if(hours > 0)
                printf("%u hour%s, ", hours, hours <= 1 ? "" : "s");
            if(minutes > 0)
                printf("%u min%s", minutes, minutes <= 1 ? "" : "s");
            putchar('\n');
        }
    }
    else
    {
        FF_STRBUF_CREATE(uptime);

        ffParseFormatString(&uptime, &instance->config.uptimeFormat, 4,
            (FFformatarg){FF_FORMAT_ARG_TYPE_UINT, &days},
            (FFformatarg){FF_FORMAT_ARG_TYPE_UINT, &hours},
            (FFformatarg){FF_FORMAT_ARG_TYPE_UINT, &minutes},
            (FFformatarg){FF_FORMAT_ARG_TYPE_UINT, &seconds}
        );

        ffPrintLogoAndKey(instance, "Uptime");
        ffStrbufWriteTo(&uptime, stdout);
        ffStrbufDestroy(&uptime);
    }
}