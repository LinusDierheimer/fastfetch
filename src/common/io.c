#include "fastfetch.h"

#include <malloc.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>

void ffPrintLogoAndKey(FFinstance* instance, const char* moduleName, uint8_t moduleIndex, const FFstrbuf* customKeyFormat)
{
    ffPrintLogoLine(instance);

    fputs(FASTFETCH_TEXT_MODIFIER_BOLT, stdout);
    ffStrbufWriteTo(&instance->config.color, stdout);

    FF_STRBUF_CREATE(key);

    if(customKeyFormat == NULL || customKeyFormat->length == 0)
    {
        ffStrbufAppendS(&key, moduleName);

        if(moduleIndex > 0)
            ffStrbufAppendF(&key, " %hhu", moduleIndex);
    }
    else
    {
        ffParseFormatString(&key, customKeyFormat, NULL, 1, (FFformatarg[]){
            {FF_FORMAT_ARG_TYPE_UINT8, &moduleIndex}
        });
    }

    ffStrbufWriteTo(&key, stdout);
    fputs(FASTFETCH_TEXT_MODIFIER_RESET, stdout);
    ffStrbufWriteTo(&instance->config.seperator, stdout);
    ffStrbufDestroy(&key);
}

void ffPrintError(FFinstance* instance, const char* moduleName, uint8_t moduleIndex, const FFstrbuf* customKeyFormat, const FFstrbuf* formatString, uint32_t numFormatArgs, const char* message, ...)
{
    va_list arguments;
    va_start(arguments, message);

    if((formatString == NULL || formatString->length == 0) && instance->config.showErrors)
    {
        ffPrintLogoAndKey(instance, moduleName, moduleIndex, customKeyFormat);
        fputs(FASTFETCH_TEXT_MODIFIER_ERROR, stdout);
        vprintf(message, arguments);
        puts(FASTFETCH_TEXT_MODIFIER_ERROR);
    }
    else
    {
        FF_STRBUF_CREATE(error);
        ffStrbufAppendVF(&error, message, arguments);

        FFformatarg nullArgs[numFormatArgs];
        for(uint32_t i = 0; i < numFormatArgs; i++)
        {
            nullArgs[i].type = FF_FORMAT_ARG_TYPE_NULL;
            nullArgs[i].value = NULL;
        }

        ffPrintFormatString(instance, moduleName, moduleIndex, customKeyFormat, formatString, &error, numFormatArgs, nullArgs);

        ffStrbufDestroy(&error);
    }

    va_end(arguments);
}

void ffPrintFormatString(FFinstance* instance, const char* moduleName, uint8_t moduleIndex, const FFstrbuf* customKeyFormat, const FFstrbuf* formatString, const FFstrbuf* error, uint32_t numArgs, const FFformatarg* arguments)
{
    FFstrbuf buffer;
    ffStrbufInitA(&buffer, 256);

    ffParseFormatString(&buffer, formatString, error, numArgs, arguments);

    if(buffer.length > 0)
    {
        ffPrintLogoAndKey(instance, moduleName, moduleIndex, customKeyFormat);
        ffStrbufPutTo(&buffer, stdout);
    }

    ffStrbufDestroy(&buffer);
}

static void appendCacheDir(FFinstance* instance, FFstrbuf* buffer)
{
    static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    static FFstrbuf cacheDir;
    static bool init = false;

    pthread_mutex_lock(&mutex);

    if(init)
    {
        pthread_mutex_unlock(&mutex);
        ffStrbufAppend(buffer, &cacheDir);
        return;
    }

    ffStrbufInitA(&cacheDir, 64);
    ffStrbufAppendS(&cacheDir, getenv("XDG_CACHE_HOME"));

    if(cacheDir.length == 0)
    {
        ffStrbufSetS(&cacheDir, instance->state.passwd->pw_dir);
        ffStrbufAppendS(&cacheDir, "/.cache/");
    }

    mkdir(cacheDir.chars, S_IRWXU | S_IXGRP | S_IRGRP | S_IXOTH | S_IROTH); //I hope everybody has a cache folder but whow knews

    ffStrbufAppendS(&cacheDir, "fastfetch/");
    mkdir(cacheDir.chars, S_IRWXU | S_IRGRP | S_IROTH);

    init = true;

    pthread_mutex_unlock(&mutex);

    ffStrbufAppend(buffer, &cacheDir);
}

static inline void getCacheFileValue(FFinstance* instance, const char* moduleName, FFstrbuf* cacheFilePath)
{
    appendCacheDir(instance, cacheFilePath);
    ffStrbufAppendS(cacheFilePath, moduleName);
    ffStrbufAppendS(cacheFilePath, ".ffcv");
}

static inline void getCacheFileSplit(FFinstance* instance, const char* moduleName, FFstrbuf* cacheFilePath)
{
    appendCacheDir(instance, cacheFilePath);
    ffStrbufAppendS(cacheFilePath, moduleName);
    ffStrbufAppendS(cacheFilePath, ".ffcs");
}

