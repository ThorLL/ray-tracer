#pragma once
#include <vector>

#include "BufferObject.h"
#include "glad/glad.h"

template <typename T, typename = std::enable_if_t<std::is_base_of_v<ShaderStruct, T> && !std::is_same_v<ShaderStruct, T>>>
class SSBO final : public BufferObject<GL_SHADER_STORAGE_BUFFER, std::byte>{
public:
	explicit SSBO(const int index) : index(index) {}
	~SSBO() override = default;

	void BufferData(const std::vector<std::shared_ptr<T>>& data, GLuint usage) const override {
		BufferObject::BufferData(ShaderStructToBytes(data), usage);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, this->handle);
	}

	void BufferData(GLsizeiptr size, GLuint usage) const override {
		BufferObject::BufferData(ShaderStruct::GetSize<T>(), usage);
	};
	void UpdateData(const std::vector<T>& data, size_t offset) const override {
		BufferObject::UpdateData(ShaderStructToBytes(data), offset);
	};
private:
	int index;

	std::vector<std::byte> ShaderStructToBytes(const std::vector<std::shared_ptr<T>>& data) {
		std::vector<std::vector<std::byte>> bytes;
		size_t totalSize = 0;
		for (const auto &s : data) {
			auto structBytes = s.get()->GetBytes();
			bytes.emplace_back(structBytes);
			totalSize += structBytes.size();
		}
		std::vector<std::byte> _data(totalSize);
		std::byte* ptr = _data.data();
		for (const auto& structBytes : bytes) {
			std::memcpy(ptr, structBytes.data(), structBytes.size());
			ptr += structBytes.size();
		}
		return _data;
	}
};

