#pragma once

#include <cheryl/export.h>
#include "gl-internal.h"

// Tests replace the owning module's function slots, never another GLAD implementation.
namespace CE::Rendering::OpenGL::Detail::Test {
    [[nodiscard]] CE_OPENGL_API decltype(glad_glActiveTexture)& glActiveTexture_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glAttachShader)& glAttachShader_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glBindBuffer)& glBindBuffer_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glBindSampler)& glBindSampler_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glBindTexture)& glBindTexture_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glBindVertexArray)& glBindVertexArray_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glBlendEquationSeparate)& glBlendEquationSeparate_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glBlendFunc)& glBlendFunc_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glBlendFuncSeparate)& glBlendFuncSeparate_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glBufferData)& glBufferData_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glClearColor)& glClearColor_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glCompileShader)& glCompileShader_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glCreateProgram)& glCreateProgram_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glCreateShader)& glCreateShader_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glCullFace)& glCullFace_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glDebugMessageCallback)& glDebugMessageCallback_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glDeleteBuffers)& glDeleteBuffers_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glDeleteProgram)& glDeleteProgram_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glDeleteSamplers)& glDeleteSamplers_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glDeleteShader)& glDeleteShader_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glDeleteTextures)& glDeleteTextures_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glDeleteVertexArrays)& glDeleteVertexArrays_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glDepthFunc)& glDepthFunc_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glDepthMask)& glDepthMask_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glDetachShader)& glDetachShader_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glDisable)& glDisable_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glDrawArrays)& glDrawArrays_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glEnable)& glEnable_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glEnableVertexAttribArray)& glEnableVertexAttribArray_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glFrontFace)& glFrontFace_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glGenBuffers)& glGenBuffers_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glGenSamplers)& glGenSamplers_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glGenTextures)& glGenTextures_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glGenVertexArrays)& glGenVertexArrays_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glGenerateMipmap)& glGenerateMipmap_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glGetActiveAttrib)& glGetActiveAttrib_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glGetActiveUniform)& glGetActiveUniform_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glGetAttribLocation)& glGetAttribLocation_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glGetError)& glGetError_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glGetFloatv)& glGetFloatv_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glGetIntegeri_v)& glGetIntegeri_v_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glGetIntegerv)& glGetIntegerv_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glGetPointerv)& glGetPointerv_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glGetProgramiv)& glGetProgramiv_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glGetSamplerParameterfv)& glGetSamplerParameterfv_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glGetShaderiv)& glGetShaderiv_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glGetString)& glGetString_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glGetTexImage)& glGetTexImage_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glGetUniformLocation)& glGetUniformLocation_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glIsBuffer)& glIsBuffer_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glIsEnabled)& glIsEnabled_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glIsProgram)& glIsProgram_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glIsSampler)& glIsSampler_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glIsShader)& glIsShader_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glIsTexture)& glIsTexture_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glIsVertexArray)& glIsVertexArray_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glLinkProgram)& glLinkProgram_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glPixelStorei)& glPixelStorei_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glReadBuffer)& glReadBuffer_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glReadPixels)& glReadPixels_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glSamplerParameterf)& glSamplerParameterf_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glSamplerParameteri)& glSamplerParameteri_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glScissor)& glScissor_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glShaderSource)& glShaderSource_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glTexImage2D)& glTexImage2D_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glTexParameterf)& glTexParameterf_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glTexParameteri)& glTexParameteri_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glTexParameteriv)& glTexParameteriv_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glUniform1f)& glUniform1f_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glUniform1i)& glUniform1i_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glUniform1ui)& glUniform1ui_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glUniform2f)& glUniform2f_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glUniform3f)& glUniform3f_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glUniform4f)& glUniform4f_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glUniformMatrix2fv)& glUniformMatrix2fv_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glUniformMatrix3fv)& glUniformMatrix3fv_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glUniformMatrix4fv)& glUniformMatrix4fv_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glUseProgram)& glUseProgram_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API decltype(glad_glVertexAttribPointer)& glVertexAttribPointer_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API int& GLAD_GL_ARB_texture_filter_anisotropic_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API int& GLAD_GL_EXT_texture_filter_anisotropic_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API int& GLAD_GL_KHR_debug_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API int& GLAD_GL_VERSION_4_3_slot() noexcept;
    [[nodiscard]] CE_OPENGL_API int& GLAD_GL_VERSION_4_6_slot() noexcept;
}

