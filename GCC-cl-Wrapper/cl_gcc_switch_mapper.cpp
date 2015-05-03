/*
 * (c) 2015- The Hourglass Resurrection Team
 * This wrappers source code is released under the GPLv2 license.
 * Refer to the file LICENSE included in the project root.
 */

#include <Windows.h>

#include <cwchar>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "cl_gcc_switch_mapper.h"

namespace {
    class WideStringCompare {
    public:
        bool operator() (LPCWSTR a, LPCWSTR b) const
        {
            return wcscmp(a, b) < 0;
        }
    };
    /*
     * MSVC 2010 sucks too much to support c++0x initializer lists for std::map.
     */
    const std::map<LPCWSTR, LPCWSTR, WideStringCompare> InitSwitchMap()
    {
        /*
         * Switches that translates into "" are either not applicable for GCC, or they are
         * on-by-default. In which case the reverse might have a switch added instead.
         */
        std::map<LPCWSTR, LPCWSTR, WideStringCompare> m;
        m[L"/c"]                  = L" -c";
        /*m[L"/I"]                  = L"-I";*/ /* Added for reference, /I has no delimiter */
        m[L"/Zi"]                 = L"";
        m[L"/nologo"]             = L"";
        m[L"/Wall"]               = L" -Wall";
        m[L"/WX"]                 = L" -Werror";
        m[L"/WX-"]                = L"";
        m[L"/Ox"]                 = L" -O3";
        m[L"/Ob2"]                = L"";
        m[L"/Oi"]                 = L"";
        m[L"/Ot"]                 = L"";
        m[L"/Oy"]                 = L"";
        m[L"/Oy-"]                = L" -fno-omit-frame-pointer";
        m[L"/GT"]                 = L"";
        m[L"/GL"]                 = L" -flto";
        m[L"/D"]                  = L" -D";
        m[L"/Gm-"]                = L"";
        m[L"/EHa"]                = L"";
        m[L"/MT"]                 =	L" -D_MT"; /* Linker has to link against LIBCMT.lib */
        m[L"/GS-"]                = L"";
        m[L"/fp:precise"]         = L" -frounding-math -fsignaling-nans";
        m[L"/Zc:auto"]            = L"";
        m[L"/Zc:wchar_t"]         = L"";
        m[L"/Zc:forScope"]        = L"";
        m[L"/Fo"]                 = L" -o";
        m[L"/Fd"]                 = L"";
        m[L"/Gd"]                 = L"";
        m[L"/TP"]                 = L"";
        m[L"/analyze-"]           = L"";
        m[L"/errorReport:prompt"] = L"";
        /*
         * We need to catch our own custom switch as well.
         */
        m[L"/GCCBuild"]           = L"";

        return m;
    }

    const std::map<LPCWSTR, LPCWSTR, WideStringCompare> switch_map = InitSwitchMap();
};

const std::vector<std::wstring> BuildGCCCommandLines(const std::wstring& command_line)
{
    std::vector<std::wstring> build_command_lines_list;
    std::vector<std::wstring> files_to_build;
    /*
     * Always build with c++11 features enabled. Should use c++14 later.
     */
    std::wstring gcc_command_line(L"-v --std=c++11");
    /*
     * Default to current directory if /Fo is not set.
     */
    std::wstring output_path(L".\\");
    LPCWSTR opt = nullptr;

    for (std::wstring::size_type sub_string_pos = command_line.find_first_of(L" /\"");
         sub_string_pos != std::wstring::npos;
         sub_string_pos = command_line.find_first_of(L" /\"", sub_string_pos + 1))
    {
        std::wstring::size_type sub_string_length;
        sub_string_length = command_line.find_first_of(L" /\"", sub_string_pos + 1) - sub_string_pos;

        /*
         * Strings smaller than 2 chars in length are not valid, they are probably the result of
         * 2 delimiters showing up after each other. Just ignore these strings.
         */
        std::wstring sub_string = command_line.substr(sub_string_pos, sub_string_length);
        if (sub_string.length() < 2)
        {
            continue;
        }
        try
        {
            opt = switch_map.at(sub_string.c_str());
            /*
             * The value of /Fo depends on the input file, so that needs special handling.
             */
            if (sub_string.find(L"/Fo") != 0)
            {
                gcc_command_line.append(opt);
            }
        }
        catch (const std::out_of_range&)
        {
            if (opt == nullptr)
            {
                break;
            }

            if (wcscmp(opt, L" -D") == 0)
            {
                gcc_command_line.append(sub_string, 1, std::wstring::npos);
            }
            else if (sub_string.find(L".cpp") != std::wstring::npos)
            {
                files_to_build.push_back(sub_string.substr(1));
            }
            /*
             * /I gets special treatment because it has no delimiter between switch and directory.
             */
            else if (sub_string.find(L"/I") == 0)
            {
                sub_string.replace(0, 1, L"-");
                gcc_command_line.append(L" ");
                gcc_command_line.append(sub_string);
            }
            else if (wcscmp(opt, L" -o") == 0)
            {
                output_path = sub_string.substr(1);
            }
        }
    }

    gcc_command_line.append(L" -o ");
    gcc_command_line.append(output_path);

    for (std::vector<std::wstring>::iterator i = files_to_build.begin();
         i != files_to_build.end();
         i++)
    {
        /*
         * We already know by here that the entry in files_to_build has .cpp somewhere.
         * We must also strip any prefixing directories.
         */
        std::wstring::size_type sub_string_start = i->rfind(L'\\');
        if (sub_string_start == std::wstring::npos)
        {
            sub_string_start = 0;
        }
        else
        {
            sub_string_start += 1;
        }
        std::wstring command_line_end(i->substr(sub_string_start, i->find(L".cpp") - sub_string_start));
        command_line_end.append(L".obj ");
        command_line_end.append(*i);
        build_command_lines_list.push_back(gcc_command_line + command_line_end);
    }

    return build_command_lines_list;
}