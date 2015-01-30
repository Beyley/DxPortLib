/*
  DxPortLib - A portability library for DxLib-based software.
  Copyright (C) 2013-2014 Patrick McCarthy <mauve@sandwich.net>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.
  
  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:
    
  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required. 
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
 */

#include "DxBuildConfig.h"

#ifdef DXPORTLIB_DRAW_OPENGL

#include "PLInternal.h"

#include "PLGLInternal.h"

#include "SDL.h"

typedef enum {
    PLGL_SHADER_BASIC_NOTEX,
    PLGL_SHADER_BASIC_TEX1,
    PLGL_SHADER_END
} PLGLShaderType;

typedef struct _PLGLShaderDefinition {
    const char *vertexShader;
    const char *fragmentShader;
    int textureCount;
    int texcoordCount;
    int hasColor;
} PLGLShaderDefinition;

typedef struct _PLGLShaderInfo {
    PLGLShaderDefinition definition;
    GLuint glVertexShaderID;
    GLuint glFragmentShaderID;
    GLuint glProgramID;
    
    GLuint glVertexAttribID;
    GLuint glTextureUniformID[4];
    GLuint glTexcoordAttribID[4];
    GLuint glColorAttribID;
} PLGLShaderInfo;

static const PLGLShaderDefinition s_stockShaderDefinitions[PLGL_SHADER_END] = {
    /* PLGL_SHADER_BASIC_NOTEX */
    {
        /* vertex shader */
        "attribute vec4 position;\n"
        "attribute vec4 color;\n"
        "uniform mat4 modelView;\n"
        "uniform mat4 projection;\n"
        "varying vec4 outColor;\n"
        "void main() {\n"
        "    gl_Position = projection * (modelView * position);\n"
        "    outColor = color;\n"
        "}\n",
        /* fragment shader */
        "precision mediump float;\n"
        "varying vec4 outColor;\n"
        "void main() {\n"
        "    gl_FragColor = outColor;\n"
        "}\n",
        0, 0, 1
    },
    /* PLGL_SHADER_BASIC_TEX1 */
    {
        /* vertex shader */
        "attribute vec4 position;\n"
        "attribute vec2 texcoord;\n"
        "attribute vec4 color;\n"
        "uniform mat4 modelView;\n"
        "uniform mat4 projection;\n"
        "varying vec2 outTexcoord;\n"
        "varying vec4 outColor;\n"
        "void main() {\n"
        "    gl_Position = projection * (modelView * position);\n"
        "    outColor = color;\n"
        "    outTexcoord = texcoord;\n"
        "}\n",
        /* fragment shader */
        "precision mediump float;\n"
        "uniform sampler2D texture;\n"
        "varying vec2 outTexcoord;\n"
        "varying vec4 outColor;\n"
        "void main() {\n"
        "    gl_FragColor = texture2D(texture, outTexcoord) * outColor;\n"
        "}\n",
        1, 1, 1
    }
};

