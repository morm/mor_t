---
layout: post
title: "RPC на C++ своими руками. Часть 1"
date: 2014-01-05 20:56:05 +0400
comments: true
categories: RPC
---
# RPC через CreateRemoteThread под Windows
Мое исследование возможных реализаций RPC. Для начала рассмотрим реализацию RPC через создание потока в удаленном процессе.
<!-- more -->
Одной из основных проблем при проектировании сложных систем является обеспечение взаимодействия между различными комплексами. Трудно представить, какое количество велосипедов было изобретено и ежеминутно куется в программных мануфактурах. Сетевое взаимодействие так возбуждает системных архитекторов, что они просто не могут остановиться и не написать протокол информационно-логического взаимодействия страниц так на 150. В таких протоколах обычно расписывается куча квитанций, которые обеспечивают надежность передачи данных. Самый эпичный велосипед, который я видел, реализовывал функциональность TCP поверх UDP.
Интересно, что самый предпочтительный с точки зрения прозрачности и дальнейшей поддержки подход  – RPC – считается сложным и ресурсоемким. И, если с последним можно отчасти согласиться, то вот насчет сложности можно поспорить. Для того чтобы RPC стал простым и понятным, я решил для себя собрать возможные реализации RPC под различные платформы. Для начала попробую сделать вызов функции в другом исполняемом модуле под ОС Windows.
## Исходные данные:
* Есть два исполняемых модуля A.exe и B.exe
* В модуле B.exe есть функция foo
## Задача
* Выполнить из модуля A.exe функцию foo
## Решение
Для того, чтобы вызвать функцию, нужно знать ее адрес. Не все знают, но любой исполняемый модуль (exe  и dll) может экспортировать функции. При импорте функции мы узнаем [RVA](http://wiki.xakep.ru/rva-adres.ashx) функции в модуле. Для того чтобы получить полный адрес, нам нужно узнать базовый адрес загрузки модуля. Итак, в модуле B.exe объявляем функцию void foo() и экспортируем ее с помощью def файла.
{% include_code Код модуля B lang:cpp simpleRPC/B/main_B.cpp %}

{% include_code Экспорт функции lang:cpp simpleRPC/B/export.def %}

Итак, в модуле B.exe определена функция foo, которая при вызове выставит флаг working в значение false. После чего цикл в функции WinMain завершится после очередной итерации. Событие ev создается для того, чтобы модуль B.exe успел проинициализироваться прежде, чем мы начнем вызывать функцию foo.

Модуль A будет посложнее.
{% include_code Точка входа модуля A lang:cpp simpleRPC/A/main_A.cpp %}

Для начала получаем путь к B.exe. Этот танец с бубном вокруг path нужен для запуска по относительному пути в случае, если текущий путь отличается от папки где лежат A.exe и B.exe. 

{% include_code Класс удаленного вызова процедур lang:cpp simpleRPC/A/rpc_caller.hpp %}

Надеюсь, отсутствие комментариев не смущает, потому что код достаточно красноречив.
Итак, для чего нужен *RPC_caller*?
Функция *call_in_ipc*, собственно, вызывает удаленную функцию.

* Для начала **получаем RVA** удаленной функции. 
```cpp
      LibHolder hModule(LoadLibraryEx(mod_name.c_str(), NULL, 0));
      
      std::uintptr_t remote_proc = (std::uintptr_t)GetProcAddress(hModule(), func_name.c_str());
      if(!remote_proc)
          return res;
```

* Затем **делаем копию списка текущих процессов**. В этом списке нам нужно найти целевой модуль, из которого мы будем вызывать функцию.
```
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
``` 

* После того как мы найдем искомый модуль, необходимо **определить адрес загрузки модуля**. Собственно, после этого нам остается только сложить базовый адрес модуля и RVA функции - в результате получаем реальный адрес в удаленном модуле. 
```
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
```
* Теперь дело осталось за малым – запустить функцию в адресном пространстве другого процесса. Для этого в Windows существует функция *CreateRemoteThread*. Эта функция **создает поток в целевом процессе, который выполняет заданную функцию**. Собственно, поэтому в B.exe я использовал *std::atomic_flag*.
```
  bool run(HANDLE hProcess, std::uintptr_t remote_proc, const std::string& func_name)
  {
      void* param = nullptr;

      std::uintptr_t addr = this->GetModuleBase(hProcess, mod_name) + remote_proc;
      auto hThread = CreateRemoteThreadEx(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)(addr), param, 0, NULL, NULL);
      return (WaitForSingleObject(hThread, 3000) == WAIT_OBJECT_0);

  }
```
В результате мы получили вызов функции в удаленном процессе. Однако для полноценного RPC не хватает передачи в функцию параметров и приема результата выполнения. Об этом подробно расскажу во второй части.

[Исходный код]({{site.root}}/downloads/simpleRPC.zip)
