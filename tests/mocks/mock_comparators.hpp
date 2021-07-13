#pragma once

#include "CppUTestExt/MockSupportPlugin.h"
#include <functional>
#include "gps_scheduler.hpp"

class MockGPSNavSettingsComparator : public MockNamedValueComparator
{
public:
    bool isEqual(const void* object1, const void* object2) override
	{
        // Casting here the void pointers to the type to compare
        const auto *pointObject1 = (const GPSNavSettings *) object1;
        const auto *pointObject2 = (const GPSNavSettings *) object2;
    	return pointObject1->fix_mode == pointObject2->fix_mode &&
    			pointObject1->dyn_model == pointObject2->dyn_model;
	}

    virtual SimpleString valueToString(const void*)
    {
		// The valueToString is called when an error message is printed and it needs to print the actual and expected values
		// It is unclear how this should be implemented
		return "Unknown";
    }
};

// Compares two "const std::function<void()>" for equality
class MockStdFunctionVoidComparator : public MockNamedValueComparator
{
public:
    bool isEqual(const void* object1, const void* object2) override
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

    virtual SimpleString valueToString(const void*)
	{
		// The valueToString is called when an error message is printed and it needs to print the actual and expected values
		// It is unclear how this should be implemented
		return "Unknown";
	}
};

// Compares two "const std::function<void(ServiceEvent&)>" for equality
class MockStdFunctionServiceEventComparator : public MockNamedValueComparator
{
public:
    virtual bool isEqual(const void* object1, const void* object2)
	{
		typedef void(functionType)();

		auto object1_func_ptr = reinterpret_cast< const std::function<void(ServiceEvent&)>* >(object1);
		auto object2_func_ptr = reinterpret_cast< const std::function<void(ServiceEvent&)>* >(object2);

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
