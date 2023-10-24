/*
 * Copyright (c) 2023, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Noncopyable.h>
#include <AK/Vector.h>
#include <AK/HashMap.h>
#include <LibAccelGfx/Forward.h>
#include <LibGfx/AffineTransform.h>
#include <LibGfx/Forward.h>
#include <LibGfx/Font/Font.h>

namespace AccelGfx {

class Painter {
    AK_MAKE_NONCOPYABLE(Painter);
    AK_MAKE_NONMOVABLE(Painter);

public:
    Painter(Canvas&);
    ~Painter();

    void clear(Gfx::Color);

    void save();
    void restore();

    [[nodiscard]] Gfx::AffineTransform const& transform() const { return state().transform; }
    void set_transform(Gfx::AffineTransform const& transform) { state().transform = transform; }

    void fill_rect(Gfx::FloatRect, Gfx::Color);
    void fill_rect(Gfx::IntRect, Gfx::Color);

    void draw_scaled_bitmap(Gfx::IntRect const& dst_rect, Gfx::Bitmap const&, Gfx::IntRect const& src_rect);

    void draw_text_run(Gfx::FloatPoint baseline_start, Utf8View const& string, Gfx::Font const& font, Color color);

    void prepare_glyph_atlas(HashMap<NonnullRefPtr<Gfx::Font>, HashTable<u32>>);

    struct AtlasKey {
        NonnullRefPtr<Gfx::Font> font;
        u32 code_point;

        bool operator==(AtlasKey const& other) const
        {
            return font == other.font && code_point == other.code_point;
        }
    };

private:
    void flush();

    Canvas& m_canvas;

    struct State {
        Gfx::AffineTransform transform;
    };

    [[nodiscard]] State& state() { return m_state_stack.last(); }
    [[nodiscard]] State const& state() const { return m_state_stack.last(); }

    [[nodiscard]] Gfx::FloatRect to_clip_space(Gfx::FloatRect const& screen_rect) const;

    Vector<State, 1> m_state_stack;

    HashMap<AtlasKey, Gfx::IntRect> m_glyph_atlas_rects;
    RefPtr<Gfx::Bitmap> m_glyph_atlas;
};

}

namespace AK {

template<>
struct Traits<AccelGfx::Painter::AtlasKey> : public GenericTraits<AccelGfx::Painter::AtlasKey> {
    static unsigned hash(AccelGfx::Painter::AtlasKey const& key)
    {
        return pair_int_hash(ptr_hash(key.font.ptr()), key.code_point);
    }
};

}
