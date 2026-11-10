#pragma once
// Host-side fake uses type and function declarations only; it never opens an
// OpenGL context or links the system OpenGL implementation.
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
