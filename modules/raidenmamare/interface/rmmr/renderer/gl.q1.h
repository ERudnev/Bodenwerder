#pragma once

#include <GL/glew.h>
namespace rmmr::renderer {

    using IntPtr = GLintptr;
    using SizePtr = GLsizeiptr;
    using Count = GLsizei;
    using Integer32 = GLuint;
    using Signed32 = GLint;

    using VertexArray = GLuint;
    using VertexBuffer = GLuint;
    using ElementBuffer = GLuint;
    using StorageBuffer = GLuint;
    using UniformBuffer = GLuint;
    using IndirectBuffer = GLuint;
    using Texture = GLuint;
    using Program = GLuint;
    using Framebuffer = GLuint;

}
