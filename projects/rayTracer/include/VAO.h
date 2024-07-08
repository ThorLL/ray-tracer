#pragma once
#include "glad/glad.h"

struct VertexAttribute {
	GLuint index;
	GLint size;
	GLenum type;
	GLboolean normalized;
	GLsizei stride;
	const void * pointer;
};

class VertexArrayObject {

};
