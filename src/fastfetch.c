#include "fastfetch.h"
#include "fastfetch_config.h"
#include "util/FFvaluestore.h"

#include <malloc.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#define FASTFETCH_DEFAULT_STRUCTURE "Title:Seperator:OS:Host:Kernel:Uptime:Packages:Shell:Resolution:DE:WM:Theme:Icons:Font:Terminal:TerminalFont:CPU:GPU:Memory:Disk:Battery:Locale:Break:Colors"

#define FASTFETCH_DEFAULT_CONFIG \
    "# Fastfetch configuration\n" \
    "# Put arguments here to make them permanently (one per line). \n" \
    "# Direct arguments will overwrite the corresponding ones in this files\n" \
    "# Each line is whitespace trimmed on beginn and end.\n" \
    "# Empty lines or lines starting with # are ignored.\n" \
    "# There are more arguments possible than listed here, take a look at fastfetch --help!\n" \
    "# This version of the file was shipped with "FASTFETCH_PROJECT_VERSION".\n" \
    "# Use fastfetch --print-default-config > ~/.config/fastfetch/config.conf to generate a new one with current defaults.\n"

//Things only needed by fastfetch
typedef struct FFdata
{
    FFvaluestore valuestore;
    FFstrbuf structure;
    FFstrbuf logoName;
} FFdata;

static inline void printHelp()
{
    puts(
        "Usage: fastfetch <options>\n"
        "\n"
        "Informative options:\n"
        "   -h,           --help:                 shows this message and exits\n"
        "   -h <command>, --help <command>:       shows help for a specific command and exits\n"
        "   -v            --version:              prints the version of fastfetch and exits\n"
        "                 --list-logos:           list available logos and exits\n"
        "                 --print-logos:          shows available logos and exits\n"
        "                 --print-default-config: prints the default config and exits\n"
        "\n"
        "General options:\n"
        "                --structure <structure>:         sets the structure of the fetch. Must be a colon seperated list of keys\n"
        "                --set <key=value>:               hard set the value of an key\n"
        "   -c <color>,  --color <color>:                 sets the color of the keys. Must be a linux console color code (+)\n"
        "                --spacing <width>:               sets the distance between logo and text\n"
        "   -s <str>,    --seperator <str>:               sets the seperator between key and value. Default is a colon with a space\n"
        "   -x <offset>, --offsetx <offset>:              sets the x offset. Can be negative to cut the logo, but no more than logo width.\n"
        "                --show-errors <?value>:          print occuring errors\n"
        "   -r <?value>  --recache <?value>:              if set to true, no cached values will be used\n"
        "                --print-remaining-logo <?value>: print the remaining logo, if it is higher than the number of lines shown\n"
        "\n"
        "Logo options:\n"
        "   -l <name>, --logo <name>:         sets the shown logo. Also changes the main color accordingly\n"
        "              --color-logo <?value>: if set to false, the logo will be black / white\n"
        "\n"
        "Format options: Provide the format string for custom output (+)\n"
        "   --os-format <format>\n"
        "   --host-format <format>\n"
        "   --kernel-format <format>\n"
        "   --uptime-format <format>\n"
        "   --packages-format <format>\n"
        "   --shell-format <format>\n"
        "   --resolution-format <format>\n"
        "   --de-format <format>\n"
        "   --wm-format <format>\n"
        "   --theme-format <format>\n"
        "   --icons-format <format>\n"
        "   --font-format <format>\n"
        "   --terminal-format <format>\n"
        "   --terminal-font-format <format>\n"
        "   --cpu-format <format>\n"
        "   --gpu-format <format>\n"
        "   --memory-format <format>\n"
        "   --disk-format <format>\n"
        "   --battery-format <format>\n"
        "   --locale-format <format>\n"
        "\n"
        "Library optins: Set the path of a library to load\n"
        "   --lib-PCI <path>\n"
        "   --lib-X11 <path>\n"
        "   --lib-Xrandr <path>\n"
        "\n"
        "If an value starts with an ?, it is optional. \"true\" will be used if not set.\n"
        "An (+) at the end indicates that more help can be printed with --help <option>\n"
        "All options can be make permanent in $XDG_CONFIG_HOME/fastfetch/config.conf"
    );
}

