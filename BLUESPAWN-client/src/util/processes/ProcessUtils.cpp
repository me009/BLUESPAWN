#include "util/processes/ProcessUtils.h"

#include "util/log/Log.h"
#include "util/filesystem/FileSystem.h"

typedef struct _CURDIR {
    UNICODE_STRING DosPath;
    PVOID Handle;
} CURDIR, * PCURDIR;

typedef struct _RTL_DRIVE_LETTER_CURDIR {
    WORD Flags;
    WORD Length;
    ULONG TimeStamp;
    STRING DosPath;
} RTL_DRIVE_LETTER_CURDIR, * PRTL_DRIVE_LETTER_CURDIR;


typedef struct _RTL_USER_PROCESS_PARAMETERS_ {
    ULONG MaximumLength;
    ULONG Length;
    ULONG Flags;
    ULONG DebugFlags;
    PVOID ConsoleHandle;
    ULONG ConsoleFlags;
    PVOID StandardInput;
    PVOID StandardOutput;
    PVOID StandardError;
    CURDIR CurrentDirectory;
    UNICODE_STRING DllPath;
    UNICODE_STRING ImagePathName;
    UNICODE_STRING CommandLine;
    PVOID Environment;
    ULONG StartingX;
    ULONG StartingY;
    ULONG CountX;
    ULONG CountY;
    ULONG CountCharsX;
    ULONG CountCharsY;
    ULONG FillAttribute;
    ULONG WindowFlags;
    ULONG ShowWindowFlags;
    UNICODE_STRING WindowTitle;
    UNICODE_STRING DesktopInfo;
    UNICODE_STRING ShellInfo;
    UNICODE_STRING RuntimeData;
    RTL_DRIVE_LETTER_CURDIR CurrentDirectores[32];
    ULONG EnvironmentSize;
} RTL_USER_PROCESS_PARAMETERS_, * PRTL_USER_PROCESS_PARAMETERS_;

bool HookIsOkay(const Hook& hook){
	// Once Detours is set up, this will become significantly more complicated...
	return false;
}

std::vector<LPVOID> GetExecutableNonImageSections(DWORD pid){
	// Make use of APIs in PE Sieve...
	return {};
}

std::wstring GetProcessCommandline(const HandleWrapper& process){
	if(process){
		PROCESS_BASIC_INFORMATION information{};
		NTSTATUS status = Linker::NtQueryInformationProcess(process, ProcessBasicInformation, &information, sizeof(information), nullptr);
		if(NT_SUCCESS(status)){
			auto peb = information.PebBaseAddress;
            RTL_USER_PROCESS_PARAMETERS_ params{};
            if(!ReadProcessMemory(process, &peb->ProcessParameters, &params, sizeof(params), nullptr)){
                LOG_ERROR("Unable to read memory from process with PID " << GetProcessId(process) << " to find its command line (error " << GetLastError() << ")");
                return {};
            }

            DWORD dwLength = params.CommandLine.Length;
            auto cmdline = AllocationWrapper{ new WCHAR[dwLength / 2 + 1], dwLength + 2, AllocationWrapper::CPP_ARRAY_ALLOC };
            if(!ReadProcessMemory(process, &peb->ProcessParameters, &params, sizeof(params), nullptr)){
                LOG_ERROR("Unable to read memory from process with PID " << GetProcessId(process) << " to find its command line (error " << GetLastError() << ")");
                return {};
            }

            if(!ReadProcessMemory(process, &params.CommandLine.Buffer, cmdline, dwLength, nullptr)){
                LOG_ERROR("Unable to read memory from process with PID " << GetProcessId(process) << " to find its command line (error " << GetLastError() << ")");
                return {};
            }
            cmdline.SetByte(dwLength, 0);
            cmdline.SetByte(dwLength + 1, 0);

            return std::wstring{ reinterpret_cast<PWCHAR>(LPVOID(cmdline)) };
		} else {
			LOG_ERROR("Unable to query information from process with PID " << GetProcessId(process) << " to find its command line (error " << status << ")");
            return {};
		}
	} else {
		LOG_ERROR("Unable to get command line of invalid process");
		return {};
	}
}

std::wstring GetProcessCommandline(DWORD dwPID){
    HandleWrapper process{ OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, false, dwPID) };
    if(process){
        return GetProcessCommandline(process);
    } else {
        LOG_ERROR("Unable to open process with PID " << dwPID << " to find its command line (error " << GetLastError() << ")");
        return {};
    }
}

std::wstring GetImagePathFromCommand(const std::wstring& wsCmd){
    WCHAR path[MAX_PATH]{};
    auto start = wsCmd.find_first_not_of(L" \t\n\r", 0);
    if(wsCmd.at(start) == '"' || wsCmd.at(start) == '\''){
        return wsCmd.substr(start, wsCmd.find_first_of(L"'\"", start) - start);
    } else {
        auto idx = start;
        while(idx != std::wstring::npos){
            auto spacepos = wsCmd.find(L" ", idx);
            if(spacepos == std::wstring::npos){
                return wsCmd.substr(start);
            } else if(reinterpret_cast<ULONG_PTR>(FindExecutableW(wsCmd.substr(start, spacepos - start).c_str(), nullptr, path)) > 32){
                return path;
            } else {
                idx = spacepos + 1;
            }
        }

        return wsCmd.substr(start, wsCmd.find_first_of(L" \t\n\r", start) - start);
    }
}