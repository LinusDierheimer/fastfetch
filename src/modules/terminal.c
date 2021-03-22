#include "fastfetch.h"

static void getTerminalName(FFinstance* instance, const char* pid, char* terminal, char* error)
{
    char file[234];
    sprintf(file, "/proc/%s/stat", pid);

    FILE* stat = fopen(file, "r");
    if(stat == NULL)
    {
        sprintf(error, "fopen(\"%s\", \"r\") == NULL", file);
        return;
    }

    char name[256];
    char ppid[256];
    if(fscanf(stat, "%*s (%[^)])%*s%s", name, ppid) != 2)
    {
        strcpy(error, "fscanf(stat, \"%*s (%[^)])%*s%s\", name, ppid) != 2");
        return;
    }

    fclose(stat);

    if (
        strcasecmp(name, "bash")   == 0 ||
        strcasecmp(name, "sh")     == 0 ||
        strcasecmp(name, "zsh")    == 0 ||
        strcasecmp(name, "ksh")    == 0 ||
        strcasecmp(name, "fish")   == 0 ||
        strcasecmp(name, "sudo")   == 0 ||
        strcasecmp(name, "su")     == 0 ||
        strcasecmp(name, "doas")   == 0 ||
        strcasecmp(name, "strace") == 0 )
    {
        getTerminalName(instance, ppid, terminal, error);
        return;
    }
    
    if(
        strcasecmp(name, "systemd") == 0 ||
        strcasecmp(name, "init")    == 0 ||
        strcasecmp(name, "login")   == 0 ||
        strcasecmp(name, "0")       == 0 ||
        strcasecmp(ppid, "0")       == 0 )
    {
        strcpy(terminal, "TTY");
    }
    else
    {
        strcpy(terminal, name);
    }
}

void ffPopulateTerminal(FFinstance* instance)
{
    if(instance->state.terminal.calculated)
        return;

    static char terminal[256];
    static char error[256];

    terminal[0] = '\0';
    error[0] = '\0';

    char ppid[256];
    sprintf(ppid, "%i", getppid());
    getTerminalName(instance, ppid, terminal, error);

    if(terminal[0] != '\0')
        ffStrbufSetS(&instance->state.terminal.value, terminal);

    if(error[0] != '\0')
        instance->state.terminal.error = error;

    instance->state.terminal.calculated = true;
}

void ffPrintTerminal(FFinstance* instance)
{
    ffPopulateTerminal(instance);

    if(instance->state.terminal.error != NULL)
    {
        ffPrintError(instance, "Terminal", instance->state.terminal.error);
        return;
    }

    ffPrintLogoAndKey(instance, "Terminal");
    
    if(ffStrbufIsEmpty(&instance->config.terminalFormat))
    {
        ffStrbufWriteTo(&instance->state.terminal.value, stdout);
    }
    else
    {
        FF_STRBUF_CREATE(terminal);

        ffParseFormatString(&terminal, &instance->config.terminalFormat, 1,
            (FFformatarg){FF_FORMAT_ARG_TYPE_STRBUF, &instance->state.terminal.value}
        );

        ffStrbufWriteTo(&terminal, stdout);
    }

    putchar('\n');
}