static inline void printCommandHelpColor()
{
    puts(
        "usage: fastfetch --color <color>\n"
        "\n"
        "<color> must be a color encoding for linux terminals. It is inserted between \"ESC[\" and \"m\".\n"
        "Infos about them can be found here: https://en.wikipedia.org/wiki/ANSI_escape_code#Colors.\n"
        "Examples:\n"
        "   \"--color 35\":    sets the color to pink\n"
        "   \"--color 4;92\":  sets the color to bright Green with underline\n"
        "   \"--color 5;104\": blinking text on a blue background\n"
        "If no color is set, the main color of the logo will be used.\n"
    );
}

static inline void printCommandHelpFormat()
{
    puts(
        "A format string is string that contains placeholder for values.\n"
        "These placeholders beginn with a '{', contain the index of the value and end with a '}'.\n"
        "For example the format string \"Values: {1} ({2})\", with the values \"First\" and \"My second val\", will produce \"Values: First (My second val)\".\n"
        "The format string can contain placeholdes in any oder and multiple occurences.\n"
        "To include spaces when setting from command line, surround the whole string with double quotes (\").\n"
        "\n"
        "If the value index is missing, meaning the placeholder is \"{}\", an internal counter sets the value index.\n"
        "This means, that the format string \"Values: {1} ({2})\" is equivalent to \"Values: {} ({})\".\n"
        "Note that this counter only counts empty placeholders, so the format string \"{2} {} {}\" will contain the second value, then the first, and then again the second.\n"
        "\n"
        "To make formatting easier, a double open curly brace (\"{{\") will be printed as a single open curly brace and not counted as the beginn of a placeholder.\n"
        "If a value index is missformatted or wants a non existing value, it will be printed as is, with the curly braces around it.\n"
        "If the last placeholder isn't closed, it will be printed as is, with the open curly braces at the start.\n"
        "\n"
        "Format string is also the way to go to set a fixed value, just use one without placeholders.\n"
        "For example when running in headless mode you could use \"--resolution-format \"Preferred\"."
    );
}

static void constructAndPrintCommandHelpFormat(const char* name, const char* def, uint32_t numArgs, ...)
{
    va_list argp;
    va_start(argp, numArgs);

    printf("--%s-format:\n", name);
    printf("Sets the format string for %s output.\n", name);
    puts("To see how a format string is constructed, take a look at \"fastfetch --help format\".");
    puts("Following values are passed:");

    for(uint32_t i = 1; i <= numArgs; i++)
        printf("        {%u}: %s\n", i, va_arg(argp, const char*));

    printf("The default is something like \"%s\".\n", def);

    va_end(argp);
}

