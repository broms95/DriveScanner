#include "FileProcessor.h"

#include <string.h>
#include <iostream>
#include <mutex>
#include <fstream>
#include <sstream>

#include "Logger.h"


static constexpr int BUFFER_SIZE = 4096;
static constexpr int OVERLAP_SIZE = 32;



std::string initializeLookupTable()
{
	std::string table;
	table.resize(256);
	for (int i = 0; i < table.size(); i++)
	{
		table[i] = '.';
	}

	table['0'] = '0';
	table['1'] = '0';
	table['2'] = '0';
	table['3'] = '0';
	table['4'] = '0';
	table['5'] = '0';
	table['6'] = '0';
	table['7'] = '0';
	table['8'] = '0';
	table['9'] = '0';
	table['-'] = '-';
	//	lookupTable[' '] = ' ';

	return table;
}

std::string FileProcessor::lookupTable = initializeLookupTable();


FileProcessor::FileProcessor() :
	workingBuffer(BUFFER_SIZE + OVERLAP_SIZE + 1),
    ifBuffer(BUFFER_SIZE + OVERLAP_SIZE + 1)
{
}

void FileProcessor::processWorkItem(const std::string & path)
{
	memset(workingBuffer.data(), ' ', workingBuffer.size());
	memset(ifBuffer.data(), ' ', workingBuffer.size());

	std::vector<char> buffer(BUFFER_SIZE);

	// try to open the file
	std::fstream fileStream(path);
	if (fileStream)
	{
		bool eof = false;
		while (!eof)
		{
			int charsRead = BUFFER_SIZE;
			fileStream.read(buffer.data(), buffer.size());
			if (!fileStream)
			{
				charsRead = (int)fileStream.gcount();
				eof = true;
			}

			// now process
			processWorkItemBuffer(path, buffer.data(), charsRead);
		}
	}
}

void FileProcessor::processWorkItemBuffer(const std::string& path, char* buffer, int bufferSize)
{
	// shift the end to the begining for both buffers
	auto workingPtr = workingBuffer.data();
	memmove(workingPtr, workingPtr + BUFFER_SIZE, OVERLAP_SIZE);
	memcpy(workingPtr + OVERLAP_SIZE, buffer, bufferSize);
	workingPtr[bufferSize + OVERLAP_SIZE] = 0;

	// shift the intermediate form data over
	auto ifPtr = ifBuffer.data();
	memmove(ifPtr, workingPtr + BUFFER_SIZE, OVERLAP_SIZE);

	// now convert the rest starting after BUFFER_SIZE
	auto dst = ifPtr + OVERLAP_SIZE;
	auto src = buffer;
	auto srcEnd = buffer + bufferSize;
	auto lookupPtr = lookupTable.data();
	while (src != srcEnd) *dst++ = lookupPtr[*src++];
	*dst = 0;

	// we are now ready to search the working buffer
	static std::vector<std::string> patterns{
		//		".000000000.",		      // compact SSN		-- this is very common with raw data
				".000-00-0000.",	      // expanded SSN
				//		".0000000000000000.",	  // compact CC
				//		".0000.0000.0000.0000.",   // expanded CC
	};

	for (auto& pattern : patterns)
	{
		auto resultsPtr = strstr(ifPtr, pattern.c_str());
		while (resultsPtr != nullptr)
		{
			int position = resultsPtr - ifPtr;
			int len = pattern.size();

			//			std::cout << "Found leaked info at position " << position << " length " << len << std::endl;

						// extract the actual string, not the pattern
						// then report it
			std::string actualString(workingPtr + position + 1, len - 2);
			reportNewData(path, actualString);

			// look again, there might be another of this pattern
			// for the buffer
			resultsPtr = strstr(resultsPtr + 1, pattern.c_str());
		}
	}
}

void FileProcessor::reportNewData(const std::string& path, const std::string& data)
{
	std::stringstream ss;
	ss << path << ": " << data;
	LogString(ss.str());
}
