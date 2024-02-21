/* mygl.h
 * Some trivial OpenGL ES 2 helpers, keeping as simple as I can.
 * This isn't the real thing. We're going to supply helpful client-side GL helpers, better than this.
 */

#include "egg/egg.h"
#include <GLES2/gl2.h>
#include <stdlib.h>
#include <limits.h>

/* Compile one part.
 */
 
static int mygl_compile_1(const char *name,const char *src,int type) {
  int shader=glCreateShader(type);
  if (shader<=0) return -1;

  char version[]="#version 120\n";//TODO
  int versionc=sizeof(version)-1;
  int srcc=0; while (src[srcc]) srcc++;
  const char *srcv[2]={version,src};
  GLint lenv[2]={versionc,srcc};
  glShaderSource(shader,2,srcv,lenv);
  
  glCompileShader(shader);
  GLint status=0;
  glGetShaderiv(shader,GL_COMPILE_STATUS,&status);
  if (status) return shader;
  
  int err=-1;
  GLint loga=0;
  glGetShaderiv(shader,GL_INFO_LOG_LENGTH,&loga);
  if ((loga>0)&&(loga<INT_MAX)) {
    char *log=malloc(loga);
    if (log) {
      GLint logc=0;
      glGetShaderInfoLog(shader,loga,&logc,log);
      while (logc&&((unsigned char)log[logc-1]<=0x20)) logc--;
      if ((logc>0)&&(logc<=loga)) {
        egg_log(
          "Error compiling '%s' %s shader:\n%.*s",
          name,(type==GL_VERTEX_SHADER)?"vertex":"fragment",logc,log
        );
        err=-2;
      }
      free(log);
    }
  }
  glDeleteShader(shader);
  return -1;
}

/* Compile shader.
 */

int mygl_compile_shader(const char *name,const char *vsrc,const char *fsrc) {

  int vshader=mygl_compile_1(name,vsrc,GL_VERTEX_SHADER);
  if (vshader<=0) return -1;
  int fshader=mygl_compile_1(name,fsrc,GL_FRAGMENT_SHADER);
  if (fshader<=0) {
    glDeleteShader(vshader);
    return -1;
  }

  int program=glCreateProgram();
  if (program<=0) {
    glDeleteShader(vshader);
    glDeleteShader(fshader);
    return -1;
  }
  glAttachShader(program,vshader);
  glAttachShader(program,fshader);
  glDeleteShader(vshader);
  glDeleteShader(fshader);
  
  glLinkProgram(program);
  GLint status=0;
  glGetProgramiv(program,GL_LINK_STATUS,&status);
  if (status) return program;

  /* Link failed. */
  int err=-1;
  GLint loga=0;
  glGetProgramiv(program,GL_INFO_LOG_LENGTH,&loga);
  if ((loga>0)&&(loga<INT_MAX)) {
    char *log=malloc(loga);
    if (log) {
      GLint logc=0;
      glGetProgramInfoLog(program,loga,&logc,log);
      while (logc&&((unsigned char)log[logc-1]<=0x20)) logc--;
      if ((logc>0)&&(logc<=loga)) {
        egg_log(
          "Error linking '%s' shader:\n%.*s",
          name,logc,log
        );
        err=-2;
      }
      free(log);
    }
  }
  glDeleteProgram(program);
  return -1;
}