static inline void printCommandHelp(const char* command)
{
    if(strcasecmp(command, "c") == 0 || strcasecmp(command, "color") == 0)
        printCommandHelpColor();
    else if(strcasecmp(command, "format") == 0)
        printCommandHelpFormat();
    else if(strcasecmp(command, "os-format") == 0)
        constructAndPrintCommandHelpFormat("os", "{} {}", 2, "Name of the OS", "Architecture of the OS");
    else if(strcasecmp(command, "host-format") == 0)
        constructAndPrintCommandHelpFormat("host", "{} {} {}", 3, "Host family", "Host name", "Host version");
    else if(strcasecmp(command, "kernel-format") == 0)
        constructAndPrintCommandHelpFormat("kernel", "{2}", 3, "Kernel sysname", "Kernel release", "Kernel version");
    else if(strcasecmp(command, "uptime-format") == 0)
        constructAndPrintCommandHelpFormat("uptime", "{} days {} hours {} mins", 4, "Days", "Hours", "Minutes", "Seconds");
    else if(strcasecmp(command, "packages-format") == 0)
        constructAndPrintCommandHelpFormat("packages", "{2} (pacman), {3} (flatpak)", 3, "Number of all packages", "Number of pacman packages", "Number of flatpak packages");
    else if(strcasecmp(command, "shell-format") == 0)
        constructAndPrintCommandHelpFormat("shell", "{2}", 2, "Shell path (without name)", "Shell name");
    else if(strcasecmp(command, "resolution-format") == 0)
        constructAndPrintCommandHelpFormat("resolution", "{}x{} @ {}Hz", 3, "Screen width", "Screen height", "Screen refresh rate");
    else if(strcasecmp(command, "de-format") == 0)
        constructAndPrintCommandHelpFormat("de", "{} {} ({})", 3, "DE name", "DE version", "Name of display server");
    else if(strcasecmp(command, "wm-format") == 0)
        constructAndPrintCommandHelpFormat("wm", "{}", 1, "WM name");
    else if(strcasecmp(command, "theme-format") == 0)
        constructAndPrintCommandHelpFormat("theme", "{} [Plasma], {5}", 5, "Plasma theme", "GTK2 theme", "GTK3 theme", "GTK4 theme", "Combined GTK themes");
    else if(strcasecmp(command, "icons-format") == 0)
        constructAndPrintCommandHelpFormat("icons", "{} [Plasma], {5}", 5, "Plasma icons", "GTK2 icons", "GTK3 icons", "GTK4 icons", "Combined GTK icons");
    else if(strcasecmp(command, "font-format") == 0)
        constructAndPrintCommandHelpFormat("font", "{} [Plasma], {5}", 5, "Plasma font", "GTK2 font", "GTK3 font", "GTK4 font", "Combined GTK fonts");
    else if(strcasecmp(command, "terminal-format") == 0)
        constructAndPrintCommandHelpFormat("terminal", "{}", 1, "Terminal name");
    else if(strcasecmp(command, "terminal-font-format") == 0)
        constructAndPrintCommandHelpFormat("terminal-font", "{}", 1, "Terminal font name");
    else if(strcasecmp(command, "cpu-format") == 0)
        constructAndPrintCommandHelpFormat("cpu", "{} ({}) @ {}GHz", 3, "CPU name", "CPU logical core count", "CPU frequency");
    else if(strcasecmp(command, "gpu-format") == 0)
        constructAndPrintCommandHelpFormat("gpu", "{} {}", 2, "GPU vendor", "GPU name");
    else if(strcasecmp(command, "memory-format") == 0)
        constructAndPrintCommandHelpFormat("memory", "{}MiB / {}MiB ({}%)", 3, "Used memory", "Total memory", "Used memory percentage");
    else if(strcasecmp(command, "disk-format") == 0)
        constructAndPrintCommandHelpFormat("disk", "{}GB / {}GB ({}%)", 3, "Used disk space", "Total disk space", "Used disk space percentage");
    else if(strcasecmp(command, "battery-format") == 0)
        constructAndPrintCommandHelpFormat("battery", "{} {} ({}) [{}%; {}]", 5, "Battery manufactor", "Battery model", "Battery technology", "Battery capacity", "Battery status");
    else if(strcasecmp(command, "locale-format") == 0)
        constructAndPrintCommandHelpFormat("locale", "{}", 1, "Locale code");
    else
        printf("No specific help for command %s provided\n", command);
}

static inline bool optionParseBoolean(const char* str)
{
    if(str == NULL)
        return true;

    return (
        strcasecmp(str, "true") == 0 ||
        strcasecmp(str, "yes")  == 0 ||
        strcasecmp(str, "1")    == 0
    );
}

static inline void optionParseString(const char* key, const char* value, FFstrbuf* buffer)
{
    if(value == NULL)
    {
        printf("Error: usage: %s <str>\n", key);
        exit(477);
    }
    ffStrbufSetS(buffer, value);
}

