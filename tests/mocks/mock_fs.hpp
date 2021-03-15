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
	bool is_mounted() {
		return mock().actualCall("is_mounted").onObject(this).returnBoolValue();
	}
	int format() {
		return mock().actualCall("format").onObject(this).returnIntValue();
	}
	int remove(const char *path) {
		return mock().actualCall("remove").onObject(this).withParameter("path", path).returnIntValue();
	}
	int set_attr(const char *path, unsigned int &attr) {
		return mock().actualCall("set_attr").onObject(this).withParameter("path", path).withParameter("attr", attr).returnIntValue();
	}
	int get_attr(const char *path, unsigned int &attr) {
		return mock().actualCall("get_attr").onObject(this).withParameter("path", path).withParameter("attr", attr).returnIntValue();
	}
	void *get_private_data() {
		return nullptr;
	}
};

#endif // __MOCK_FS_HPP_
