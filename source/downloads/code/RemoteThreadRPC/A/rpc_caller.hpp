#ifndef RPC_CALLER_HPP
#define RPC_CALLER_HPP

#pragma comment( lib, "psapi" )

#include <Windows.h>

#include <cstdio>
#include <tlhelp32.h>
#include <TlHelp32.h>
#include <Psapi.h>
#include <tchar.h>

#include <vector>
#include <string>
#include <iostream>
#include <cassert>
#include <locale>
#include <codecvt>

typedef std::basic_string<TCHAR, std::char_traits<TCHAR>, std::allocator<TCHAR> > TString;

class rpc_call_error : public std::exception
{
	std::string msg;
public:
	rpc_call_error(const std::string& err)
	{
		msg = err;
	}
	rpc_call_error(const std::wstring& err)
	{
		msg = std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(err);
	}
	const char* what() const override
	{
		return msg.c_str();
	}
};

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
	HANDLE _handle;
public:
	HandleHolder() : _handle(0)
	{}
	HandleHolder(HANDLE handle) : _handle(handle){}
	~HandleHolder()
	{
		try
		{
			Free();
		}
		catch(...)
		{}
	}
	HandleHolder& operator = (const HANDLE& hVal)
	{
		Free();
		_handle = hVal;

		return *this;
	}
	HANDLE operator()()
	{
		return _handle;
	}
private:
	void Free()
	{
		if(_handle)
			CloseHandle(_handle);
	}
};

class RPC_caller
{
	LibHolder hModule;
	HandleHolder hProcess;
	DWORD base;
public:
	explicit RPC_caller(const TString& module_name) : hModule(LoadLibraryEx(module_name.c_str(), NULL, 0))
	{
		PROCESSENTRY32 entry;
		entry.dwSize = sizeof(PROCESSENTRY32);

		HandleHolder snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL));

		if (Process32First(snapshot(), &entry) == TRUE)
		{
			while (Process32Next(snapshot(), &entry) == TRUE)
			{
				if (module_name.compare(entry.szExeFile) == 0)
				{  
					hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, entry.th32ProcessID);
					if(!hProcess())
						throw rpc_call_error(TString(_TEXT("Unable to open process ")) + module_name);
					base = this->GetModuleBase(hProcess(), module_name);
				}
			}
		}
	}

	template <class R, class T, int time_out>
	R call_in_ipc(const std::string& func_name, T* val, int size)
	{
		assert(hProcess());
		
		int remote_proc = (int)GetProcAddress(hModule(), func_name.c_str());
		if(!remote_proc)
			throw std::invalid_argument(std::string("No function ") + func_name + " found");

		return std::move(run<typename R, typename T, time_out>(hProcess(), remote_proc - (int)hModule(), func_name, val, size));
	}
private:
	template <class R, class T, int time_out>
	R run(HANDLE hProcess, int remote_proc, const std::string& func_name, T* user_param, int size)
	{
		static_assert(sizeof(R) <= sizeof(DWORD), "Only 4 byte types can be used as return type of a run function");
		void* param = nullptr;

		if(user_param)
		{
			param = VirtualAllocEx(hProcess, NULL, size, MEM_COMMIT, PAGE_READWRITE);
			WriteProcessMemory(hProcess, param, (void*)user_param, size, NULL);
		}

		int addr = base + remote_proc;
		auto hThread = CreateRemoteThreadEx(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)(addr), param, 0, NULL, NULL);
		if(WaitForSingleObject(hThread, time_out) != WAIT_OBJECT_0)
			throw std::runtime_error("call failed");

		DWORD res = -1;
		GetExitCodeThread(hThread, &res);

		return transform_return<R>(res);
	}
	template <class R>
	R transform_return(const DWORD& res)
	{
		static_assert(sizeof(R) <= 4, "Return type can not be longer than 4 bytes");

		static const int mask[] = {0xFF, 0xFFFF, 0xFFFFFF, 0xFFFFFFFF};
		return std::move(R(res & mask[sizeof(R) - 1]));
	}
	template <>
	bool transform_return(const DWORD& res)
	{
		return std::move((res & 0xFF)!=0);
	}

	DWORD GetModuleBase(HANDLE hProc, const TString& sModuleName) 
	{
		TCHAR szBuf[255]; 
		DWORD cModules = 0; 
		DWORD dwBase = -1;

		if(!EnumProcessModules(hProc, NULL, 0, &cModules))
			throw rpc_call_error(TString(_TEXT("Module ")) + sModuleName + _TEXT(" is not loaded"));
		
		auto len = cModules/sizeof(HMODULE);
		std::vector<HMODULE> hModules(len);

		if(EnumProcessModules(hProc, &hModules.front(), len, &cModules)) 
		{ 
			for(std::size_t i = 0; i < len; ++i) 
			{
				if(GetModuleBaseName(hProc, hModules[i], szBuf, sizeof(szBuf))) 
				{ 
					if(sModuleName.compare(szBuf) == 0) 
					{
						dwBase = (DWORD)hModules[i]; 
						break; 
					} 
				} 
			}
			len = cModules/sizeof(HMODULE);
			hModules.resize(len);
		} 

		return dwBase; 
	}
};
#endif