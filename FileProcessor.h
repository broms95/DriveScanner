
#include <vector>
#include <string>

class FileProcessor
{
public:
	FileProcessor();
	void processWorkItem(const std::string& path);


protected:
	void processWorkItemBuffer(const std::string& path, char* buffer, int bufferSize);
	void reportNewData(const std::string& path, const std::string& data);

	std::vector<char> workingBuffer;
	std::vector<char> ifBuffer;

	// table to convert regular characters to intermediate form
	static std::string lookupTable;
};