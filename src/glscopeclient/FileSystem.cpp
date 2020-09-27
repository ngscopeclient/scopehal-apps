#include "FileSystem.h"

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#include <fileapi.h>
#include <shellapi.h>
#else
#include <glob.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ftw.h>
#include <stdio.h>
#endif


using namespace std;


vector<string> Glob(const string& pathPattern, bool onlyDirectories)
{
   vector<string> result{ };
 
#ifdef _WIN32
    WIN32_FIND_DATA findData{ };
    HANDLE fileSearch{ };
    
    fileSearch = FindFirstFileEx(
        pathPattern.c_str(),
        FindExInfoStandard,
        &findData,
        onlyDirectories ? FindExSearchLimitToDirectories : 0,
        NULL,
        0
    );
    
    if(fileSearch != INVALID_HANDLE_VALUE)
    {
        while(FindNextFile(fileSearch, &findData))
        {
            const auto* dir = findData.cFileName;
            
            if(!strcmp(dir, "..") || !strcmp(dir, "."))
                continue;
                
            if(!onlyDirectories || (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                results.push_back(string{dir});
            }
        }
    }
#else
    glob_t globResult{ };
    glob(pathPattern.c_str(), onlyDirectories ? GLOB_ONLYDIR : 0, NULL, &globResult);
    
    if(globResult.gl_pathc > 0)
    {
        for(auto ix = 0U; ix < globResult.gl_pathc; ++ix)
        {
            const auto* dir = globResult.gl_pathv[ix];
            result.push_back(string{dir});
        }
    }
    
    globfree(&globResult);
#endif
   
   return result;
}

void RemoveDirectory(const string& basePath)
{
#ifdef _WIN32
    SHFILEOPSTRUCT deleteDir = {
        NULL,
        FO_DELETE,
        basePath.c_str(),
        NULL,
        FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMATION,
        FALSE,
        NULL,
        NULL
    };
    
    SHFileOperation(&deleteDir);
#else
    const auto deleteTree =
        [](const char* path, const struct stat*, int, struct FTW*) -> int
        {
            ::remove(path);
            return 0;
        };

    nftw(basePath.c_str(), deleteTree, 32, FTW_DEPTH);
#endif
}