static void parseOption(FFinstance* instance, FFdata* data, const char* key, const char* value)
{
    if(strcasecmp(key, "-h") == 0 || strcasecmp(key, "--help") == 0)
    {
        if(value == NULL)
            printHelp();
        else
            printCommandHelp(value);

        exit(0);
    }
    else if(strcasecmp(key, "-v") == 0 || strcasecmp(key, "--version") == 0)
    {
        puts(FASTFETCH_PROJECT_NAME" "FASTFETCH_PROJECT_VERSION);
        exit(0);
    }
    else if(strcasecmp(key, "--list-logos") == 0)
    {
        ffListLogos();
        exit(0);
    }
    else if(strcasecmp(key, "--print-logos") == 0)
    {
        ffPrintLogos(instance->config.colorLogo);
        exit(0);
    }
    else if(strcasecmp(key, "--print-default-config") == 0)
    {
        puts(FASTFETCH_DEFAULT_CONFIG);
        exit(0);
    }
    else if(strcasecmp(key, "--spacing") == 0)
    {
        if(value == NULL)
        {
            printf("Error: usage: %s <width>\n", key);
            exit(404);
        }
        if(sscanf(value, "%hd", &instance->config.logo_spacing) != 1)
        {
            printf("Error: couldn't parse %s to uint16_t\n", value);
            exit(405);
        }
    }
    else if(strcasecmp(key, "-x") == 0 || strcasecmp(key, "--offsetx") == 0)
    {
        if(value == NULL)
        {
            printf("Error: usage: %s <offset>\n", key);
            exit(408);
        }
        if(sscanf(value, "%hi", &instance->config.offsetx) != 1)
        {
            printf("Error: couldn't parse %s to int16_t\n", value);
            exit(409);
        }
    }
    else if(strcasecmp(key, "--set") == 0)
    {
        if(value == NULL)
        {
            printf("Error: usage: %s <key=value>\n", key);
            exit(411);
        }

        char* seperator = strchr(value, '=');

        if(seperator == NULL)
        {
            printf("Error: usage: %s <key=value>, '=' missing\n", key);
            exit(412);
        }

        *seperator = '\0';

        ffValuestoreSet(&data->valuestore, value, seperator + 1);
    }
    else if(strcasecmp(key, "-r") == 0 || strcasecmp(key, "--recache") == 0)
    {
        //Set cacheSave as well, beacuse the user expects the values to  be cached when expliciting using --recache   
        instance->config.recache = optionParseBoolean(value);
        instance->config.cacheSave = instance->config.recache;
    }
    else if(strcasecmp(key, "--show-errors") == 0)
        instance->config.showErrors = optionParseBoolean(value);
    else if(strcasecmp(key, "--color-logo") == 0)
        instance->config.colorLogo = optionParseBoolean(value);
    else if(strcasecmp(key, "--print-remaining-logo") == 0)
        instance->config.printRemainingLogo = optionParseBoolean(value);
    else if(strcasecmp(key, "--structure") == 0)
        optionParseString(key, value, &data->structure);
    else if(strcasecmp(key, "-l") == 0 || strcasecmp(key, "--logo") == 0)
        optionParseString(key, value, &data->logoName);
    else if(strcasecmp(key, "-s") == 0 || strcasecmp(key, "--seperator") == 0)
        optionParseString(key, value, &instance->config.seperator);
    else if(strcasecmp(key, "-c") == 0 || strcasecmp(key, "--color") == 0)
        optionParseString(key, value, &instance->config.color);
    else if(strcasecmp(key, "--os-format") == 0)
        optionParseString(key, value, &instance->config.osFormat);
    else if(strcasecmp(key, "--host-format") == 0)
        optionParseString(key, value, &instance->config.hostFormat);
    else if(strcasecmp(key, "--kernel-format") == 0)
        optionParseString(key, value, &instance->config.kernelFormat);
    else if(strcasecmp(key, "--uptime-format") == 0)
        optionParseString(key, value, &instance->config.uptimeFormat);
    else if(strcasecmp(key, "--packages-format") == 0)
        optionParseString(key, value, &instance->config.packagesFormat);
    else if(strcasecmp(key, "--shell-format") == 0)
        optionParseString(key, value, &instance->config.shellFormat);
    else if(strcasecmp(key, "--resolution-format") == 0)
        optionParseString(key, value, &instance->config.resolutionFormat);
    else if(strcasecmp(key, "--de-format") == 0)
        optionParseString(key, value, &instance->config.deFormat);
    else if(strcasecmp(key, "--wm-format") == 0)
        optionParseString(key, value, &instance->config.wmFormat);
    else if(strcasecmp(key, "--theme-format") == 0)
        optionParseString(key, value, &instance->config.themeFormat);
    else if(strcasecmp(key, "--icons-format") == 0)
        optionParseString(key, value, &instance->config.iconsFormat);
    else if(strcasecmp(key, "--font-format") == 0)
        optionParseString(key, value, &instance->config.fontFormat);
    else if(strcasecmp(key, "--terminal-format") == 0)
        optionParseString(key, value, &instance->config.terminalFormat);
    else if(strcasecmp(key, "--terminal-font-format") == 0)
        optionParseString(key, value, &instance->config.termFontFormat);
    else if(strcasecmp(key, "--cpu-format") == 0)
        optionParseString(key, value, &instance->config.cpuFormat);
    else if(strcasecmp(key, "--gpu-format") == 0)
        optionParseString(key, value, &instance->config.gpuFormat);
    else if(strcasecmp(key, "--memory-format") == 0)
        optionParseString(key, value, &instance->config.memoryFormat);
    else if(strcasecmp(key, "--disk-format") == 0)
        optionParseString(key, value, &instance->config.diskFormat);
    else if(strcasecmp(key, "--battery-format") == 0)
        optionParseString(key, value, &instance->config.batteryFormat);
    else if(strcasecmp(key, "--locale-format") == 0)
        optionParseString(key, value, &instance->config.localeFormat);
    else if(strcasecmp(key, "--lib-PCI") == 0)
        optionParseString(key, value, &instance->config.libPCI);
    else if(strcasecmp(key, "--lib-X11") == 0)
        optionParseString(key, value, &instance->config.libX11);
    else if(strcasecmp(key, "--lib-Xrandr") == 0)
        optionParseString(key, value, &instance->config.libXrandr);
    else
    {
        printf("Error: unknown option: %s\n", key);
        exit(400);
    }
}

