/*
 * Copyright (c) 2023, Aliaksandr Kalenik <kalenik.aliaksandr@gmail.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibAccelGfx/GlyphAtlas.h>
#include <LibWeb/Painting/BorderRadiusCornerClipper.h>
#include <LibWeb/Painting/PaintingCommandExecutorGPU.h>
#include <LibWeb/Painting/ShadowPainting.h>

namespace Web::Painting {

PaintingCommandExecutorGPU::PaintingCommandExecutorGPU(AccelGfx::Context& context, Gfx::Bitmap& bitmap)
    : m_target_bitmap(bitmap)
    , m_context(context)
{
    auto canvas = AccelGfx::Canvas::create(bitmap.size());
    auto painter = AccelGfx::Painter::create(m_context, canvas);
    m_stacking_contexts.append({ .canvas = canvas,
        .painter = move(painter),
        .opacity = 1.0f,
        .destination = {},
        .transform = {} });
}

PaintingCommandExecutorGPU::~PaintingCommandExecutorGPU()
{
    VERIFY(m_stacking_contexts.size() == 1);
    painter().flush(m_target_bitmap);
}

CommandResult PaintingCommandExecutorGPU::draw_glyph_run(Vector<Gfx::DrawGlyphOrEmoji> const& glyph_run, Color const& color)
{
    painter().draw_glyph_run(glyph_run, color);
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::draw_text(Gfx::IntRect const&, String const&, Gfx::TextAlignment, Color const&, Gfx::TextElision, Gfx::TextWrapping, Optional<NonnullRefPtr<Gfx::Font>> const&)
{
    // FIXME
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::fill_rect(Gfx::IntRect const& rect, Color const& color)
{
    painter().fill_rect(rect, color);
    return CommandResult::Continue;
}

static AccelGfx::Painter::ScalingMode to_accelgfx_scaling_mode(Gfx::Painter::ScalingMode scaling_mode)
{
    switch (scaling_mode) {
    case Gfx::Painter::ScalingMode::NearestNeighbor:
    case Gfx::Painter::ScalingMode::BoxSampling:
    case Gfx::Painter::ScalingMode::SmoothPixels:
    case Gfx::Painter::ScalingMode::None:
        return AccelGfx::Painter::ScalingMode::NearestNeighbor;
    case Gfx::Painter::ScalingMode::BilinearBlend:
        return AccelGfx::Painter::ScalingMode::Bilinear;
    default:
        VERIFY_NOT_REACHED();
    }
}

CommandResult PaintingCommandExecutorGPU::draw_scaled_bitmap(Gfx::IntRect const& dst_rect, Gfx::Bitmap const& bitmap, Gfx::IntRect const& src_rect, Gfx::Painter::ScalingMode scaling_mode)
{
    painter().draw_scaled_bitmap(dst_rect, bitmap, src_rect, to_accelgfx_scaling_mode(scaling_mode));
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::draw_scaled_immutable_bitmap(Gfx::IntRect const& dst_rect, Gfx::ImmutableBitmap const& immutable_bitmap, Gfx::IntRect const& src_rect, Gfx::Painter::ScalingMode scaling_mode)
{
    painter().draw_scaled_immutable_bitmap(dst_rect, immutable_bitmap, src_rect, to_accelgfx_scaling_mode(scaling_mode));
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::set_clip_rect(Gfx::IntRect const& rect)
{
    painter().set_clip_rect(rect);
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::clear_clip_rect()
{
    painter().clear_clip_rect();
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::set_font(Gfx::Font const&)
{
    // FIXME
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::push_stacking_context(float opacity, bool is_fixed_position, Gfx::IntRect const& source_paintable_rect, Gfx::IntPoint post_transform_translation, CSS::ImageRendering, StackingContextTransform transform, Optional<StackingContextMask>)
{
    if (source_paintable_rect.is_empty())
        return CommandResult::SkipStackingContext;

    m_stacking_contexts.last().stacking_context_depth++;
    painter().save();
    if (is_fixed_position) {
        auto const& translation = painter().transform().translation();
        painter().translate(-translation);
    }

    auto stacking_context_transform = Gfx::extract_2d_affine_transform(transform.matrix);

    Gfx::AffineTransform inverse_origin_translation;
    inverse_origin_translation.translate(-transform.origin);
    Gfx::AffineTransform origin_translation;
    origin_translation.translate(transform.origin);

    Gfx::AffineTransform final_transform = origin_translation;
    final_transform.multiply(stacking_context_transform);
    final_transform.multiply(inverse_origin_translation);
    if (opacity < 1 || !stacking_context_transform.is_identity_or_translation()) {
        auto canvas = AccelGfx::Canvas::create(source_paintable_rect.size());
        auto painter = AccelGfx::Painter::create(m_context, canvas);
        painter->translate(-source_paintable_rect.location().to_type<float>());
        painter->clear(Color::Transparent);
        m_stacking_contexts.append({ .canvas = canvas,
            .painter = move(painter),
            .opacity = opacity,
            .destination = source_paintable_rect,
            .transform = final_transform });
    } else {
        painter().translate(stacking_context_transform.translation() + post_transform_translation.to_type<float>());
        m_stacking_contexts.append({ .canvas = {},
            .painter = MaybeOwned(painter()),
            .opacity = opacity,
            .destination = {},
            .transform = final_transform });
    }
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::pop_stacking_context()
{
    auto stacking_context = m_stacking_contexts.take_last();
    VERIFY(stacking_context.stacking_context_depth == 0);
    if (stacking_context.painter.is_owned()) {
        painter().blit_canvas(stacking_context.destination, *stacking_context.canvas, stacking_context.opacity, stacking_context.transform);
    }
    painter().restore();
    m_stacking_contexts.last().stacking_context_depth--;
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::paint_linear_gradient(Gfx::IntRect const& rect, Web::Painting::LinearGradientData const& data)
{
    painter().fill_rect_with_linear_gradient(rect, data.color_stops.list, data.gradient_angle, data.color_stops.repeat_length);
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::paint_outer_box_shadow(PaintOuterBoxShadowParams const& params)
{
    auto shadow_config = get_outer_box_shadow_configuration(params);
    (void)shadow_config;

    dbgln(">>>paint_outer_box_shadow");

    auto const& box_shadow_data = params.box_shadow_data;
    auto const& inner_bounding_rect = shadow_config.inner_bounding_rect.to_type<int>();

    auto const& top_left_corner_rect = shadow_config.top_left_corner_rect.to_type<int>();
    auto const& top_right_corner_rect = shadow_config.top_right_corner_rect.to_type<int>();
    auto const& bottom_right_corner_rect = shadow_config.bottom_right_corner_rect.to_type<int>();
    auto const& bottom_left_corner_rect = shadow_config.bottom_left_corner_rect.to_type<int>();

    auto const& blurred_edge_thickness = shadow_config.blurred_edge_thickness.value();

    auto& painter = this->painter();

    dbgln(">blurred_edge_thickness: {}", blurred_edge_thickness);

    auto const& shadow_bitmap_rect = shadow_config.shadow_bitmap_rect.to_type<int>();

    auto paint_shadow_infill = [&] {
//        if (!params.border_radii.has_any_radius()) {
//            dbgln(">>>paint_shadow_infill: no border radii inner_bounding_rect=({})", inner_bounding_rect);
//            return painter.fill_rect(inner_bounding_rect.to_type<int>(), box_shadow_data.color);
//        }

        auto top_left_inner_width = top_left_corner_rect.width() - blurred_edge_thickness;
        auto top_left_inner_height = top_left_corner_rect.height() - blurred_edge_thickness;
        auto top_right_inner_width = top_right_corner_rect.width() - blurred_edge_thickness;
        auto top_right_inner_height = top_right_corner_rect.height() - blurred_edge_thickness;
        auto bottom_right_inner_width = bottom_right_corner_rect.width() - blurred_edge_thickness;
        auto bottom_right_inner_height = bottom_right_corner_rect.height() - blurred_edge_thickness;
        auto bottom_left_inner_width = bottom_left_corner_rect.width() - blurred_edge_thickness;
        auto bottom_left_inner_height = bottom_left_corner_rect.height() - blurred_edge_thickness;

        Gfx::IntRect const top_rect {
            inner_bounding_rect.x() + top_left_inner_width,
            inner_bounding_rect.y(),
            inner_bounding_rect.width() - top_left_inner_width - top_right_inner_width,
            top_left_inner_height
        };
        Gfx::IntRect const right_rect {
            inner_bounding_rect.x() + inner_bounding_rect.width() - top_right_inner_width,
            inner_bounding_rect.y() + top_right_inner_height,
            top_right_inner_width,
            inner_bounding_rect.height() - top_right_inner_height - bottom_right_inner_height
        };
        Gfx::IntRect const bottom_rect {
            inner_bounding_rect.x() + bottom_left_inner_width,
            inner_bounding_rect.y() + inner_bounding_rect.height() - bottom_right_inner_height,
            inner_bounding_rect.width() - bottom_left_inner_width - bottom_right_inner_width,
            bottom_right_inner_height
        };
        Gfx::IntRect const left_rect {
            inner_bounding_rect.x(),
            inner_bounding_rect.y() + top_left_inner_height,
            bottom_left_inner_width,
            inner_bounding_rect.height() - top_left_inner_height - bottom_left_inner_height
        };
        Gfx::IntRect const inner = {
            left_rect.x() + left_rect.width(),
            left_rect.y(),
            inner_bounding_rect.width() - left_rect.width() - right_rect.width(),
            inner_bounding_rect.height() - top_rect.height() - bottom_rect.height()
        };

        painter.fill_rect(top_rect, box_shadow_data.color);
        painter.fill_rect(right_rect, box_shadow_data.color);
        painter.fill_rect(bottom_rect, box_shadow_data.color);
        painter.fill_rect(left_rect, box_shadow_data.color);
        painter.fill_rect(inner, box_shadow_data.color);
    };

    paint_shadow_infill();

    auto shadow_canvas = AccelGfx::Canvas::create(shadow_bitmap_rect.size());
    auto shadow_painter = AccelGfx::Painter::create(m_context, shadow_canvas);
    shadow_painter->clear(params.box_shadow_data.color.with_alpha(0));

    auto top_left_shadow_corner = AccelGfx::Painter::CornerRadius { (float)shadow_config.top_left_shadow_corner.horizontal_radius, (float)shadow_config.top_left_shadow_corner.vertical_radius };
    auto top_right_shadow_corner = AccelGfx::Painter::CornerRadius { (float)shadow_config.top_right_shadow_corner.horizontal_radius, (float)shadow_config.top_right_shadow_corner.vertical_radius };
    auto bottom_right_shadow_corner = AccelGfx::Painter::CornerRadius { (float)shadow_config.bottom_right_shadow_corner.horizontal_radius, (float)shadow_config.bottom_right_shadow_corner.vertical_radius };
    auto bottom_left_shadow_corner = AccelGfx::Painter::CornerRadius { (float)shadow_config.bottom_left_shadow_corner.horizontal_radius, (float)shadow_config.bottom_left_shadow_corner.vertical_radius };

    auto const& double_radius = shadow_config.double_radius.value();

    shadow_painter->fill_rect_with_rounded_corners(
        shadow_bitmap_rect.shrunken(double_radius, double_radius, double_radius, double_radius),
        params.box_shadow_data.color, top_left_shadow_corner, top_right_shadow_corner, bottom_right_shadow_corner, bottom_left_shadow_corner);

    auto shadow_texture = shadow_canvas->framebuffer().texture;

    auto horizontal_blurred_shadow_canvas = AccelGfx::Canvas::create(shadow_bitmap_rect.size());
    auto horizontal_blurred_shadow_painter = AccelGfx::Painter::create(m_context, horizontal_blurred_shadow_canvas);
    horizontal_blurred_shadow_painter->clear(params.box_shadow_data.color.with_alpha(0));
    horizontal_blurred_shadow_painter->blit_blurred_texture(shadow_bitmap_rect.to_type<float>(), shadow_texture, shadow_bitmap_rect.to_type<float>(), shadow_config.blur_radius.value(), AccelGfx::Painter::BlurDirection::Horizontal);

    auto vertical_blurred_shadow_canvas = AccelGfx::Canvas::create(shadow_bitmap_rect.size());
    auto vertical_blurred_shadow_painter = AccelGfx::Painter::create(m_context, vertical_blurred_shadow_canvas);
    vertical_blurred_shadow_painter->clear(params.box_shadow_data.color.with_alpha(0));
    vertical_blurred_shadow_painter->blit_blurred_texture(shadow_bitmap_rect.to_type<float>(), horizontal_blurred_shadow_canvas->framebuffer().texture, shadow_bitmap_rect.to_type<float>(), shadow_config.blur_radius.value(), AccelGfx::Painter::BlurDirection::Vertical);

    auto blurred_shadow_texture = vertical_blurred_shadow_canvas->framebuffer().texture;

    auto const& bottom_right_corner_size = shadow_config.bottom_right_corner_size.to_type<int>();
    auto const& bottom_left_corner_size = shadow_config.bottom_left_corner_size.to_type<int>();
    auto bottom_start = shadow_config.bottom_start.value();
    auto bottom_edge_rect = shadow_config.bottom_edge_rect.to_type<int>();
    for (auto x = inner_bounding_rect.left() + (bottom_left_corner_size.width() - double_radius); x < inner_bounding_rect.right() - (bottom_right_corner_size.width() - double_radius); ++x) {
        painter.blit_scaled_texture(Gfx::FloatRect { { x, bottom_start },  bottom_edge_rect.size() }, blurred_shadow_texture, bottom_edge_rect.to_type<float>(), to_accelgfx_scaling_mode(Gfx::Painter::ScalingMode::NearestNeighbor));
    }

    auto top_start = shadow_config.top_start.value();
    auto top_edge_rect = shadow_config.top_edge_rect.to_type<int>();
    auto top_left_corner_size = shadow_config.top_left_corner_size.to_type<int>();
    auto top_right_corner_size = shadow_config.top_right_corner_size.to_type<int>();
    for (auto x = inner_bounding_rect.left() + (top_left_corner_size.width() - double_radius); x < inner_bounding_rect.right() - (top_right_corner_size.width() - double_radius); ++x) {
        painter.blit_scaled_texture(Gfx::FloatRect { { x, top_start },  top_edge_rect.size() }, blurred_shadow_texture, top_edge_rect.to_type<float>(), to_accelgfx_scaling_mode(Gfx::Painter::ScalingMode::NearestNeighbor));
    }

    auto left_start = shadow_config.left_start.value();
    auto left_edge_rect = shadow_config.left_edge_rect.to_type<int>();
    for (auto y = inner_bounding_rect.top() + (top_left_corner_size.height() - double_radius); y < inner_bounding_rect.bottom() - (bottom_left_corner_size.height() - double_radius); ++y) {
        painter.blit_scaled_texture(Gfx::FloatRect { { left_start, y },  left_edge_rect.size() }, blurred_shadow_texture, left_edge_rect.to_type<float>(), to_accelgfx_scaling_mode(Gfx::Painter::ScalingMode::NearestNeighbor));
    }

    auto right_start = shadow_config.right_start.value();
    auto right_edge_rect = shadow_config.right_edge_rect.to_type<int>();
    for (auto y = inner_bounding_rect.top() + (top_right_corner_size.height() - double_radius); y < inner_bounding_rect.bottom() - (bottom_right_corner_size.height() - double_radius); ++y) {
        painter.blit_scaled_texture(Gfx::FloatRect { { right_start, y },  right_edge_rect.size() }, blurred_shadow_texture, right_edge_rect.to_type<float>(), to_accelgfx_scaling_mode(Gfx::Painter::ScalingMode::NearestNeighbor));
    }

    auto top_left_corner_blit_pos = shadow_config.top_left_corner_blit_pos.to_type<int>();
    auto top_right_corner_blit_pos = shadow_config.top_right_corner_blit_pos.to_type<int>();
    auto bottom_right_corner_blit_pos = shadow_config.bottom_right_corner_blit_pos.to_type<int>();
    auto bottom_left_corner_blit_pos = shadow_config.bottom_left_corner_blit_pos.to_type<int>();

    painter.blit_scaled_texture(Gfx::FloatRect { top_left_corner_blit_pos,  top_left_corner_rect.size() }, blurred_shadow_texture, top_left_corner_rect.to_type<float>(), to_accelgfx_scaling_mode(Gfx::Painter::ScalingMode::NearestNeighbor));
    painter.blit_scaled_texture(Gfx::FloatRect { top_right_corner_blit_pos,  top_right_corner_rect.size() }, blurred_shadow_texture, top_right_corner_rect.to_type<float>(), to_accelgfx_scaling_mode(Gfx::Painter::ScalingMode::NearestNeighbor));
    painter.blit_scaled_texture(Gfx::FloatRect { bottom_left_corner_blit_pos,  bottom_left_corner_rect.size() }, blurred_shadow_texture, bottom_left_corner_rect.to_type<float>(), to_accelgfx_scaling_mode(Gfx::Painter::ScalingMode::NearestNeighbor));
    painter.blit_scaled_texture(Gfx::FloatRect { bottom_right_corner_blit_pos,  bottom_right_corner_rect.size() }, blurred_shadow_texture, bottom_right_corner_rect.to_type<float>(), to_accelgfx_scaling_mode(Gfx::Painter::ScalingMode::NearestNeighbor));

    // FIXME
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::paint_inner_box_shadow(PaintOuterBoxShadowParams const&)
{
    // FIXME
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::paint_text_shadow(int blur_radius, Gfx::IntRect const& shadow_bounding_rect, Gfx::IntRect const& text_rect, Span<Gfx::DrawGlyphOrEmoji const> glyph_run, Color const& color, int fragment_baseline, Gfx::IntPoint const& draw_location)
{
    auto text_shadow_canvas = AccelGfx::Canvas::create(shadow_bounding_rect.size());
    auto text_shadow_painter = AccelGfx::Painter::create(m_context, text_shadow_canvas);
    text_shadow_painter->clear(color.with_alpha(0));

    Gfx::FloatRect const shadow_location { draw_location, shadow_bounding_rect.size() };
    Gfx::IntPoint const baseline_start(text_rect.x(), text_rect.y() + fragment_baseline);
    text_shadow_painter->translate(baseline_start.to_type<float>());
    text_shadow_painter->draw_glyph_run(glyph_run, color);
    if (blur_radius == 0) {
        painter().blit_canvas(shadow_location, *text_shadow_canvas);
        return CommandResult::Continue;
    }

    auto horizontal_blur_canvas = AccelGfx::Canvas::create(shadow_bounding_rect.size());
    auto horizontal_blur_painter = AccelGfx::Painter::create(m_context, horizontal_blur_canvas);
    horizontal_blur_painter->clear(color.with_alpha(0));
    horizontal_blur_painter->blit_blurred_canvas(shadow_bounding_rect.to_type<float>(), *text_shadow_canvas, blur_radius, AccelGfx::Painter::BlurDirection::Horizontal);
    painter().blit_blurred_canvas(shadow_location, *horizontal_blur_canvas, blur_radius, AccelGfx::Painter::BlurDirection::Vertical);
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::fill_rect_with_rounded_corners(Gfx::IntRect const& rect, Color const& color, Gfx::AntiAliasingPainter::CornerRadius const& top_left_radius, Gfx::AntiAliasingPainter::CornerRadius const& top_right_radius, Gfx::AntiAliasingPainter::CornerRadius const& bottom_left_radius, Gfx::AntiAliasingPainter::CornerRadius const& bottom_right_radius, Optional<Gfx::FloatPoint> const&)
{
    painter().fill_rect_with_rounded_corners(
        rect, color,
        { static_cast<float>(top_left_radius.horizontal_radius), static_cast<float>(top_left_radius.vertical_radius) },
        { static_cast<float>(top_right_radius.horizontal_radius), static_cast<float>(top_right_radius.vertical_radius) },
        { static_cast<float>(bottom_left_radius.horizontal_radius), static_cast<float>(bottom_left_radius.vertical_radius) },
        { static_cast<float>(bottom_right_radius.horizontal_radius), static_cast<float>(bottom_right_radius.vertical_radius) });
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::fill_path_using_color(Gfx::Path const&, Color const&, Gfx::Painter::WindingRule, Optional<Gfx::FloatPoint> const&)
{
    // FIXME
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::fill_path_using_paint_style(Gfx::Path const&, Gfx::PaintStyle const&, Gfx::Painter::WindingRule, float, Optional<Gfx::FloatPoint> const&)
{
    // FIXME
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::stroke_path_using_color(Gfx::Path const&, Color const&, float, Optional<Gfx::FloatPoint> const&)
{
    // FIXME
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::stroke_path_using_paint_style(Gfx::Path const&, Gfx::PaintStyle const&, float, float, Optional<Gfx::FloatPoint> const&)
{
    // FIXME
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::draw_ellipse(Gfx::IntRect const&, Color const&, int)
{
    // FIXME
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::fill_ellipse(Gfx::IntRect const& rect, Color const& color, Gfx::AntiAliasingPainter::BlendMode)
{
    auto horizontal_radius = static_cast<float>(rect.width() / 2);
    auto vertical_radius = static_cast<float>(rect.height() / 2);
    painter().fill_rect_with_rounded_corners(
        rect, color,
        { horizontal_radius, vertical_radius },
        { horizontal_radius, vertical_radius },
        { horizontal_radius, vertical_radius },
        { horizontal_radius, vertical_radius });
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::draw_line(Color const& color, Gfx::IntPoint const& a, Gfx::IntPoint const& b, int thickness, Gfx::Painter::LineStyle, Color const&)
{
    // FIXME: Pass line style and alternate color once AccelGfx::Painter supports it
    painter().draw_line(a, b, thickness, color);
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::draw_signed_distance_field(Gfx::IntRect const&, Color const&, Gfx::GrayscaleBitmap const&, float)
{
    // FIXME
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::paint_frame(Gfx::IntRect const&, Palette const&, Gfx::FrameStyle)
{
    // FIXME
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::apply_backdrop_filter(Gfx::IntRect const&, Web::CSS::ResolvedBackdropFilter const&)
{
    // FIXME
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::draw_rect(Gfx::IntRect const&, Color const&, bool)
{
    // FIXME
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::paint_radial_gradient(Gfx::IntRect const&, Web::Painting::RadialGradientData const&, Gfx::IntPoint const&, Gfx::IntSize const&)
{
    // FIXME
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::paint_conic_gradient(Gfx::IntRect const&, Web::Painting::ConicGradientData const&, Gfx::IntPoint const&)
{
    // FIXME
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::draw_triangle_wave(Gfx::IntPoint const&, Gfx::IntPoint const&, Color const&, int, int)
{
    // FIXME
    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::sample_under_corners(u32 id, CornerRadii const& corner_radii, Gfx::IntRect const& border_rect, CornerClip)
{
    m_corner_clippers.resize(id + 1);
    m_corner_clippers[id] = make<BorderRadiusCornerClipper>();
    auto& corner_clipper = *m_corner_clippers[id];

    auto const& top_left = corner_radii.top_left;
    auto const& top_right = corner_radii.top_right;
    auto const& bottom_right = corner_radii.bottom_right;
    auto const& bottom_left = corner_radii.bottom_left;

    auto sampling_config = calculate_border_radius_sampling_config(corner_radii, border_rect);
    auto const& page_locations = sampling_config.page_locations;
    auto const& bitmap_locations = sampling_config.bitmap_locations;

    auto top_left_corner_size = Gfx::IntSize { top_left.horizontal_radius, top_left.vertical_radius };
    auto top_right_corner_size = Gfx::IntSize { top_right.horizontal_radius, top_right.vertical_radius };
    auto bottom_right_corner_size = Gfx::IntSize { bottom_right.horizontal_radius, bottom_right.vertical_radius };
    auto bottom_left_corner_size = Gfx::IntSize { bottom_left.horizontal_radius, bottom_left.vertical_radius };

    corner_clipper.page_top_left_rect = { page_locations.top_left, top_left_corner_size };
    corner_clipper.page_top_right_rect = { page_locations.top_right, top_right_corner_size };
    corner_clipper.page_bottom_right_rect = { page_locations.bottom_right, bottom_right_corner_size };
    corner_clipper.page_bottom_left_rect = { page_locations.bottom_left, bottom_left_corner_size };

    corner_clipper.sample_canvas_top_left_rect = { bitmap_locations.top_left, top_left_corner_size };
    corner_clipper.sample_canvas_top_right_rect = { bitmap_locations.top_right, top_right_corner_size };
    corner_clipper.sample_canvas_bottom_right_rect = { bitmap_locations.bottom_right, bottom_right_corner_size };
    corner_clipper.sample_canvas_bottom_left_rect = { bitmap_locations.bottom_left, bottom_left_corner_size };

    corner_clipper.corners_sample_canvas = AccelGfx::Canvas::create(sampling_config.corners_bitmap_size);
    auto corner_painter = AccelGfx::Painter::create(m_context, *corner_clipper.corners_sample_canvas);
    corner_painter->clear(Color::White);

    corner_painter->fill_rect_with_rounded_corners(
        Gfx::IntRect { { 0, 0 }, sampling_config.corners_bitmap_size },
        Color::Transparent,
        { static_cast<float>(top_left.horizontal_radius), static_cast<float>(top_left.vertical_radius) },
        { static_cast<float>(top_right.horizontal_radius), static_cast<float>(top_right.vertical_radius) },
        { static_cast<float>(bottom_right.horizontal_radius), static_cast<float>(bottom_right.vertical_radius) },
        { static_cast<float>(bottom_left.horizontal_radius), static_cast<float>(bottom_left.vertical_radius) },
        AccelGfx::Painter::BlendingMode::AlphaOverride);

    auto const& target_canvas = painter().canvas();
    if (!corner_clipper.sample_canvas_top_left_rect.is_empty())
        corner_painter->blit_canvas(corner_clipper.sample_canvas_top_left_rect, target_canvas, painter().transform().map(corner_clipper.page_top_left_rect), 1.0f, {}, AccelGfx::Painter::BlendingMode::AlphaPreserve);
    if (!corner_clipper.sample_canvas_top_right_rect.is_empty())
        corner_painter->blit_canvas(corner_clipper.sample_canvas_top_right_rect, target_canvas, painter().transform().map(corner_clipper.page_top_right_rect), 1.0f, {}, AccelGfx::Painter::BlendingMode::AlphaPreserve);
    if (!corner_clipper.sample_canvas_bottom_right_rect.is_empty())
        corner_painter->blit_canvas(corner_clipper.sample_canvas_bottom_right_rect, target_canvas, painter().transform().map(corner_clipper.page_bottom_right_rect), 1.0f, {}, AccelGfx::Painter::BlendingMode::AlphaPreserve);
    if (!corner_clipper.sample_canvas_bottom_left_rect.is_empty())
        corner_painter->blit_canvas(corner_clipper.sample_canvas_bottom_left_rect, target_canvas, painter().transform().map(corner_clipper.page_bottom_left_rect), 1.0f, {}, AccelGfx::Painter::BlendingMode::AlphaPreserve);

    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::blit_corner_clipping(u32 id)
{
    auto const& corner_clipper = *m_corner_clippers[id];
    auto const& corner_sample_canvas = *corner_clipper.corners_sample_canvas;
    if (!corner_clipper.sample_canvas_top_left_rect.is_empty())
        painter().blit_canvas(corner_clipper.page_top_left_rect, corner_sample_canvas, corner_clipper.sample_canvas_top_left_rect);
    if (!corner_clipper.sample_canvas_top_right_rect.is_empty())
        painter().blit_canvas(corner_clipper.page_top_right_rect, corner_sample_canvas, corner_clipper.sample_canvas_top_right_rect);
    if (!corner_clipper.sample_canvas_bottom_right_rect.is_empty())
        painter().blit_canvas(corner_clipper.page_bottom_right_rect, corner_sample_canvas, corner_clipper.sample_canvas_bottom_right_rect);
    if (!corner_clipper.sample_canvas_bottom_left_rect.is_empty())
        painter().blit_canvas(corner_clipper.page_bottom_left_rect, corner_sample_canvas, corner_clipper.sample_canvas_bottom_left_rect);

    m_corner_clippers[id].clear();

    return CommandResult::Continue;
}

CommandResult PaintingCommandExecutorGPU::paint_borders(DevicePixelRect const& border_rect, CornerRadii const& corner_radii, BordersDataDevicePixels const& borders_data)
{
    // FIXME: Add support for corner radiuses
    (void)corner_radii;

    Gfx::IntRect top_border_rect = {
        border_rect.x(),
        border_rect.y(),
        border_rect.width(),
        borders_data.top.width
    };
    Gfx::IntRect right_border_rect = {
        border_rect.x() + (border_rect.width() - borders_data.right.width),
        border_rect.y(),
        borders_data.right.width,
        border_rect.height()
    };
    Gfx::IntRect bottom_border_rect = {
        border_rect.x(),
        border_rect.y() + (border_rect.height() - borders_data.bottom.width),
        border_rect.width(),
        borders_data.bottom.width
    };
    Gfx::IntRect left_border_rect = {
        border_rect.x(),
        border_rect.y(),
        borders_data.left.width,
        border_rect.height()
    };

    if (borders_data.top.width > 0)
        painter().fill_rect(top_border_rect, borders_data.top.color);
    if (borders_data.right.width > 0)
        painter().fill_rect(right_border_rect, borders_data.right.color);
    if (borders_data.bottom.width > 0)
        painter().fill_rect(bottom_border_rect, borders_data.bottom.color);
    if (borders_data.left.width > 0)
        painter().fill_rect(left_border_rect, borders_data.left.color);

    return CommandResult::Continue;
}

bool PaintingCommandExecutorGPU::would_be_fully_clipped_by_painter(Gfx::IntRect rect) const
{
    auto translation = painter().transform().translation().to_type<int>();
    return !painter().clip_rect().intersects(rect.translated(translation));
}

void PaintingCommandExecutorGPU::prepare_glyph_texture(HashMap<Gfx::Font const*, HashTable<u32>> const& unique_glyphs)
{
    AccelGfx::GlyphAtlas::the().update(unique_glyphs);
}

void PaintingCommandExecutorGPU::update_immutable_bitmap_texture_cache(HashMap<u32, Gfx::ImmutableBitmap const*>& immutable_bitmaps)
{
    painter().update_immutable_bitmap_texture_cache(immutable_bitmaps);
}

}
