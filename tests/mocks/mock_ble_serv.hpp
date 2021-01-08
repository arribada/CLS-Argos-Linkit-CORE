#ifndef __MOCK_BLE_HPP_
#define __MOCK_BLE_HPP_

#include "ble_service.hpp"

// Compares two "const std::function<void()>" for equality
class MockStdFunctionVoidComparator : public MockNamedValueComparator
{
public:
    virtual bool isEqual(const void* object1, const void* object2)
	{
		typedef void(functionType)();

		auto object1_func_ptr = reinterpret_cast< const std::function<void()>* >(object1);
		auto object2_func_ptr = reinterpret_cast< const std::function<void()>* >(object2);

		// Check function target is of the same type
		if (object1_func_ptr->target_type() != object2_func_ptr->target_type())
			return false;
		
		auto object1_func_target = object1_func_ptr->target<functionType*>();
		auto object2_func_target = object1_func_ptr->target<functionType*>();

		if (object1_func_target != object2_func_target)
			return false;

		return true;
	}

    virtual SimpleString valueToString(const void* object)
	{
		(void) object;
		// The valueToString is called when an error message is printed and it needs to print the actual and expected values
		// It is unclear how this should be implemented
		return "Unknown";
	}
};

class MockBLEService : public BLEService {
	void start(const std::function<void()> &on_connected, const std::function<void()> &on_disconnected, const std::function<void()> &on_received) {

		// Install a comparator for checking the equality of std::functions
		mock().installComparator("std::function<void()>", m_comparator);

		mock().actualCall("start").onObject(this).withParameterOfType("std::function<void()>", "on_connected", &on_connected).withParameterOfType("std::function<void()>", "on_disconnected", &on_disconnected).withParameterOfType("std::function<void()>", "on_received", &on_received);
	}
	void stop() {
		mock().actualCall("stop").onObject(this);
	}
	void write(std::string str) {
		mock().actualCall("write").onObject(this).withParameterOfType("std::string", "str", &str);
	}
	std::string read_line() {
		return *static_cast<const std::string*>(mock().actualCall("read_line").onObject(this).returnConstPointerValue());
	}

private:
	MockStdFunctionVoidComparator m_comparator;
};

#endif // __MOCK_BLE_HPP_
