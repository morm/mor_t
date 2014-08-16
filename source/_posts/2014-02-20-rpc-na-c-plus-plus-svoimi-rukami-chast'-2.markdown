---
layout: post
title: "RPC на C++ своими руками. Часть 2"
date: 2014-02-20 21:20:38 +0400
comments: true
categories: RPC
---
Продолжение статьи об удаленном вызове функции с помощью CreateRemoteThread. Эта часть посвящена вызову функции с передачей параметра и возвратом кода результата.
# RPC через CreateRemoteThread под Windows с передачей параметров в функцию
<!-- more -->
Итак, в первой части я сделал вызов функции одного процесса из другого. Но вызов этот был не вполне полноценным, потому что я не анализировал результат выполнения и не передавал параметры на вход.
Для начала определимся, как вернуть результат из вызываемой функции. Так как функция у нас вызывается с помощью **CreateRemoteThreadEx**, то получить результат выполнения потока можно с помощью **GetExitCodeThread**. А это накладывает очевидное ограничение – эта функция может вернуть только **4 байта**. Печально. Но если нужно вернуть больше, можно воспользоваться возвращением результата через аргументы функции.
Переходим к передаче параметров функции. И опять ограничивающим фактором является использование **CreateRemoteThreadEx**, который позволяет передать в функцию аргумент размером: *на x86* – **4 байта**, *на x64* – **8 байт**.
{% include_code Меняем сигнатуру вызываемой функции lang:cpp RemoteThreadRPC/B/main_B.cpp %}
В RPC_caller я перенес в конструктор подгрузку целевого модуля, что позволит не делать этого при каждом вызове. А также функция run была шаблонизирована, чтобы обрабатывать различные типы возврата функции в пределах 4х байт.
{% include_code Шаблонизированный RPC_caller lang:cpp RemoteThreadRPC/A/rpc_caller.hpp %}
А теперь подробнее о ключевых моментах:
```cpp
    template <class R, class T, int time_out>
	R call_in_ipc(const std::string& func_name, T* val, int size)
	{
		assert(hProcess());
		
		int remote_proc = (int)GetProcAddress(hModule(), func_name.c_str());
		if(!remote_proc)
			throw std::invalid_argument(std::string("No function ") + func_name + " found");

		return std::move(run<typename R, typename T, time_out>(hProcess(), remote_proc - (int)hModule(), func_name, val, size));
```
При инстанциировании задается время ожидания выполнения целевой функции - time_out.

Для передачи параметра в удаленную функцию используется функция **VirtualAllocEx**. Она позволяет выделить память в целевом процессе, после чего можно заполнить ее требуемыми данными. Указатель на заполненный буфер затем передается как параметр в функцию **CreateRemoteThreadEx**.
```cpp
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
```
Для того чтобы корректно обработать возвращаемый результат, введена функция **transform_return**. Она позволяет наложить маску на возвращаемые **GetExitCodeThread** 4 байта. Специализированная функция для типа bool нужна для того, чтобы избежать предупреждения компилятора о принудительном приведении к типу bool. 
```cpp
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
```
Для того чтобы передать более сложную структуру, придется немного попотеть. Может потом как-нибудь опишу.

Осталось подвести некоторые итоги.

Плюсы:

* Передаваемые данные находятся только в памяти взаимодействующих процессов
* Вызов достаточно прост и надежен

Минусы:

* Ограничение на количество параметров
* Ограничение на возвращаемый тип

В следующий раз опишу взаимодействие через boost::ipc - это очень интересная и полезная библиотека.

[Исходный код]({{site.root}}/downloads/RemoteThreadRPC.zip)
