#pragma once
#include <vector>

template<typename D>
class Object {
public:
	Object() = default;
	virtual ~Object() = default;

	virtual void Bind() const = 0;
	virtual void UnBind() const = 0;


	virtual void BufferData(GLsizeiptr size, GLuint usage) const = 0;
	virtual void BufferData(const std::vector<D>& data, GLuint usage) const = 0;
	virtual void UpdateData(const std::vector<D>& data, size_t offset) const = 0;

protected:
	GLuint handle = -1;
	bool isBound = false;
};
