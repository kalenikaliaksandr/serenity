/*
 * Copyright (c) 2023, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWeb/Painting/PaintableBox.h>

namespace Web::Painting {

class ViewportPaintable final : public PaintableWithLines {
    JS_CELL(ViewportPaintable, PaintableWithLines);

public:
    static JS::NonnullGCPtr<ViewportPaintable> create(Layout::Viewport const&);
    virtual ~ViewportPaintable() override;

    void paint_all_phases(PaintContext&);
    void build_stacking_context_tree_if_needed();

    struct ScrollFrame {
        i32 id { -1 };
        CSSPixelPoint offset;
    };
    void resolve_paint_only_properties();

    HashMap<Painting::PaintableBox const*, ScrollFrame> m_scroll_frames;
    bool m_needs_to_resolve_paint_only_properties { true };

    void assign_scroll_frame_ids();

private:
    void build_stacking_context_tree();
    void assign_clip_rectangles();

    explicit ViewportPaintable(Layout::Viewport const&);
};

}