static bool printCachedValue(FFinstance* instance, const char* moduleName, const FFstrbuf* customKeyFormat)
{
    FFstrbuf cacheFilePath;
    ffStrbufInitA(&cacheFilePath, 64);
    getCacheFileValue(instance, moduleName, &cacheFilePath);

    FFstrbuf content;
    ffStrbufInitA(&content, 512);
    ffAppendFileContent(cacheFilePath.chars, &content);

    ffStrbufTrimRight(&content, '\0'); //Strbuf always appends a '\0' at the end. We want the last null byte to be at the position of the length

    if(content.length == 0)
        return false;

    uint8_t moduleCounter = 1;

    uint32_t lastIndex = 0;
    while(lastIndex < content.length)
    {
        uint32_t nullByteIndex = ffStrbufFirstIndexAfterC(&content, lastIndex, '\0');
        uint8_t moduleIndex = (moduleCounter == 1 && nullByteIndex == content.length) ? 0 : moduleCounter;
        ffPrintLogoAndKey(instance, moduleName, moduleIndex, customKeyFormat);
        puts(content.chars + lastIndex);
        lastIndex = nullByteIndex + 1;
        ++moduleCounter;
    }

    ffStrbufDestroy(&content);
    ffStrbufDestroy(&cacheFilePath);

    return moduleCounter > 1;
}

static bool printCachedFormat(FFinstance* instance, const char* moduleName, const FFstrbuf* customKeyFormat, const FFstrbuf* formatString, uint32_t numArgs)
{
    FFstrbuf cacheFilePath;
    ffStrbufInitA(&cacheFilePath, 64);
    getCacheFileSplit(instance, moduleName, &cacheFilePath);

    FFstrbuf content;
    ffStrbufInitA(&content, 512);
    ffAppendFileContent(cacheFilePath.chars, &content);

    if(content.length == 0)
        return false;

    //Strbuf adds an extra nullbyte at chars[length]. We want this one to be the end of the last value to test for index == length
    if(content.chars[content.length - 1] == '\0')
        ffStrbufSubstrBefore(&content, content.length - 1);

    uint8_t moduleCounter = 1;

    FFformatarg arguments[numArgs];
    uint32_t argumentCounter = 0;

    uint32_t lastIndex = 0;
    while(lastIndex < content.length)
    {
        arguments[argumentCounter].type = FF_FORMAT_ARG_TYPE_STRING;
        arguments[argumentCounter].value = &content.chars[lastIndex];
        ++argumentCounter;

        uint32_t nullByteIndex = ffStrbufFirstIndexAfterC(&content, lastIndex, '\0');

        if(argumentCounter == numArgs)
        {
            uint8_t moduleIndex = (moduleCounter == 1 && nullByteIndex == content.length) ? 0 : moduleCounter;
            ffPrintFormatString(instance, moduleName, moduleIndex, customKeyFormat, formatString, NULL, numArgs, arguments);
            ++moduleCounter;
            argumentCounter = 0;
        }

        lastIndex = nullByteIndex + 1;
    }

    ffStrbufDestroy(&content);
    ffStrbufDestroy(&cacheFilePath);

    return moduleCounter > 1;
}

bool ffPrintFromCache(FFinstance* instance, const char* moduleName, const FFstrbuf* customKeyFormat, const FFstrbuf* formatString, uint32_t numArgs)
{
    if(instance->config.recache)
        return false;

    if(formatString == NULL || formatString->length == 0)
        return printCachedValue(instance, moduleName, customKeyFormat);
    else
        return printCachedFormat(instance, moduleName, customKeyFormat, formatString, numArgs);
}

void ffPrintAndAppendToCache(FFinstance* instance, const char* moduleName, uint8_t moduleIndex, const FFstrbuf* customKeyFormat, FFcache* cache, const FFstrbuf* value, const FFstrbuf* formatString, uint32_t numArgs, const FFformatarg* arguments)
{
    if(formatString == NULL || formatString->length == 0)
    {
        ffPrintLogoAndKey(instance, moduleName, moduleIndex, customKeyFormat);
        ffStrbufPutTo(value, stdout);
    }
    else
    {
        ffPrintFormatString(instance, moduleName, moduleIndex, customKeyFormat, formatString, NULL, numArgs, arguments);
    }

    if(cache->value != NULL)
    {
        ffStrbufWriteTo(value, cache->value);
        fputc('\0', cache->value);
    }

    if(cache->split == NULL)
        return;

    for(uint32_t i = 0; i < numArgs; i++)
    {
        FFstrbuf buffer;
        ffStrbufInit(&buffer);
        ffFormatAppendFormatArg(&buffer, &arguments[i]);
        ffStrbufWriteTo(&buffer, cache->split);
        ffStrbufDestroy(&buffer);
        fputc('\0', cache->split);
    }
}

