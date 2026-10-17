#define CHERYL_OPENGL_TEST_IMPLEMENTATION
#include "backends/opengl/gl-test.h"

namespace CE::Rendering::OpenGL::Detail::Test {
    decltype(glad_glActiveTexture)& glActiveTexture_slot() noexcept {
        return glad_glActiveTexture;
    }
    decltype(glad_glAttachShader)& glAttachShader_slot() noexcept {
        return glad_glAttachShader;
    }
    decltype(glad_glBindBuffer)& glBindBuffer_slot() noexcept {
        return glad_glBindBuffer;
    }
    decltype(glad_glBindSampler)& glBindSampler_slot() noexcept {
        return glad_glBindSampler;
    }
    decltype(glad_glBindTexture)& glBindTexture_slot() noexcept {
        return glad_glBindTexture;
    }
    decltype(glad_glBindVertexArray)& glBindVertexArray_slot() noexcept {
        return glad_glBindVertexArray;
    }
    decltype(glad_glBlendEquationSeparate)& glBlendEquationSeparate_slot() noexcept {
        return glad_glBlendEquationSeparate;
    }
    decltype(glad_glBlendFunc)& glBlendFunc_slot() noexcept {
        return glad_glBlendFunc;
    }
    decltype(glad_glBlendFuncSeparate)& glBlendFuncSeparate_slot() noexcept {
        return glad_glBlendFuncSeparate;
    }
    decltype(glad_glBufferData)& glBufferData_slot() noexcept {
        return glad_glBufferData;
    }
    decltype(glad_glClearColor)& glClearColor_slot() noexcept {
        return glad_glClearColor;
    }
    decltype(glad_glCompileShader)& glCompileShader_slot() noexcept {
        return glad_glCompileShader;
    }
    decltype(glad_glCreateProgram)& glCreateProgram_slot() noexcept {
        return glad_glCreateProgram;
    }
    decltype(glad_glCreateShader)& glCreateShader_slot() noexcept {
        return glad_glCreateShader;
    }
    decltype(glad_glCullFace)& glCullFace_slot() noexcept {
        return glad_glCullFace;
    }
    decltype(glad_glDebugMessageCallback)& glDebugMessageCallback_slot() noexcept {
        return glad_glDebugMessageCallback;
    }
    decltype(glad_glDeleteBuffers)& glDeleteBuffers_slot() noexcept {
        return glad_glDeleteBuffers;
    }
    decltype(glad_glDeleteProgram)& glDeleteProgram_slot() noexcept {
        return glad_glDeleteProgram;
    }
    decltype(glad_glDeleteSamplers)& glDeleteSamplers_slot() noexcept {
        return glad_glDeleteSamplers;
    }
    decltype(glad_glDeleteShader)& glDeleteShader_slot() noexcept {
        return glad_glDeleteShader;
    }
    decltype(glad_glDeleteTextures)& glDeleteTextures_slot() noexcept {
        return glad_glDeleteTextures;
    }
    decltype(glad_glDeleteVertexArrays)& glDeleteVertexArrays_slot() noexcept {
        return glad_glDeleteVertexArrays;
    }
    decltype(glad_glDepthFunc)& glDepthFunc_slot() noexcept {
        return glad_glDepthFunc;
    }
    decltype(glad_glDepthMask)& glDepthMask_slot() noexcept {
        return glad_glDepthMask;
    }
    decltype(glad_glDetachShader)& glDetachShader_slot() noexcept {
        return glad_glDetachShader;
    }
    decltype(glad_glDisable)& glDisable_slot() noexcept {
        return glad_glDisable;
    }
    decltype(glad_glDrawArrays)& glDrawArrays_slot() noexcept {
        return glad_glDrawArrays;
    }
    decltype(glad_glEnable)& glEnable_slot() noexcept {
        return glad_glEnable;
    }
    decltype(glad_glEnableVertexAttribArray)& glEnableVertexAttribArray_slot() noexcept {
        return glad_glEnableVertexAttribArray;
    }
    decltype(glad_glFrontFace)& glFrontFace_slot() noexcept {
        return glad_glFrontFace;
    }
    decltype(glad_glGenBuffers)& glGenBuffers_slot() noexcept {
        return glad_glGenBuffers;
    }
    decltype(glad_glGenSamplers)& glGenSamplers_slot() noexcept {
        return glad_glGenSamplers;
    }
    decltype(glad_glGenTextures)& glGenTextures_slot() noexcept {
        return glad_glGenTextures;
    }
    decltype(glad_glGenVertexArrays)& glGenVertexArrays_slot() noexcept {
        return glad_glGenVertexArrays;
    }
    decltype(glad_glGenerateMipmap)& glGenerateMipmap_slot() noexcept {
        return glad_glGenerateMipmap;
    }
    decltype(glad_glGetActiveAttrib)& glGetActiveAttrib_slot() noexcept {
        return glad_glGetActiveAttrib;
    }
    decltype(glad_glGetActiveUniform)& glGetActiveUniform_slot() noexcept {
        return glad_glGetActiveUniform;
    }
    decltype(glad_glGetAttribLocation)& glGetAttribLocation_slot() noexcept {
        return glad_glGetAttribLocation;
    }
    decltype(glad_glGetError)& glGetError_slot() noexcept {
        return glad_glGetError;
    }
    decltype(glad_glGetFloatv)& glGetFloatv_slot() noexcept {
        return glad_glGetFloatv;
    }
    decltype(glad_glGetIntegeri_v)& glGetIntegeri_v_slot() noexcept {
        return glad_glGetIntegeri_v;
    }
    decltype(glad_glGetIntegerv)& glGetIntegerv_slot() noexcept {
        return glad_glGetIntegerv;
    }
    decltype(glad_glGetPointerv)& glGetPointerv_slot() noexcept {
        return glad_glGetPointerv;
    }
    decltype(glad_glGetProgramiv)& glGetProgramiv_slot() noexcept {
        return glad_glGetProgramiv;
    }
    decltype(glad_glGetSamplerParameterfv)& glGetSamplerParameterfv_slot() noexcept {
        return glad_glGetSamplerParameterfv;
    }
    decltype(glad_glGetShaderiv)& glGetShaderiv_slot() noexcept {
        return glad_glGetShaderiv;
    }
    decltype(glad_glGetString)& glGetString_slot() noexcept {
        return glad_glGetString;
    }
    decltype(glad_glGetTexImage)& glGetTexImage_slot() noexcept {
        return glad_glGetTexImage;
    }
    decltype(glad_glGetUniformLocation)& glGetUniformLocation_slot() noexcept {
        return glad_glGetUniformLocation;
    }
    decltype(glad_glIsBuffer)& glIsBuffer_slot() noexcept {
        return glad_glIsBuffer;
    }
    decltype(glad_glIsEnabled)& glIsEnabled_slot() noexcept {
        return glad_glIsEnabled;
    }
    decltype(glad_glIsProgram)& glIsProgram_slot() noexcept {
        return glad_glIsProgram;
    }
    decltype(glad_glIsSampler)& glIsSampler_slot() noexcept {
        return glad_glIsSampler;
    }
    decltype(glad_glIsShader)& glIsShader_slot() noexcept {
        return glad_glIsShader;
    }
    decltype(glad_glIsTexture)& glIsTexture_slot() noexcept {
        return glad_glIsTexture;
    }
    decltype(glad_glIsVertexArray)& glIsVertexArray_slot() noexcept {
        return glad_glIsVertexArray;
    }
    decltype(glad_glLinkProgram)& glLinkProgram_slot() noexcept {
        return glad_glLinkProgram;
    }
    decltype(glad_glPixelStorei)& glPixelStorei_slot() noexcept {
        return glad_glPixelStorei;
    }
    decltype(glad_glReadBuffer)& glReadBuffer_slot() noexcept {
        return glad_glReadBuffer;
    }
    decltype(glad_glReadPixels)& glReadPixels_slot() noexcept {
        return glad_glReadPixels;
    }
    decltype(glad_glSamplerParameterf)& glSamplerParameterf_slot() noexcept {
        return glad_glSamplerParameterf;
    }
    decltype(glad_glSamplerParameteri)& glSamplerParameteri_slot() noexcept {
        return glad_glSamplerParameteri;
    }
    decltype(glad_glScissor)& glScissor_slot() noexcept {
        return glad_glScissor;
    }
    decltype(glad_glShaderSource)& glShaderSource_slot() noexcept {
        return glad_glShaderSource;
    }
    decltype(glad_glTexImage2D)& glTexImage2D_slot() noexcept {
        return glad_glTexImage2D;
    }
    decltype(glad_glTexParameterf)& glTexParameterf_slot() noexcept {
        return glad_glTexParameterf;
    }
    decltype(glad_glTexParameteri)& glTexParameteri_slot() noexcept {
        return glad_glTexParameteri;
    }
    decltype(glad_glTexParameteriv)& glTexParameteriv_slot() noexcept {
        return glad_glTexParameteriv;
    }
    decltype(glad_glUniform1f)& glUniform1f_slot() noexcept {
        return glad_glUniform1f;
    }
    decltype(glad_glUniform1i)& glUniform1i_slot() noexcept {
        return glad_glUniform1i;
    }
    decltype(glad_glUniform1ui)& glUniform1ui_slot() noexcept {
        return glad_glUniform1ui;
    }
    decltype(glad_glUniform2f)& glUniform2f_slot() noexcept {
        return glad_glUniform2f;
    }
    decltype(glad_glUniform3f)& glUniform3f_slot() noexcept {
        return glad_glUniform3f;
    }
    decltype(glad_glUniform4f)& glUniform4f_slot() noexcept {
        return glad_glUniform4f;
    }
    decltype(glad_glUniformMatrix2fv)& glUniformMatrix2fv_slot() noexcept {
        return glad_glUniformMatrix2fv;
    }
    decltype(glad_glUniformMatrix3fv)& glUniformMatrix3fv_slot() noexcept {
        return glad_glUniformMatrix3fv;
    }
    decltype(glad_glUniformMatrix4fv)& glUniformMatrix4fv_slot() noexcept {
        return glad_glUniformMatrix4fv;
    }
    decltype(glad_glUseProgram)& glUseProgram_slot() noexcept {
        return glad_glUseProgram;
    }
    decltype(glad_glVertexAttribPointer)& glVertexAttribPointer_slot() noexcept {
        return glad_glVertexAttribPointer;
    }
    int& GLAD_GL_ARB_texture_filter_anisotropic_slot() noexcept {
        return GLAD_GL_ARB_texture_filter_anisotropic;
    }
    int& GLAD_GL_EXT_texture_filter_anisotropic_slot() noexcept {
        return GLAD_GL_EXT_texture_filter_anisotropic;
    }
    int& GLAD_GL_KHR_debug_slot() noexcept {
        return GLAD_GL_KHR_debug;
    }
    int& GLAD_GL_VERSION_4_3_slot() noexcept {
        return GLAD_GL_VERSION_4_3;
    }
    int& GLAD_GL_VERSION_4_6_slot() noexcept {
        return GLAD_GL_VERSION_4_6;
    }
}
