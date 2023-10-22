/*
 * Copyright (c) 2023, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibAccelGfx/Forward.h>
#include <LibGfx/Bitmap.h>
#include <LibGfx/Rect.h>

namespace AccelGfx {

class Canvas {
public:
    static ErrorOr<Canvas> create(Gfx::IntSize);

    [[nodiscard]] Gfx::IntSize size() const { return m_bitmap->size(); }
    [[nodiscard]] int width() const { return m_bitmap->width(); }
    [[nodiscard]] int height() const { return m_bitmap->height(); }

    void flush();

    [[nodiscard]] Gfx::Bitmap const& bitmap() const { return *m_bitmap; }

private:
    explicit Canvas(NonnullRefPtr<Gfx::Bitmap>);

    void initialize();

    NonnullRefPtr<Gfx::Bitmap> m_bitmap;
};

}
