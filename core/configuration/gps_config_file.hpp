#ifndef __GPS_CONFIG_FILE_HPP_
#define __GPS_CONFIG_FILE_HPP_

#include "filesystem.hpp"

class GPSConfigFile {
private:
	FileSystem *m_filesystem;

public:
	GPSConfigFile(FileSystem *fs);
};

#endif // __GPS_CONFIG_FILE_HPP_