int PL_Shaders_CompileDefinition(const PLGLShaderDefinition *definition) {
    GLuint glVertexShaderID = 0;
    GLuint glFragmentShaderID = 0;
    GLuint glProgramID = 0;
    int shaderHandle;
    PLGLShaderInfo *info;
    
    do {
        GLint status;
        /* Clear any GL errors out before we do anything. */
        PL_GL.glGetError();

        glProgramID = PL_GL.glCreateProgram();
        if (PL_GL.glGetError() != GL_NO_ERROR) { break; }
        
        if (definition->vertexShader != NULL) {
            glVertexShaderID = PL_GL.glCreateShader(GL_VERTEX_SHADER);
            if (PL_GL.glGetError() != GL_NO_ERROR) { break; }
            PL_GL.glShaderSource(glVertexShaderID, 1, &definition->vertexShader, NULL);
            if (PL_GL.glGetError() != GL_NO_ERROR) { break; }
            PL_GL.glCompileShader(glVertexShaderID);
            if (PL_GL.glGetError() != GL_NO_ERROR) { break; }
            PL_GL.glGetShaderiv(glVertexShaderID, GL_COMPILE_STATUS, &status);
            if (status != GL_TRUE) { break; }
            
            PL_GL.glAttachShader(glProgramID, glVertexShaderID);
            if (PL_GL.glGetError() != GL_NO_ERROR) { break; }
        }
        
        if (definition->fragmentShader != NULL) {
            glFragmentShaderID = PL_GL.glCreateShader(GL_FRAGMENT_SHADER);
            if (PL_GL.glGetError() != GL_NO_ERROR) { break; }
            PL_GL.glShaderSource(glFragmentShaderID, 1, &definition->fragmentShader, NULL);
            if (PL_GL.glGetError() != GL_NO_ERROR) { break; }
            PL_GL.glCompileShader(glFragmentShaderID);
            if (PL_GL.glGetError() != GL_NO_ERROR) { break; }
            PL_GL.glGetShaderiv(glFragmentShaderID, GL_COMPILE_STATUS, &status);
            if (status != GL_TRUE) { break; }
            
            PL_GL.glAttachShader(glProgramID, glFragmentShaderID);
            if (PL_GL.glGetError() != GL_NO_ERROR) { break; }
        }
        
        shaderHandle = PL_Handle_AcquireID(DXHANDLE_SHADER);
        info = (PLGLShaderInfo *)PL_Handle_AllocateData(shaderHandle, sizeof(PLGLShaderInfo));
        memset(info, 0, sizeof(PLGLShaderInfo));
        
        memcpy(&info->definition, definition, sizeof(PLGLShaderDefinition));
        info->definition.vertexShader = NULL;
        info->definition.fragmentShader = NULL;
        
        info->glVertexAttribID = PL_GL.glGetAttribLocation(glProgramID, "position");
        if (info->definition.textureCount >= 1) {
            info->glTextureUniformID[0] = PL_GL.glGetUniformLocation(glProgramID, "texture");
        }
        if (info->definition.textureCount >= 2) {
            info->glTextureUniformID[1] = PL_GL.glGetUniformLocation(glProgramID, "texture1");
        }
        if (info->definition.textureCount >= 3) {
            info->glTextureUniformID[2] = PL_GL.glGetUniformLocation(glProgramID, "texture2");
        }
        if (info->definition.textureCount >= 4) {
            info->glTextureUniformID[3] = PL_GL.glGetUniformLocation(glProgramID, "texture3");
        }
        if (info->definition.texcoordCount >= 1) {
            info->glTexcoordAttribID[0] = PL_GL.glGetAttribLocation(glProgramID, "texcoord");
        }
        if (info->definition.texcoordCount >= 2) {
            info->glTexcoordAttribID[1] = PL_GL.glGetAttribLocation(glProgramID, "texcoord2");
        }
        if (info->definition.texcoordCount >= 3) {
            info->glTexcoordAttribID[2] = PL_GL.glGetAttribLocation(glProgramID, "texcoord3");
        }
        if (info->definition.texcoordCount >= 4) {
            info->glTexcoordAttribID[3] = PL_GL.glGetAttribLocation(glProgramID, "texcoord4");
        }
        if (info->definition.hasColor) {
            info->glColorAttribID = PL_GL.glGetAttribLocation(glProgramID, "color");
        }
        
        return shaderHandle;
    } while(0);
    
    if (glVertexShaderID != 0) {
        PL_GL.glDeleteShader(glVertexShaderID);
    }
    if (glFragmentShaderID != 0) {
        PL_GL.glDeleteShader(glFragmentShaderID);
    }
    if (glProgramID != 0) {
        PL_GL.glDeleteProgram(glProgramID);
    }
    
    return -1;
}

void PL_Shaders_DeleteShader(int shaderHandle) {
    PLGLShaderInfo *info = (PLGLShaderInfo *)PL_Handle_GetData(shaderHandle, DXHANDLE_SHADER);
    
    if (info != NULL) {
        if (info->glVertexShaderID != 0) {
            PL_GL.glDeleteShader(info->glVertexShaderID);
        }
        if (info->glFragmentShaderID != 0) {
            PL_GL.glDeleteShader(info->glFragmentShaderID);
        }
        PL_GL.glDeleteProgram(info->glProgramID);
        
        PL_Handle_ReleaseID(shaderHandle, DXTRUE);
    }
}

static GLenum VertexElementSizeToGL(int value) {
    switch(value) {
        case VERTEXSIZE_UNSIGNED_BYTE:
            return GL_UNSIGNED_BYTE;
        default: /* VERTEXSIZE_FLOAT */
            return GL_FLOAT;
    }
}

