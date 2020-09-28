#pragma once
#include <memory>

class ConstantBufferData {
public:
	virtual UINT byteSize() const = 0;
	virtual std::unique_ptr<ConstantBufferData> clone() const = 0;
	virtual void* getData() = 0;
	virtual ~ConstantBufferData() = default;
};