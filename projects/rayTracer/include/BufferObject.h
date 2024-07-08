#pragma once
#include <imgui_impl_opengl3_loader.h>
#include <vector>

#include "Object.h"

template<GLuint T, typename D>
class BufferObject : public Object<D>{
public:
	BufferObject() {glGenBuffers(1, &this->handle);}

	~BufferObject() override {glDeleteBuffers(1, &this->handle);}

	void Bind() const override {glBindBuffer(T, this->handle); this->isBound = true;}
	void UnBind() const override {glBindBuffer(T, 0); this->isBound = false;}

	void BufferData(GLsizeiptr size, GLuint usage) const override {
		assert(this->isBound);
		glBufferData(T, size, nullptr, usage);
	};
	void BufferData(const std::vector<D>& data, GLuint usage) const override {
		assert(this->isBound);
		glBufferData(T, sizeof(D) * data.size(), data.data(), usage);
	}
	void UpdateData(const std::vector<D>& data, size_t offset) const override {
		assert(this->isBound);
		glBufferSubData(T, offset, sizeof(D) * data.size(), data.data());
	};
};