static void parseConfigFile(FFinstance* instance, FFdata* data)
{
    char fileName[256];

    const char* xdgConfig = getenv("XDG_CONFIG_HOME");
    if(xdgConfig == NULL)
    {
        strcpy(fileName, instance->state.passwd->pw_dir);
        strcat(fileName, "/.config");
    }
    else
    {
        strcpy(fileName, xdgConfig);
    }
    mkdir(fileName, S_IRWXU | S_IXGRP | S_IRGRP | S_IXOTH | S_IROTH); //I hope everybody has a config folder but whow knews
    
    strcat(fileName, "/fastfetch/");
    mkdir(fileName, S_IRWXU | S_IRGRP | S_IROTH);

    strcat(fileName, "config.conf");

    if(access(fileName, F_OK) != 0)
    {
        FILE* file = fopen(fileName, "w");
        fputs(FASTFETCH_DEFAULT_CONFIG, file);
        fclose(file);
        return;
    }
    
    FILE* file = fopen(fileName, "r");

    char* lineStart = NULL;
    size_t len = 0;
    ssize_t read;

    while ((read = getline(&lineStart, &len, file)) != -1) {

        //We need to copy lineStart because we modify this value, but need the original for free
        char* line = lineStart;

        if(line[read - 1] == '\n')
            line[read - 1] = '\0';
        else
            line[read] = '\0';

        ffTrimTrailingWhitespace(line);

        //This trims leading whitespace
        while(*line == ' ')
            ++line;

        if(line[0] == '\0' || line[0] == '#')
            continue;

        char* valueStart = strchr(line, ' ');
        if(valueStart == NULL)
        {
            parseOption(instance, data, line, NULL);
        }
        else
        {
            //Seperate key and value by simply replacing the first space with a \0
            *valueStart = '\0';
            ++valueStart;

            //Trim whitespace at beginn of value
            while(*valueStart == ' ')
                ++valueStart;

            //If we want whitespace in values, we need to quote it. This is done to keep consistency with shell.
            if(*valueStart == '"')
            {
                char* last = valueStart + strlen(valueStart) - 1;
                if(*last == '"')
                {
                    ++valueStart;
                    *last = '\0';
                }
            }

            parseOption(instance, data, line, valueStart);
        }
    }

    if(lineStart != NULL)
        free(lineStart);

    fclose(file);
}

static void parseArguments(FFinstance* instance, FFdata* data, int argc, const char** argv)
{
    //This is generally a good idea, because cached values most likely contain values generated with other arguments
    //Hovwever we dont do this with arguments in the config file, because they are more likely to stay the same
    //If caching is _really_ wanted (e.g. a call in .bashrc with arguments), one can still set --recache false
    if(argc > 1)
    {
        instance->config.recache = true;
        instance->config.cacheSave = false;
    }

    for(int i = 1; i < argc; i++)
    {
        if(i == argc - 1 || argv[i + 1][0] == '-')
        {
            parseOption(instance, data, argv[i], NULL);
        }
        else
        {
            parseOption(instance, data, argv[i], argv[i + 1]);
            ++i;
        }
    }
}

