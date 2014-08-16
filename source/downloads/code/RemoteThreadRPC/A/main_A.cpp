#include <tchar.h>

#include "rpc_caller.hpp"

template <class T>
class rpc_allocator : public std::allocator<T>
{
};

int _tmain(int argc, TCHAR* argv[])
{
	std::basic_string<TCHAR, std::char_traits<TCHAR>, rpc_allocator<TCHAR> > ttt;

	TString full_path(argv[0]);
	TString path = full_path.substr(0, full_path.find_last_of(_T("/\\")) + 1);
	
	HandleHolder ev(CreateEvent(NULL, FALSE, FALSE, _T("B_ready_mutex")));
	
	char buf[256];
	wcstombs ( buf, (TString(_T("start ")) + path.c_str() + TString(_T("B.exe"))).c_str(), sizeof(buf) );
	std::system(buf);
	
	wchar_t* str = _TEXT("this string was transfered from A module");
	try
	{
		if(WaitForSingleObject(ev(), 3000) == WAIT_OBJECT_0)
			std::cout << "foo ret is " << std::hex << RPC_caller(_T("B.exe")).call_in_ipc<int, TCHAR, 3000>("foo", str, wcslen(str)*sizeof(str[0])) << std::endl;
	}
	catch(std::exception& err)
	{
		std::cout << err.what() << std::endl;
		return 1;
	}
	return 0;
}