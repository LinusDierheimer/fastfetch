#include "fastfetch.h"

#include <dirent.h>

static uint32_t get_num_dirs(const char* dirname) {
    uint32_t num_dirs = 0;
    DIR * dirp;
    struct dirent *entry;

    dirp = opendir(dirname);
    if(dirp == NULL)
        return 0;

    while((entry = readdir(dirp)) != NULL) {
        if(entry->d_type == DT_DIR)
            ++num_dirs;
    }
   
    num_dirs -= 2; // accounting for . and ..

    closedir(dirp);

    return num_dirs;
}

void ffPrintPackages(FFinstance* instance)
{
    uint32_t pacman = get_num_dirs("/var/lib/pacman/local");
    uint32_t flatpak = get_num_dirs("/var/lib/flatpak/app");

    uint32_t all = pacman + flatpak;

    if(all == 0)
    {
        ffPrintError(instance, "Packages", "No packages from known package managers found");
        return;
    }

    if(ffStrbufIsEmpty(&instance->config.packagesFormat))
    {
        ffPrintLogoAndKey(instance, "Packages");

        #define FF_PRINT_PACKAGE(name) \
        if(name > 0) \
        { \
            printf("%u ("#name")", name); \
            if((all = all - name) > 0) \
                printf(", "); \
        };

        FF_PRINT_PACKAGE(pacman)
        FF_PRINT_PACKAGE(flatpak)

        #undef FF_PRINT_PACKAGE

        putchar('\n');
        return;
    }

    FF_STRBUF_CREATE(packages);

    ffParseFormatString(&packages, &instance->config.packagesFormat, 3,
        (FFformatarg){FF_FORMAT_ARG_TYPE_UINT, &all},
        (FFformatarg){FF_FORMAT_ARG_TYPE_UINT, &pacman},
        (FFformatarg){FF_FORMAT_ARG_TYPE_UINT, &flatpak}   
    );

    ffPrintLogoAndKey(instance, "Packages");
    ffStrbufWriteTo(&packages, stdout);
    ffStrbufDestroy(&packages);
}