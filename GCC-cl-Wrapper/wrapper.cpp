/*
 * (c) 2015- The Hourglass Resurrection Team
 * This wrappers source code is released under the GPLv2 license.
 * Refer to the file LICENSE included in the project root.
 */

/*
 * This wrapper was designed to support Visual Studio 2010 SP1.
 * While it should work with other versions of Visual Studio it has not been tested.
 * It also does not implement a mapping for all of the command line switches to cl.exe,
 * just the switches used by Hourglass-Resurrection, adding new switches are however fairly easy,
 * simply add the mapping to the list cl_gcc_switch_mapper.cpp file.
 */
#include <Windows.h>
#include <Shlwapi.h>

#include <string>
#include <vector>

#include "cl_gcc_switch_mapper.h"

/*
 * Since it's not possible to easily find the directory where gcc.exe exists, a file with the
 * directory simplifies things greatly.
 * It's not guaranteed that the path exists in the environment variables nor the registry, and
 * searching for it by going through all the files in the system is just not good practice.
 */
static const std::wstring MINGW32_BINDIR_FILENAME = L"mingw32_bindir.txt";

/*
 * This function is necessary because CreateProcess will search the PATH variable last,
 * meaning we will otherwise find ourselves before the real cl.exe and become a fork-bomb.
 */
std::wstring GetCLExecutableWithPath()
{
    /*
     * As defined by Microsoft on MSDN, to my knowledge this is not declared in the Windows API.
     * -- Warepire
     */
    static const std::wstring::size_type MAX_ENVIRONMENT_VARIABLE_SIZE = 32767;
    DWORD returned_wchars = 0;
    std::wstring::size_type cl_path_begin = 0;
    std::wstring::size_type cl_path_end = 0;
    ptrdiff_t point_in_sub_string = 0;
    std::wstring cl_path;
    PWSTR sub_string_pointer = nullptr;
    std::wstring buffer;
    buffer.resize(MAX_ENVIRONMENT_VARIABLE_SIZE);

    /*
     * Use buffer.size() as nSize parameter for extra safety
     */
    returned_wchars = GetEnvironmentVariable(L"Path", const_cast<LPWSTR>(buffer.data()), buffer.size());
    if (returned_wchars == 0)
    {
        /* 
         * The contents of buffer are undefined, so make them defined again ( = empty)
         */
        buffer.clear();
        return buffer;
    }

    /*
     * Check if the value exists at all, and if it does, remember it's position.
     * Do the search case-insensitive as Visual Studio may change the PATH variable
     * content case-formatting between versions.
     */
    sub_string_pointer = StrStrIW(buffer.c_str(), L"\\VC\\BIN");
    if (sub_string_pointer == nullptr)
    {
        buffer.clear();
        return buffer;
    }

    point_in_sub_string = sub_string_pointer - buffer.c_str();
    /*
     * Find the path among the paths, semi-colons are delimiters.
     */
    for (; static_cast<uintptr_t>(point_in_sub_string) > cl_path_end;
         cl_path_begin = cl_path_end, cl_path_end = buffer.find(L';', cl_path_end)) {}
    /*
     * If we cannot find any semi-colons, we need to set cl_path_begin to 0 again.
     */
    if (cl_path_begin == std::wstring::npos)
    {
        cl_path_begin = 0;
    }
    cl_path = buffer.substr(cl_path_begin, cl_path_end);
    cl_path.append(L"\\cl.exe");

    return cl_path;
}

std::wstring GetGCCExecutableWithPath()
{
    std::wstring gcc_path;
    HANDLE mingw32_bindir_file = CreateFileW(MINGW32_BINDIR_FILENAME.c_str(),
                                             GENERIC_READ,
                                             0,
                                             nullptr,
                                             OPEN_EXISTING,
                                             FILE_ATTRIBUTE_NORMAL,
                                             nullptr);
    LARGE_INTEGER file_size;
    if (GetFileSizeEx(mingw32_bindir_file, &file_size) == FALSE)
    {
        DWORD error = GetLastError();
        CloseHandle(mingw32_bindir_file);
        ExitProcess(error);
    }
    if (file_size.HighPart != 0)
    {
        CloseHandle(mingw32_bindir_file);
        ExitProcess(ERROR_FILE_TOO_LARGE);
    }

    gcc_path.resize(file_size.LowPart / sizeof(WCHAR));

    DWORD read_bytes;
    BOOL read_file_result = ReadFile(mingw32_bindir_file,
                                     const_cast<LPWSTR>(gcc_path.data()),
                                     file_size.LowPart,
                                     &read_bytes,
                                     nullptr);
    if (read_file_result == FALSE)
    {
        DWORD error = GetLastError();
        CloseHandle(mingw32_bindir_file);
        ExitProcess(error);
    }

    /*
     * We encountered an UTF-16 BOM, skip it.
     */
    if (gcc_path.compare(0, 1, L"\xFEFF") == 0)
    {
        gcc_path.erase(0, 1);
    }

    CloseHandle(mingw32_bindir_file);

    gcc_path.shrink_to_fit();
    gcc_path.append(L"\\gcc.exe");

    return gcc_path;
}

