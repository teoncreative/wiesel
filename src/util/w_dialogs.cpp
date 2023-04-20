//
//    Copyright 2023 Metehan Gezer
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//         http://www.apache.org/licenses/LICENSE-2.0
//

#include "util/w_dialogs.hpp"
#include <nfd.h>
#include <thread>

namespace Wiesel {
	namespace Dialogs {
#ifdef __APPLE__
		void Init() {
			NFD_Init();
		}

		void OpenFileDialog(std::vector<FilterEntry> filters, std::function<void(const std::string&)> fn) {
			nfdchar_t *outPath;
			nfdnfilteritem_t* filterList = new nfdnfilteritem_t[filters.size()];
			nfdfiltersize_t filterCount = filters.size();
			for (int i = 0; i < filterCount; i++) {
				filterList[i] = {filters[i].name, filters[i].spec};
			}
			nfdresult_t result = NFD_OpenDialog(&outPath, filterList, filterCount, NULL);
			if (result == NFD_OKAY) {
				fn(outPath);
				NFD_FreePath(outPath);
			} else {
				fn("");
			}
		}

		void Destroy() {
			NFD_Quit();
		}
	}
#else
		std::vector<std::function<void()>> m_DispatchThreadQueue;
		std::mutex m_DispatchThreadQueueMutex;
		bool s_DispatcherRunning = true;
		std::thread s_DispatcherThread{
				[]() {
					while (s_DispatcherRunning) {
						std::scoped_lock<std::mutex> lock(m_DispatchThreadQueueMutex);

						for (auto& func : m_DispatchThreadQueue)
							func();

						m_DispatchThreadQueue.clear();
					}
				}
		};

		void SubmitToDispatcher(const std::function<void()>& function) {
			std::scoped_lock<std::mutex> lock(m_DispatchThreadQueueMutex);

			m_DispatchThreadQueue.emplace_back(function);
		}

		void Init() {
			NFD_Init();
		}


		void OpenFileDialogInternal(FileDialogCallbackFn fn) {
			nfdchar_t *outPath;
			nfdfilteritem_t filterItem[2] = { { "Source code", "c,cpp,cc" }, { "Headers", "h,hpp" } };
			nfdresult_t result = NFD_OpenDialog(&outPath, filterItem, 2, NULL);
			if (result == NFD_OKAY) {
				fn(outPath);
				NFD_FreePath(outPath);
			} else {
				fn("");
			}
		}

		void OpenFileDialog(FileDialogCallbackFn fn) {
			SubmitToDispatcher([fn]() {
				OpenFileDialogInternal(fn);
			});
		}

		void Destroy() {
			s_DispatcherRunning = false;
			s_DispatcherThread.join();
			NFD_Quit();
		}
	}
#endif
}