#ifndef CHERYL_OPENGL_TEST_IMPLEMENTATION
#define glad_glActiveTexture (::CE::Rendering::OpenGL::Detail::Test::glActiveTexture_slot())
#define glad_glAttachShader (::CE::Rendering::OpenGL::Detail::Test::glAttachShader_slot())
#define glad_glBindBuffer (::CE::Rendering::OpenGL::Detail::Test::glBindBuffer_slot())
#define glad_glBindSampler (::CE::Rendering::OpenGL::Detail::Test::glBindSampler_slot())
#define glad_glBindTexture (::CE::Rendering::OpenGL::Detail::Test::glBindTexture_slot())
#define glad_glBindVertexArray (::CE::Rendering::OpenGL::Detail::Test::glBindVertexArray_slot())
#define glad_glBlendEquationSeparate (::CE::Rendering::OpenGL::Detail::Test::glBlendEquationSeparate_slot())
#define glad_glBlendFunc (::CE::Rendering::OpenGL::Detail::Test::glBlendFunc_slot())
#define glad_glBlendFuncSeparate (::CE::Rendering::OpenGL::Detail::Test::glBlendFuncSeparate_slot())
#define glad_glBufferData (::CE::Rendering::OpenGL::Detail::Test::glBufferData_slot())
#define glad_glClearColor (::CE::Rendering::OpenGL::Detail::Test::glClearColor_slot())
#define glad_glCompileShader (::CE::Rendering::OpenGL::Detail::Test::glCompileShader_slot())
#define glad_glCreateProgram (::CE::Rendering::OpenGL::Detail::Test::glCreateProgram_slot())
#define glad_glCreateShader (::CE::Rendering::OpenGL::Detail::Test::glCreateShader_slot())
#define glad_glCullFace (::CE::Rendering::OpenGL::Detail::Test::glCullFace_slot())
#define glad_glDebugMessageCallback (::CE::Rendering::OpenGL::Detail::Test::glDebugMessageCallback_slot())
#define glad_glDeleteBuffers (::CE::Rendering::OpenGL::Detail::Test::glDeleteBuffers_slot())
#define glad_glDeleteProgram (::CE::Rendering::OpenGL::Detail::Test::glDeleteProgram_slot())
#define glad_glDeleteSamplers (::CE::Rendering::OpenGL::Detail::Test::glDeleteSamplers_slot())
#define glad_glDeleteShader (::CE::Rendering::OpenGL::Detail::Test::glDeleteShader_slot())
#define glad_glDeleteTextures (::CE::Rendering::OpenGL::Detail::Test::glDeleteTextures_slot())
#define glad_glDeleteVertexArrays (::CE::Rendering::OpenGL::Detail::Test::glDeleteVertexArrays_slot())
#define glad_glDepthFunc (::CE::Rendering::OpenGL::Detail::Test::glDepthFunc_slot())
#define glad_glDepthMask (::CE::Rendering::OpenGL::Detail::Test::glDepthMask_slot())
#define glad_glDetachShader (::CE::Rendering::OpenGL::Detail::Test::glDetachShader_slot())
#define glad_glDisable (::CE::Rendering::OpenGL::Detail::Test::glDisable_slot())
#define glad_glDrawArrays (::CE::Rendering::OpenGL::Detail::Test::glDrawArrays_slot())
#define glad_glEnable (::CE::Rendering::OpenGL::Detail::Test::glEnable_slot())
#define glad_glEnableVertexAttribArray (::CE::Rendering::OpenGL::Detail::Test::glEnableVertexAttribArray_slot())
#define glad_glFrontFace (::CE::Rendering::OpenGL::Detail::Test::glFrontFace_slot())
#define glad_glGenBuffers (::CE::Rendering::OpenGL::Detail::Test::glGenBuffers_slot())
#define glad_glGenSamplers (::CE::Rendering::OpenGL::Detail::Test::glGenSamplers_slot())
#define glad_glGenTextures (::CE::Rendering::OpenGL::Detail::Test::glGenTextures_slot())
#define glad_glGenVertexArrays (::CE::Rendering::OpenGL::Detail::Test::glGenVertexArrays_slot())
#define glad_glGenerateMipmap (::CE::Rendering::OpenGL::Detail::Test::glGenerateMipmap_slot())
#define glad_glGetActiveAttrib (::CE::Rendering::OpenGL::Detail::Test::glGetActiveAttrib_slot())
#define glad_glGetActiveUniform (::CE::Rendering::OpenGL::Detail::Test::glGetActiveUniform_slot())
#define glad_glGetAttribLocation (::CE::Rendering::OpenGL::Detail::Test::glGetAttribLocation_slot())
#define glad_glGetError (::CE::Rendering::OpenGL::Detail::Test::glGetError_slot())
#define glad_glGetFloatv (::CE::Rendering::OpenGL::Detail::Test::glGetFloatv_slot())
#define glad_glGetIntegeri_v (::CE::Rendering::OpenGL::Detail::Test::glGetIntegeri_v_slot())
#define glad_glGetIntegerv (::CE::Rendering::OpenGL::Detail::Test::glGetIntegerv_slot())
#define glad_glGetPointerv (::CE::Rendering::OpenGL::Detail::Test::glGetPointerv_slot())
#define glad_glGetProgramiv (::CE::Rendering::OpenGL::Detail::Test::glGetProgramiv_slot())
#define glad_glGetSamplerParameterfv (::CE::Rendering::OpenGL::Detail::Test::glGetSamplerParameterfv_slot())
#define glad_glGetShaderiv (::CE::Rendering::OpenGL::Detail::Test::glGetShaderiv_slot())
#define glad_glGetString (::CE::Rendering::OpenGL::Detail::Test::glGetString_slot())
#define glad_glGetTexImage (::CE::Rendering::OpenGL::Detail::Test::glGetTexImage_slot())
#define glad_glGetUniformLocation (::CE::Rendering::OpenGL::Detail::Test::glGetUniformLocation_slot())
#define glad_glIsBuffer (::CE::Rendering::OpenGL::Detail::Test::glIsBuffer_slot())
#define glad_glIsEnabled (::CE::Rendering::OpenGL::Detail::Test::glIsEnabled_slot())
#define glad_glIsProgram (::CE::Rendering::OpenGL::Detail::Test::glIsProgram_slot())
#define glad_glIsSampler (::CE::Rendering::OpenGL::Detail::Test::glIsSampler_slot())
#define glad_glIsShader (::CE::Rendering::OpenGL::Detail::Test::glIsShader_slot())
#define glad_glIsTexture (::CE::Rendering::OpenGL::Detail::Test::glIsTexture_slot())
#define glad_glIsVertexArray (::CE::Rendering::OpenGL::Detail::Test::glIsVertexArray_slot())
#define glad_glLinkProgram (::CE::Rendering::OpenGL::Detail::Test::glLinkProgram_slot())
#define glad_glPixelStorei (::CE::Rendering::OpenGL::Detail::Test::glPixelStorei_slot())
#define glad_glReadBuffer (::CE::Rendering::OpenGL::Detail::Test::glReadBuffer_slot())
#define glad_glReadPixels (::CE::Rendering::OpenGL::Detail::Test::glReadPixels_slot())
#define glad_glSamplerParameterf (::CE::Rendering::OpenGL::Detail::Test::glSamplerParameterf_slot())
#define glad_glSamplerParameteri (::CE::Rendering::OpenGL::Detail::Test::glSamplerParameteri_slot())
#define glad_glScissor (::CE::Rendering::OpenGL::Detail::Test::glScissor_slot())
#define glad_glShaderSource (::CE::Rendering::OpenGL::Detail::Test::glShaderSource_slot())
#define glad_glTexImage2D (::CE::Rendering::OpenGL::Detail::Test::glTexImage2D_slot())
#define glad_glTexParameterf (::CE::Rendering::OpenGL::Detail::Test::glTexParameterf_slot())
#define glad_glTexParameteri (::CE::Rendering::OpenGL::Detail::Test::glTexParameteri_slot())
#define glad_glTexParameteriv (::CE::Rendering::OpenGL::Detail::Test::glTexParameteriv_slot())
#define glad_glUniform1f (::CE::Rendering::OpenGL::Detail::Test::glUniform1f_slot())
#define glad_glUniform1i (::CE::Rendering::OpenGL::Detail::Test::glUniform1i_slot())
#define glad_glUniform1ui (::CE::Rendering::OpenGL::Detail::Test::glUniform1ui_slot())
#define glad_glUniform2f (::CE::Rendering::OpenGL::Detail::Test::glUniform2f_slot())
#define glad_glUniform3f (::CE::Rendering::OpenGL::Detail::Test::glUniform3f_slot())
#define glad_glUniform4f (::CE::Rendering::OpenGL::Detail::Test::glUniform4f_slot())
#define glad_glUniformMatrix2fv (::CE::Rendering::OpenGL::Detail::Test::glUniformMatrix2fv_slot())
#define glad_glUniformMatrix3fv (::CE::Rendering::OpenGL::Detail::Test::glUniformMatrix3fv_slot())
#define glad_glUniformMatrix4fv (::CE::Rendering::OpenGL::Detail::Test::glUniformMatrix4fv_slot())
#define glad_glUseProgram (::CE::Rendering::OpenGL::Detail::Test::glUseProgram_slot())
#define glad_glVertexAttribPointer (::CE::Rendering::OpenGL::Detail::Test::glVertexAttribPointer_slot())
#define GLAD_GL_ARB_texture_filter_anisotropic (::CE::Rendering::OpenGL::Detail::Test::GLAD_GL_ARB_texture_filter_anisotropic_slot())
#define GLAD_GL_EXT_texture_filter_anisotropic (::CE::Rendering::OpenGL::Detail::Test::GLAD_GL_EXT_texture_filter_anisotropic_slot())
#define GLAD_GL_KHR_debug (::CE::Rendering::OpenGL::Detail::Test::GLAD_GL_KHR_debug_slot())
#define GLAD_GL_VERSION_4_3 (::CE::Rendering::OpenGL::Detail::Test::GLAD_GL_VERSION_4_3_slot())
#define GLAD_GL_VERSION_4_6 (::CE::Rendering::OpenGL::Detail::Test::GLAD_GL_VERSION_4_6_slot())
#endif
