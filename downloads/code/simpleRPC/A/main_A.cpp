#include "rpc_caller.hpp"

#include <locale>

int _tmain(int argc, TCHAR* argv[])
{
	std::locale::global(std::locale(""));

	TString full_path(argv[0]);
	TString path = full_path.substr(0, full_path.find_last_of(_T("/\\")) + 1);
	
	HandleHolder ev(CreateEvent(NULL, FALSE, FALSE, _T("B_ready_mutex")));
	
	char buf[256];
	wcstombs ( buf, (TString(_T("start ")) + path.c_str() + TString(_T("B.exe"))).c_str(), sizeof(buf) );
	std::system(buf);
	
	if(WaitForSingleObject(ev(), 3000) == WAIT_OBJECT_0)
		RPC_caller(_T("B.exe")).call_in_ipc("foo");

	return 0;
}