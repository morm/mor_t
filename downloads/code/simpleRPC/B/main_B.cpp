#include <Windows.h>
#include <tchar.h>

#include <thread>
#include <chrono>
#include <atomic>

std::atomic_flag working = {true};

void foo()
{
	working.clear();
}

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	auto ev = OpenEvent(EVENT_ALL_ACCESS, FALSE, _T("B_ready_mutex"));
	SetEvent(ev);

	while(working.test_and_set())
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	
	::MessageBox(NULL, _T("call succeeded"), _T("Remote Call"), MB_OK);

	CloseHandle(ev);

	return 0;
}