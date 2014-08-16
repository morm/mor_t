#ifndef RPC_CALLER_HPP
#define RPC_CALLER_HPP

#pragma comment( lib, "psapi" )

#include <Windows.h>
#include <tchar.h>

#include <tlhelp32.h>
#include <Psapi.h>

#include <vector>
#include <string>
#include <cstdint>

typedef std::basic_string<TCHAR, std::char_traits<TCHAR>, std::allocator<TCHAR> > TString;

class LibHolder
{
public:
	LibHolder(HMODULE hLib) : _hLib(hLib){}
	~LibHolder()
	{
		if(_hLib)
			FreeLibrary(_hLib);
	}
	HMODULE operator()()
	{
		return _hLib;
	}
private:
	HMODULE _hLib;
};

class HandleHolder
{
public:
	HandleHolder(HANDLE handle) : _handle(handle){}
	~HandleHolder()
	{
		if(_handle)
			CloseHandle(_handle);
	}
	HANDLE operator()()
	{
		return _handle;
	}
private:
	HANDLE _handle;
};

class RPC_caller
{
public:
	explicit RPC_caller(const TString& module_name) : mod_name(module_name){}

	bool call_in_ipc(const std::string& func_name)
	{
		bool res = false;
		LibHolder hModule(LoadLibraryEx(mod_name.c_str(), NULL, 0));
		
		std::uintptr_t remote_proc = (std::uintptr_t)GetProcAddress(hModule(), func_name.c_str());
		if(!remote_proc)
			return res;


		PROCESSENTRY32 entry;
		entry.dwSize = sizeof(PROCESSENTRY32);

		HandleHolder snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL));

		if (Process32First(snapshot(), &entry) == TRUE)
		{
			while (Process32Next(snapshot(), &entry) == TRUE)
			{
				if (mod_name.compare(entry.szExeFile) == 0)
				{  
					HandleHolder hProcess(OpenProcess(PROCESS_ALL_ACCESS, FALSE, entry.th32ProcessID));
					if(!hProcess())
						continue;

					res = run(hProcess(), remote_proc - (std::uintptr_t)hModule(), func_name);

					break;
				}
			}
		}

		return res;
	}
private:
	bool run(HANDLE hProcess, std::uintptr_t remote_proc, const std::string& func_name)
	{
		void* param = nullptr;

		std::uintptr_t addr = this->GetModuleBase(hProcess, mod_name) + remote_proc;
		auto hThread = CreateRemoteThreadEx(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)(addr), param, 0, NULL, NULL);
		return (WaitForSingleObject(hThread, 3000) == WAIT_OBJECT_0);

	}
	std::uintptr_t GetModuleBase(HANDLE hProc, const TString& sModuleName) 
	{
		TCHAR szBuf[255]; 
		DWORD cModules = 0; 
		std::uintptr_t dwBase = -1;

		EnumProcessModules(hProc, NULL, 0, &cModules); 
		
		DWORD len = cModules/sizeof(HMODULE);
		std::vector<HMODULE> hModules(len);

		if(EnumProcessModulesEx(hProc, &hModules.front(), len, &cModules, LIST_MODULES_ALL)) 
		{ 
			for(std::size_t i = 0; i < len; ++i) 
			{
				if(GetModuleBaseName(hProc, hModules[i], szBuf, sizeof(szBuf))) 
				{ 
					if(sModuleName.compare(szBuf) == 0) 
					{
						dwBase = reinterpret_cast<std::uintptr_t>(hModules[i]);
						break; 
					} 
				} 
			}
			len = cModules/sizeof(HMODULE);
			hModules.resize(len);
		} 

		return dwBase; 
	}
private:
	TString mod_name;
};
#endif