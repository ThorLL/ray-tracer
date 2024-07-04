#pragma once
#include <vector>

#include "glad/glad.h"


class SSBO {
public:
	explicit SSBO(int index) : index(index) {
		glGenBuffers(1, &handle);
	}

	~SSBO() {
		glDeleteBuffers(1, &handle);
	}

	void BufferData(const std::vector<std::shared_ptr<ShaderStruct>>& data) const {
		if (data.empty()) return;
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
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, handle);
		glBufferData(GL_SHADER_STORAGE_BUFFER, static_cast<GLsizeiptr>(_data.size()), _data.data(), GL_STATIC_READ);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, index, handle);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}

private:
	GLuint handle = -1;
	int index;
};

