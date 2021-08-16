#ifndef __MOCK_BLE_HPP_
#define __MOCK_BLE_HPP_

#include "ble_service.hpp"

// Compares two "const std::function<int(BLEServiceEvent&>" for equality
class MockStdFunctionBLEServiceEventComparator : public MockNamedValueComparator
{
public:
    virtual bool isEqual(const void* object1, const void* object2)
	{
		typedef void(functionType)();

		auto object1_func_ptr = reinterpret_cast< const std::function<int(BLEServiceEvent&)>* >(object1);
		auto object2_func_ptr = reinterpret_cast< const std::function<int(BLEServiceEvent&)>* >(object2);

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
public:
	void start(std::function<int(BLEServiceEvent&)> on_event) {
		// Install a comparator for checking the equality of std::functions
		mock().installComparator("std::function<int(BLEServiceEvent&)>", m_comparator);
		mock().actualCall("start").onObject(this).withParameterOfType("std::function<int(BLEServiceEvent&)>", "on_event", &on_event);
		m_on_event = on_event;
	}
	void stop() {
		mock().actualCall("stop").onObject(this);
	}
	bool write(std::string str) {
		return mock().actualCall("write").onObject(this).withParameterOfType("std::string", "str", &str).returnBoolValue();
	}
	std::string read_line() {
		return *static_cast<const std::string*>(mock().actualCall("read_line").onObject(this).returnConstPointerValue());
	}
	void set_device_name(const std::string& name) {
		mock().actualCall("set_device_name").onObject(this).withParameter("name", name.c_str());
	}

	int invoke_event(BLEServiceEvent& event) {
		return m_on_event(event);
	}

private:
	MockStdFunctionBLEServiceEventComparator m_comparator;
	std::function<int(BLEServiceEvent&)> m_on_event;

};

#endif // __MOCK_BLE_HPP_