void PL_Shaders_ApplyProgram(int shaderHandle,
                             const char *vertexData, const VertexDefinition *definition,
                             int *textureIDs, int textureCount)
{
    PLGLShaderInfo *info = (PLGLShaderInfo *)PL_Handle_GetData(shaderHandle, DXHANDLE_SHADER);
    int i;
    
    if (info == NULL) {
        return;
    }
    
    PL_GL.glUseProgram(info->glProgramID);
    
    if (textureCount > info->definition.textureCount) {
        textureCount = info->definition.textureCount;
    }
    for (i = 0; i < textureCount; ++i) {
        PL_GL.glUniform1i(info->glTextureUniformID[i], i);
    }
    
    if (definition != NULL) {
        const VertexElement *e = definition->elements;
        int elementCount = definition->elementCount;
        int vertexDataSize = definition->vertexByteSize;
        
        for (i = 0; i < elementCount; ++i, ++e) {
            GLenum vertexType = VertexElementSizeToGL(e->vertexElementSize);
            switch (e->vertexType) {
                case VERTEX_POSITION:
                    PL_GL.glEnableVertexAttribArray(info->glVertexAttribID);
                    PL_GL.glVertexAttribPointer(info->glVertexAttribID,
                                                e->size, vertexType, GL_FALSE,
                                                vertexDataSize, vertexData + e->offset);
                    break; 
                case VERTEX_TEXCOORD0:
                case VERTEX_TEXCOORD1:
                case VERTEX_TEXCOORD2:
                case VERTEX_TEXCOORD3: {
                        GLint attribID = info->glTexcoordAttribID[e->vertexType - VERTEX_TEXCOORD0];
                        if (attribID != 0) {
                            PL_GL.glEnableVertexAttribArray(attribID);
                            PL_GL.glVertexAttribPointer(attribID,
                                                        e->size, vertexType, GL_FALSE,
                                                        vertexDataSize, vertexData + e->offset);
                        }
                        break;
                    }
                case VERTEX_COLOR:
                    if (info->glColorAttribID != 0) {
                        PL_GL.glEnableVertexAttribArray(info->glColorAttribID);
                        PL_GL.glVertexAttribPointer(info->glColorAttribID,
                                                    e->size, vertexType, GL_FALSE,
                                                    vertexDataSize, vertexData + e->offset);
                    }
                    break;
            }
        }
    }
}

void PL_Shaders_ClearProgram(int shaderHandle, const VertexDefinition *definition) {
    PLGLShaderInfo *info = (PLGLShaderInfo *)PL_Handle_GetData(shaderHandle, DXHANDLE_SHADER);
    int i;
    
    if (info == NULL) {
        return;
    }
    
    if (definition != NULL) {
        const VertexElement *e = definition->elements;
        int elementCount = definition->elementCount;
        
        for (i = 0; i < elementCount; ++i, ++e) {
            switch (e->vertexType) {
                case VERTEX_POSITION:
                    PL_GL.glDisableVertexAttribArray(info->glVertexAttribID);
                    break; 
                case VERTEX_TEXCOORD0:
                case VERTEX_TEXCOORD1:
                case VERTEX_TEXCOORD2:
                case VERTEX_TEXCOORD3: {
                        GLint attribID = info->glTexcoordAttribID[e->vertexType - VERTEX_TEXCOORD0];
                        if (attribID != 0) {
                            PL_GL.glDisableVertexAttribArray(attribID);
                        }
                        break;
                    }
                case VERTEX_COLOR:
                    if (info->glColorAttribID != 0) {
                        PL_GL.glDisableVertexAttribArray(info->glColorAttribID);
                    }
                    break;
            }
        }
    }
    
    PL_GL.glUseProgram(0);
}

static int s_stockShaderIDs[PLGL_SHADER_END] = { 0 };

int PL_Shaders_GetStockProgramForID(PLGLShaderType shaderType) {
    return s_stockShaderIDs[shaderType];
}

void PL_Shaders_Init() {
    int i;
    
    for (i = 0; i < PLGL_SHADER_END; ++i) {
        s_stockShaderIDs[i] = PL_Shaders_CompileDefinition(&s_stockShaderDefinitions[i]);
    }
}

void PL_Shaders_Cleanup() {
    int i;
    
    for (i = 0; i < PLGL_SHADER_END; ++i) {
        PL_Shaders_DeleteShader(s_stockShaderIDs[i]);
        s_stockShaderIDs[i] = 0;
    }
}

#endif
