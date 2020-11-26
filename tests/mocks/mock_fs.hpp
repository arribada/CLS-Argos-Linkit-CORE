#ifndef __MOCK_FS_HPP_
#define __MOCK_FS_HPP_

#include "filesystem.hpp"

class MockFileSystem : public FileSystem {
public:
	int mount() {
		return mock().actualCall("mount").onObject(this).returnIntValue();
	}
	int umount() {
		return mock().actualCall("umount").onObject(this).returnIntValue();
	}
	int format() {
		return mock().actualCall("format").onObject(this).returnIntValue();
	}
};

#endif // __MOCK_FS_HPP_
