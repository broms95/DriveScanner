// DriveScanner.cpp : Defines the entry point for the application.
//

#include "DriveScanner.h"

#include <string>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <filesystem>
#include <memory>
#include <sstream>
#include <chrono>

#include "FileProcessor.h"
#include "Logger.h"

using namespace std;

namespace fs = std::filesystem;

// List of work items and a running tally to let us
// report our progress.  Use the work mutex to protect
// access from multiple threads.
std::mutex workMutex;
std::queue<std::string> workItems;
int workItemsProcessed = 0;

int dirsScanned = 0;
int filesScanned = 0;
int64_t bytesToProcess = 0;

void generateWorkItems(const std::string& startingPath)
{
	// OK, in one method, let's fill the entire work item list
	// of paths that we will use for phase 2.  Use a stack to handle
	// traversing the tree of directories.
	for (auto const& dir_entry : fs::recursive_directory_iterator(startingPath))
	{
		if (dir_entry.is_directory())
		{
//			std::cout << "DIR  " << dir_entry << '\n';
			dirsScanned++;
		}
		else if (dir_entry.is_regular_file())
		{
//			std::cout << "FILE " << dir_entry << '\n';
			workItems.push(dir_entry.path().string());
			bytesToProcess += dir_entry.file_size();
			filesScanned++;
		}

		if (filesScanned == 100000)
		{
			break;
		}
	}
}

void processThread()
{
	FileProcessor processor;
	std::string item;
	int processCount = 0;

	// PHASE 2.  Simple way, one after another.
	for (;;)
	{
		// Grab the item and bump the count.
		
		{
			std::lock_guard guard(workMutex);
			if (workItems.empty()) {
				// no more work items, exit thread.
				return;
			}
			item = workItems.front();
			workItems.pop();
			processCount = ++workItemsProcessed;
		}

		processor.processWorkItem(item);

		if (processCount % 10000 == 0)
		{
			std::stringstream ss;
			ss << "Processed " << processCount;
			LogString(ss.str());
		}
	}
}

int main(int argc, char *argv[])
{
	int threadsToUse = 10;
	std::string startingPath = "C:\\dev";

	auto timeStart = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

	// PHASE 1.  Make work items.
	generateWorkItems(startingPath);

	auto timePhase1 = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

	// PHASE 2.  Process work items using thread pool
	std::vector<std::shared_ptr<std::thread>> threads;
	for (int i = 0; i < threadsToUse; i++)
	{
		std::shared_ptr<std::thread> t = std::make_shared<std::thread>(processThread);
		threads.push_back(t);
	}

	// each thread will exit when it has no work to do.  Just join
	// each thread and we are done.
	for (auto& thread : threads)
	{
		thread->join();
	}

	auto timePhase2 = duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

	double gb = bytesToProcess / double(1024 * 1024 * 1024);

	std::cout << "Threads:                " << threadsToUse << std::endl;

	std::cout << "Directories scanned:    " << dirsScanned << std::endl;
	std::cout << "Files scanned:          " << filesScanned << std::endl;
	std::cout << "Bytes scanned           " << gb << " GB" << std::endl;

	// report the timings
	double durationPhase1 = (timePhase1 - timeStart) / 1000.0;
	std::cout << "Time for phase 1:       " << durationPhase1 << " sec" << std::endl;

	double durationPhase2 = (timePhase2 - timePhase1) / 1000.0;
	std::cout << "Time for phase 2:       " << durationPhase2 << " sec" << std::endl;

	double totalTime = (timePhase2 - timeStart) / 1000.0;
	std::cout << "Total scanning time:    " << totalTime << " sec" << std::endl;

	double speed = gb / totalTime;
	double speedPerThread = speed / threadsToUse;
	std::cout << "Bandwidth (per thread): " << speedPerThread << " GB/sec" << std::endl;
	std::cout << "Bandwidth (aggregate):  " << speed << " GB/sec" << std::endl;



	return 0;
}
