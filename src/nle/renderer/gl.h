/**
 * @file gl.h
 * @brief OpenGL's functions, from wherever this platform keeps them.
 *
 * Desktop OpenGL 3.3 through GLEW, or OpenGL ES 3 where that is all there is
 * (NLE_GLES): a phone. The engine draws with what the two have in common;
 * the handful of places that differ say so where they are.
 */

#pragma once

#ifdef NLE_GLES
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>

// A desktop's ways of filling a triangle, named so the engine can keep asking
// for them by name. OpenGL ES only fills, and nothing passes these to it.
#ifndef GL_POINT
#define GL_POINT 0x1B00
#define GL_LINE 0x1B01
#define GL_FILL 0x1B02
#endif
#else
#include <GL/glew.h>
#endif