static void applyData(FFinstance* instance, FFdata* data)
{
    //We must do this after parsing all options because of color options
    if(ffStrbufIsEmpty(&data->logoName))
        ffLoadLogo(&instance->config);
    else
        ffLoadLogoSet(&instance->config, data->logoName.chars);

    //This must be done after loading the logo
    if(ffStrbufIsEmpty(&instance->config.color))
        ffStrbufSetS(&instance->config.color, instance->config.logo.color);
}

static void parseStructureCommand(FFinstance* instance, FFdata* data, const char* line)
{
    const char* setValue = ffValuestoreGet(&data->valuestore, line);
    if(setValue != NULL)
    {
        ffPrintCustom(instance, line, setValue);
        return;
    }

    if(strcasecmp(line, "break") == 0)
        ffPrintBreak(instance);
    else if(strcasecmp(line, "title") == 0)
        ffPrintTitle(instance);
    else if(strcasecmp(line, "seperator") == 0)
        ffPrintSeperator(instance);
    else if(strcasecmp(line, "os") == 0)
        ffPrintOS(instance);
    else if(strcasecmp(line, "host") == 0)
        ffPrintHost(instance);
    else if(strcasecmp(line, "kernel") == 0)
        ffPrintKernel(instance);
    else if(strcasecmp(line, "uptime") == 0)
        ffPrintUptime(instance);
    else if(strcasecmp(line, "packages") == 0)
        ffPrintPackages(instance);
    else if(strcasecmp(line, "shell") == 0)
        ffPrintShell(instance);
    else if(strcasecmp(line, "resolution") == 0)
        ffPrintResolution(instance);
    else if(strcasecmp(line, "desktopenvironment") == 0 || strcasecmp(line, "de") == 0)
        ffPrintDesktopEnvironment(instance);
    else if(strcasecmp(line, "windowmanager") == 0 || strcasecmp(line, "wm") == 0)
        ffPrintWM(instance);
    else if(strcasecmp(line, "theme") == 0)
        ffPrintTheme(instance);
    else if(strcasecmp(line, "icons") == 0)
        ffPrintIcons(instance);
    else if(strcasecmp(line, "font") == 0)
        ffPrintFont(instance);
    else if(strcasecmp(line, "terminal") == 0)
        ffPrintTerminal(instance);
    else if(strcasecmp(line, "terminalfont") == 0)
        ffPrintTerminalFont(instance);
    else if(strcasecmp(line, "cpu") == 0)
        ffPrintCPU(instance);
    else if(strcasecmp(line, "gpu") == 0)
        ffPrintGPU(instance);
    else if(strcasecmp(line, "memory") == 0)
        ffPrintMemory(instance);
    else if(strcasecmp(line, "disk") == 0)
        ffPrintDisk(instance);
    else if(strcasecmp(line, "battery") == 0)
        ffPrintBattery(instance);
    else if(strcasecmp(line, "locale") == 0)
        ffPrintLocale(instance);
    else if(strcasecmp(line, "colors") == 0)
        ffPrintColors(instance);
    else
        ffPrintError(instance, line, "<no implementaion provided>");
}

static void run(FFinstance* instance, FFdata* data)
{
    if(ffStrbufIsEmpty(&data->structure))
        ffStrbufSetS(&data->structure, FASTFETCH_DEFAULT_STRUCTURE);
    
    char* remaining = data->structure.chars;
    char* colon = NULL;

    while(true)
    {
        colon = strchr(remaining, ':');
        
        if(colon != NULL)
            *colon = '\0';
        
        parseStructureCommand(instance, data, remaining);
        
        if(colon != NULL)
            remaining = colon + 1;
        else
            break;
    }
}

int main(int argc, const char** argv)
{
    FFinstance instance;
    ffInitState(&instance.state);
    ffDefaultConfig(&instance.config);

    FFdata data;
    ffValuestoreInit(&data.valuestore);
    ffStrbufInit(&data.structure);
    ffStrbufInitA(&data.logoName, 2048);

    parseConfigFile(&instance, &data);
    parseArguments(&instance, &data, argc, argv);
    applyData(&instance, &data); //Here we do things that need to be done after parsing all options

    run(&instance, &data);

    ffFinish(&instance);

    ffStrbufDestroy(&data.structure);
    ffStrbufDestroy(&data.logoName);
    ffValuestoreDelete(&data.valuestore);
}
