#pragma once
#include <memory>

// Generic abstract class that can be overriden to fill a constant buffer with any size/type of data
class ConstantBufferData {
public:
	virtual UINT byteSize() const = 0;
	virtual std::unique_ptr<ConstantBufferData> clone() const = 0;
	virtual void* getData() = 0;
	virtual ~ConstantBufferData() = default;
};