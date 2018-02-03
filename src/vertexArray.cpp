#include "vertexArray.h"
#include "gl_cpp.hpp"
#include "vertex.h"


VertexArray::VertexArray() {
	gl::CreateVertexArrays(1, &mName);
}

VertexArray::VertexArray(const VertexBuffer& vbo) {
	gl::CreateVertexArrays(1, &mName);
	setBuffer(vbo);
}

VertexArray::VertexArray(const VertexBuffer& vbo, const IndexBuffer& ibo) {
	gl::CreateVertexArrays(1, &mName);
	setBuffer(vbo);
	setIndexBuffer(ibo);
}

VertexArray::~VertexArray() {
	gl::DeleteVertexArrays(1, &mName);
}

void VertexArray::bind() const {
	gl::BindVertexArray(mName);
}

void VertexArray::unbind() const {
	gl::BindVertexArray(0);
}

void VertexArray::addAttribute(int size, unsigned type, unsigned offset, bool normalize) {
	gl::VertexArrayAttribBinding(mName, mNextAttribute, 0);
	gl::VertexArrayAttribFormat(mName, mNextAttribute, size, type, (normalize ? gl::TRUE_ : gl::FALSE_), offset);
	gl::EnableVertexArrayAttrib(mName, mNextAttribute);
	++mNextAttribute;
}

void VertexArray::removeLastAttribute() {
	--mNextAttribute;
	gl::DisableVertexArrayAttrib(mName, mNextAttribute);
}

void VertexArray::setBuffer(const VertexBuffer& vbo) {
	gl::VertexArrayVertexBuffer(mName, 0, vbo.name(), 0, sizeof(Vertex));
}

void VertexArray::setIndexBuffer(const IndexBuffer& ibo) {
	gl::VertexArrayElementBuffer(mName, ibo.name());
}