void ffPrintAndSaveToCache(FFinstance* instance, const char* moduleName, const FFstrbuf* customKeyFormat, const FFstrbuf* value, const FFstrbuf* formatString, uint32_t numArgs, const FFformatarg* arguments)
{
    FFcache cache;
    ffCacheOpenWrite(instance, moduleName, &cache);
    ffPrintAndAppendToCache(instance, moduleName, 0, customKeyFormat, &cache, value, formatString, numArgs, arguments);
    ffCacheClose(&cache);
}

void ffCacheValidate(FFinstance* instance)
{
    FFstrbuf cacheFilePath;
    ffStrbufInitA(&cacheFilePath, 64);
    appendCacheDir(instance, &cacheFilePath);
    ffStrbufAppendS(&cacheFilePath, "cacheversion.ffv");

    FFstrbuf content;
    ffStrbufInit(&content);
    ffAppendFileContent(cacheFilePath.chars, &content);

    if(ffStrbufCompS(&content, FASTFETCH_PROJECT_VERSION) == 0)
    {
        ffStrbufDestroy(&content);
        ffStrbufDestroy(&cacheFilePath);
        return;
    }

    instance->config.recache = true;

    FILE* versionFile = fopen(cacheFilePath.chars, "w");
    if(versionFile == NULL)
        return;

    fputs(FASTFETCH_PROJECT_VERSION, versionFile);

    fclose(versionFile);

    ffStrbufDestroy(&content);
    ffStrbufDestroy(&cacheFilePath);
}

void ffCacheOpenWrite(FFinstance* instance, const char* moduleName, FFcache* cache)
{
    FFstrbuf cacheFileValue;
    ffStrbufInitA(&cacheFileValue, 64);
    getCacheFileValue(instance, moduleName, &cacheFileValue);
    cache->value = fopen(cacheFileValue.chars, "w");
    ffStrbufDestroy(&cacheFileValue);

    FFstrbuf cacheFileSplit;
    ffStrbufInitA(&cacheFileSplit, 64);
    getCacheFileSplit(instance, moduleName, &cacheFileSplit);
    cache->split = fopen(cacheFileSplit.chars, "w");
    ffStrbufDestroy(&cacheFileSplit);
}

void ffCacheClose(FFcache* cache)
{
    if(cache->value != NULL)
        fclose(cache->value);

    if(cache->split != NULL)
        fclose(cache->split);
}

void ffParsePropFile(const char* fileName, const char* regex, char* buffer)
{
    buffer[0] = '\0'; //If an error occures, this is the indicator

    char* line = NULL;
    size_t len = 0;

    FILE* file = fopen(fileName, "r");
    if(file == NULL)
        return; // handle errors in higher functions

    while (getline(&line, &len, file) != -1)
    {
        if (sscanf(line, regex, buffer) > 0)
            break;
    }

    fclose(file);
    if(line != NULL)
    { // Strip unescaped quotes
		int j = 0;
		for (int i = 0; i < len; i++)
		{
			if (buffer[i] != '"' && buffer[i] != '\\')
			{ 
				buffer[j++] = buffer[i];
			}
			else if (buffer[i+1] == '"' && buffer[i] == '\\')
			{ 
				buffer[j++] = '"';
			}
			else if (buffer[i+1] != '"' && buffer[i] == '\\')
			{ 
				buffer[j++] = '\\';
			}
		}
		if (j > 0) buffer[j] = 0;

        free(line);
    }
}

void ffParsePropFileHome(FFinstance* instance, const char* relativeFile, const char* regex, char* buffer)
{
    FFstrbuf absolutePath;
    ffStrbufInitA(&absolutePath, 64);
    ffStrbufAppendS(&absolutePath, instance->state.passwd->pw_dir);
    ffStrbufAppendC(&absolutePath, '/');
    ffStrbufAppendS(&absolutePath, relativeFile);

    ffParsePropFile(absolutePath.chars, regex, buffer);

    ffStrbufDestroy(&absolutePath);
}

void ffAppendFDContent(int fd, FFstrbuf* buffer)
{
    ssize_t readed;
    while((readed = read(fd, buffer->chars + buffer->length, buffer->allocated - buffer->length)) == (buffer->allocated - buffer->length))
    {
        buffer->length += (uint32_t) readed;
        ffStrbufEnsureCapacity(buffer, buffer->allocated * 2);
    }

    if(readed >= 0)
        buffer->length += (uint32_t) readed;

    ffStrbufTrimRight(buffer, '\n');
    ffStrbufTrimRight(buffer, ' ');
}

void ffAppendFileContent(const char* fileName, FFstrbuf* buffer)
{
    int fd = open(fileName, O_RDONLY);
    if(fd == -1)
        return;

    ffAppendFDContent(fd, buffer);

    close(fd);
}

void ffGetFileContent(const char* fileName, FFstrbuf* buffer)
{
    ffStrbufClear(buffer);
    ffAppendFileContent(fileName, buffer);
}
