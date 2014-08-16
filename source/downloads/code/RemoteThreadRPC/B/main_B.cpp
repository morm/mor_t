#include <Windows.h>
#include <tchar.h>

#include <thread>
#include <chrono>
#include <atomic>
#include <string>

std::atomic_flag working = {true};

std::wstring input_string;

int foo(const wchar_t* str)
{
	input_string = str;

	working.clear();

	return 0xAA55AA55;
}

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	auto ev = OpenEvent(EVENT_ALL_ACCESS, FALSE, _T("B_ready_mutex"));
	SetEvent(ev);

	while(working.test_and_set())
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	
	::MessageBox(NULL, input_string.c_str(), _T("Remote Call"), MB_OK);

	CloseHandle(ev);

	return 0;
}