DWORD Build(const std::wstring& compiler, const std::wstring& command_line)
{
    std::wstring command_line_non_const_copy(command_line);
    BOOL bool_ret;
    DWORD dword_ret;
    DWORD build_status;
    STARTUPINFOW startup_info;
    PROCESS_INFORMATION process_info;

    ZeroMemory(&startup_info, sizeof(STARTUPINFOW));
    ZeroMemory(&process_info, sizeof(PROCESS_INFORMATION));
    startup_info.cb = sizeof(STARTUPINFOW);

    /*
     * Pipe the compiler output into Visual Studio by sharing our own std-handles.
     */
    startup_info.dwFlags = STARTF_USESTDHANDLES;
    startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup_info.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup_info.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    /*
     * CreateProcess is quite evil. The way CreateProcess behaves when supplied with both
     * lpApplicationName and lpCommandLine is that the parameters given on the command line will be
     * fed into the program starting in argv[0]!
     * This causes quite the headache, as many programs developed (including those by Microsoft)
     * will assume the name of the executable it was started as is placed in argv[0], as according
     * to the C-standard. C++ is however a little more forgiving about this, but the Windows API is
     * a C API!
     * Source: https://support.microsoft.com/en-us/kb/175986
     * Copy the name of the application first in the parameter list to work around this issue.
     * -- Warepire
     */
    //command_line_non_const_copy.insert(0, compiler.substr(compiler.rfind(L'\\') + 1) + L" ");
    command_line_non_const_copy.insert(0, L"\"" + compiler + L"\" ");
    bool_ret =  CreateProcessW(compiler.c_str(), 
                               const_cast<LPWSTR>(command_line_non_const_copy.c_str()),
                               nullptr,
                               nullptr,
                               TRUE,
                               CREATE_NO_WINDOW,
                               nullptr,
                               nullptr,
                               &startup_info,
                               &process_info);
    if (bool_ret == FALSE)
    {
        DWORD error = GetLastError();
        CloseHandle(process_info.hProcess);
        CloseHandle(process_info.hThread);
        ExitProcess(error);
    }

    dword_ret = WaitForSingleObject(process_info.hProcess, INFINITE);

    if (dword_ret == WAIT_FAILED)
    {
        DWORD error = GetLastError();
        CloseHandle(process_info.hProcess);
        CloseHandle(process_info.hThread);
        ExitProcess(error);
    }
    /*
     * WAIT_ABANDONED cannot happen on processes,
     * and WAIT_TIMEOUT cannot happen with INFINITE timeout
     */

    bool_ret = GetExitCodeProcess(process_info.hProcess, &build_status);
    if (bool_ret == 0)
    {
        DWORD error = GetLastError();
        CloseHandle(process_info.hProcess);
        CloseHandle(process_info.hThread);
        ExitProcess(error);
    }

    CloseHandle(process_info.hProcess);
    CloseHandle(process_info.hThread);
    return build_status;
}

/*
 * true = build with GCC
 * false = build with CL
 */
bool TestCompileWithGCC(const std::wstring& command_line)
{
    return command_line.find(L"/GCCBuild") != std::wstring::npos;
}

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR lpCmdLine, int)
{
    std::wstring command_line(lpCmdLine);
    DWORD return_code = 0;

    /*
     * We might be fed command line switches from a file, so check for that marker (@).
     * If '@' is somewhere else than the first char, or second if the first char is a '"',
     * or not present at all, we must consider it part of the command line switches.
     */
    std::wstring::size_type substr_offset = 0;
    if (command_line.find('@') == 0)
    {
        substr_offset = 1;
    }
    else if (command_line.find(L"\"@") == 0)
    {
        substr_offset = 2;
    }
    if (substr_offset > 0)
    {
        HANDLE command_line_file = CreateFileW(command_line.substr(substr_offset).c_str(),
                                               GENERIC_READ,
                                               0,
                                               nullptr,
                                               OPEN_EXISTING,
                                               FILE_ATTRIBUTE_NORMAL,
                                               nullptr);
        LARGE_INTEGER file_size;
        if (GetFileSizeEx(command_line_file, &file_size) == FALSE)
        {
            DWORD error = GetLastError();
            CloseHandle(command_line_file);
            ExitProcess(error);
        }
        if (file_size.HighPart != 0)
        {
            CloseHandle(command_line_file);
            ExitProcess(ERROR_FILE_TOO_LARGE);
        }
        /*
         * Contents of command_line are no longer important to us,
         * so we can re-use it to store the contents of the file.
         */
        command_line.clear();
        /*
         * UTF-16 uses 2 bytes per char, we need to divide by 2 to get a closer approximation
         * of the number of chars in the file. It will still be too big, but less so.
         */
        command_line.resize(file_size.LowPart / sizeof(WCHAR));

        DWORD read_bytes;
        BOOL read_file_result = ReadFile(command_line_file,
                                         const_cast<LPWSTR>(command_line.data()),
                                         file_size.LowPart,
                                         &read_bytes,
                                         nullptr);
        if (read_file_result == FALSE)
        {
            DWORD error = GetLastError();
            CloseHandle(command_line_file);
            ExitProcess(error);
        }

        /*
         * We encountered an UTF-16 BOM, skip it.
         */
        if (command_line.compare(0, 1, L"\xFEFF") == 0)
        {
            command_line.erase(0, 1);
        }

        CloseHandle(command_line_file);
    }

    if (TestCompileWithGCC(command_line) == true)
    {
        std::wstring gcc_compiler = GetGCCExecutableWithPath();
        std::vector<std::wstring> command_lines = BuildGCCCommandLines(command_line);
        for (std::vector<std::wstring>::iterator i = command_lines.begin();
             i != command_lines.end();
             i++)
        {
            return_code = Build(gcc_compiler, *i);
            if (return_code != ERROR_SUCCESS)
            {
                break;
            }
        }
    }
    else
    {
        std::wstring cl_compiler = GetCLExecutableWithPath();
        return_code = Build(cl_compiler, command_line);
    }

    ExitProcess(return_code);
}
