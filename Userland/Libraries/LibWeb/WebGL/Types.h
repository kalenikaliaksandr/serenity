/*
 * Copyright (c) 2022, Luke Wilde <lukew@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

namespace Web::WebGL {
typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef int GLint;
typedef unsigned int GLuint;
typedef int GLsizei;
typedef float GLfloat;
typedef double GLdouble;
typedef GLfloat GLclampf;
typedef unsigned int GLbitfield;
typedef char GLchar;

const GLenum FRAGMENT_SHADER = 0x8B30;
const GLenum VERTEX_SHADER = 0x8B31;

}
