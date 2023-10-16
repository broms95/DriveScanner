#include "Logger.h"

#include <mutex>
#include <iostream>

std::mutex coutMutex;

void LogString(const std::string& val)
{
	std::lock_guard guard(coutMutex);
	std::cout << val << std::endl;
}