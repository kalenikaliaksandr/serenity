/*
 * Copyright (c) 2022, Martin Falisse <mfalisse@outlook.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/DOM/Node.h>
#include <LibWeb/Layout/Box.h>
#include <LibWeb/Layout/GridFormattingContext.h>

namespace Web::Layout {

GridFormattingContext::GridFormattingContext(LayoutState& state, BlockContainer const& block_container, FormattingContext* parent)
    : BlockFormattingContext(state, block_container, parent)
{
}

GridFormattingContext::~GridFormattingContext() = default;

void GridFormattingContext::run(Box const& box, LayoutMode, AvailableSpace const& available_space)
{
    auto& box_state = m_state.get_mutable(box);
    auto should_skip_is_anonymous_text_run = [&](Box& child_box) -> bool {
        if (child_box.is_anonymous() && !child_box.first_child_of_type<BlockContainer>()) {
            bool contains_only_white_space = true;
            child_box.for_each_in_subtree([&](auto const& node) {
                if (!is<TextNode>(node) || !static_cast<TextNode const&>(node).dom_node().data().is_whitespace()) {
                    contains_only_white_space = false;
                    return IterationDecision::Break;
                }
                return IterationDecision::Continue;
            });
            if (contains_only_white_space)
                return true;
        }
        return false;
    };

    auto resolve_definite_track_size = [&](CSS::GridTrackSize const& grid_track_size) -> float {
        VERIFY(grid_track_size.is_definite());
        switch (grid_track_size.type()) {
        case CSS::GridTrackSize::Type::Length:
            if (grid_track_size.length().is_auto())
                break;
            return grid_track_size.length().to_px(box);
            break;
        case CSS::GridTrackSize::Type::Percentage:
            return grid_track_size.percentage().as_fraction() * box_state.content_width();
            break;
        default:
            VERIFY_NOT_REACHED();
        }
        return 0;
    };

    // https://drafts.csswg.org/css-grid/#overview-placement
    // 2.2. Placing Items
    // The contents of the grid container are organized into individual grid items (analogous to
    // flex items), which are then assigned to predefined areas in the grid. They can be explicitly
    // placed using coordinates through the grid-placement properties or implicitly placed into
    // empty areas using auto-placement.
    struct PositionedBox {
        Box const& box;
        int row { 0 };
        int row_span { 1 };
        int column { 0 };
        int column_span { 1 };
        float computed_height { 0 };
    };
    Vector<PositionedBox> positioned_boxes;
    Vector<Box const&> boxes_to_place;
    box.for_each_child_of_type<Box>([&](Box& child_box) {
        if (should_skip_is_anonymous_text_run(child_box))
            return IterationDecision::Continue;
        boxes_to_place.append(child_box);
        return IterationDecision::Continue;
    });
    auto column_repeat_count = box.computed_values().grid_template_columns().is_repeat() ? box.computed_values().grid_template_columns().repeat_count() : 1;
    auto row_repeat_count = box.computed_values().grid_template_rows().is_repeat() ? box.computed_values().grid_template_rows().repeat_count() : 1;

    // https://www.w3.org/TR/css-grid-2/#auto-repeat
    // 7.2.3.2. Repeat-to-fill: auto-fill and auto-fit repetitions
    // On a subgridded axis, the auto-fill keyword is only valid once per <line-name-list>, and repeats
    // enough times for the name list to match the subgrid’s specified grid span (falling back to 0 if
    // the span is already fulfilled).

    // Otherwise on a standalone axis, when auto-fill is given as the repetition number
    if (box.computed_values().grid_template_columns().is_auto_fill() || box.computed_values().grid_template_columns().is_auto_fit()) {
        // If the grid container has a definite size or max size in the relevant axis, then the number of
        // repetitions is the largest possible positive integer that does not cause the grid to overflow the
        // content box of its grid container

        auto sum_of_grid_track_sizes = 0;
        // (treating each track as its max track sizing function if that is definite or its minimum track sizing
        // function otherwise, flooring the max track sizing function by the min track sizing function if both
        // are definite, and taking gap into account)
        // FIXME: take gap into account
        for (auto& meta_grid_track : box.computed_values().grid_template_columns().meta_grid_track_sizes()) {
            if (meta_grid_track.max_grid_track_size().is_definite() && !meta_grid_track.min_grid_track_size().is_definite())
                sum_of_grid_track_sizes += resolve_definite_track_size(meta_grid_track.max_grid_track_size());
            else if (meta_grid_track.min_grid_track_size().is_definite() && !meta_grid_track.max_grid_track_size().is_definite())
                sum_of_grid_track_sizes += resolve_definite_track_size(meta_grid_track.min_grid_track_size());
            else if (meta_grid_track.min_grid_track_size().is_definite() && meta_grid_track.max_grid_track_size().is_definite())
                sum_of_grid_track_sizes += min(resolve_definite_track_size(meta_grid_track.min_grid_track_size()), resolve_definite_track_size(meta_grid_track.max_grid_track_size()));
        }
        column_repeat_count = max(1, static_cast<int>(get_free_space_x(box) / sum_of_grid_track_sizes));

        // For the purpose of finding the number of auto-repeated tracks in a standalone axis, the UA must
        // floor the track size to a UA-specified value to avoid division by zero. It is suggested that this
        // floor be 1px.
    }
    if (box.computed_values().grid_template_rows().is_auto_fill() || box.computed_values().grid_template_rows().is_auto_fit()) {
        // If the grid container has a definite size or max size in the relevant axis, then the number of
        // repetitions is the largest possible positive integer that does not cause the grid to overflow the
        // content box of its grid container

        auto sum_of_grid_track_sizes = 0;
        // (treating each track as its max track sizing function if that is definite or its minimum track sizing
        // function otherwise, flooring the max track sizing function by the min track sizing function if both
        // are definite, and taking gap into account)
        // FIXME: take gap into account
        for (auto& meta_grid_track : box.computed_values().grid_template_rows().meta_grid_track_sizes()) {
            if (meta_grid_track.max_grid_track_size().is_definite() && !meta_grid_track.min_grid_track_size().is_definite())
                sum_of_grid_track_sizes += resolve_definite_track_size(meta_grid_track.max_grid_track_size());
            else if (meta_grid_track.min_grid_track_size().is_definite() && !meta_grid_track.max_grid_track_size().is_definite())
                sum_of_grid_track_sizes += resolve_definite_track_size(meta_grid_track.min_grid_track_size());
            else if (meta_grid_track.min_grid_track_size().is_definite() && meta_grid_track.max_grid_track_size().is_definite())
                sum_of_grid_track_sizes += min(resolve_definite_track_size(meta_grid_track.min_grid_track_size()), resolve_definite_track_size(meta_grid_track.max_grid_track_size()));
        }
        row_repeat_count = max(1, static_cast<int>(get_free_space_y(box) / sum_of_grid_track_sizes));

        // The auto-fit keyword behaves the same as auto-fill, except that after grid item placement any
        // empty repeated tracks are collapsed. An empty track is one with no in-flow grid items placed into
        // or spanning across it. (This can result in all tracks being collapsed, if they’re all empty.)

        // A collapsed track is treated as having a fixed track sizing function of 0px, and the gutters on
        // either side of it—including any space allotted through distributed alignment—collapse.

        // For the purpose of finding the number of auto-repeated tracks in a standalone axis, the UA must
        // floor the track size to a UA-specified value to avoid division by zero. It is suggested that this
        // floor be 1px.
    }
    auto occupation_grid = OccupationGrid(column_repeat_count * box.computed_values().grid_template_columns().meta_grid_track_sizes().size(), row_repeat_count * box.computed_values().grid_template_rows().meta_grid_track_sizes().size());

    // https://drafts.csswg.org/css-grid/#auto-placement-algo
    // 8.5. Grid Item Placement Algorithm

    // FIXME: 0. Generate anonymous grid items

    // 1. Position anything that's not auto-positioned.
    for (size_t i = 0; i < boxes_to_place.size(); i++) {
        auto const& child_box = boxes_to_place[i];
        if (is_auto_positioned_row(child_box.computed_values().grid_row_start(), child_box.computed_values().grid_row_end())
            || is_auto_positioned_column(child_box.computed_values().grid_column_start(), child_box.computed_values().grid_column_end()))
            continue;

        int row_start = child_box.computed_values().grid_row_start().raw_value();
        int row_end = child_box.computed_values().grid_row_end().raw_value();
        int column_start = child_box.computed_values().grid_column_start().raw_value();
        int column_end = child_box.computed_values().grid_column_end().raw_value();

        // https://drafts.csswg.org/css-grid/#line-placement
        // 8.3. Line-based Placement: the grid-row-start, grid-column-start, grid-row-end, and grid-column-end properties

        // https://drafts.csswg.org/css-grid/#grid-placement-slot
        // FIXME: <custom-ident>
        // First attempt to match the grid area’s edge to a named grid area: if there is a grid line whose
        // line name is <custom-ident>-start (for grid-*-start) / <custom-ident>-end (for grid-*-end),
        // contributes the first such line to the grid item’s placement.

        // Note: Named grid areas automatically generate implicitly-assigned line names of this form, so
        // specifying grid-row-start: foo will choose the start edge of that named grid area (unless another
        // line named foo-start was explicitly specified before it).

        // Otherwise, treat this as if the integer 1 had been specified along with the <custom-ident>.

        // https://drafts.csswg.org/css-grid/#grid-placement-int
        // [ <integer [−∞,−1]> | <integer [1,∞]> ] && <custom-ident>?
        // Contributes the Nth grid line to the grid item’s placement. If a negative integer is given, it
        // instead counts in reverse, starting from the end edge of the explicit grid.
        if (row_end < 0)
            row_end = occupation_grid.row_count() + row_end + 2;
        if (column_end < 0)
            column_end = occupation_grid.column_count() + column_end + 2;

        // If a name is given as a <custom-ident>, only lines with that name are counted. If not enough
        // lines with that name exist, all implicit grid lines are assumed to have that name for the purpose
        // of finding this position.

        // An <integer> value of zero makes the declaration invalid.

        // https://drafts.csswg.org/css-grid/#grid-placement-span-int
        // span && [ <integer [1,∞]> || <custom-ident> ]
        // Contributes a grid span to the grid item’s placement such that the corresponding edge of the grid
        // item’s grid area is N lines from its opposite edge in the corresponding direction. For example,
        // grid-column-end: span 2 indicates the second grid line in the endward direction from the
        // grid-column-start line.
        int row_span = 1;
        int column_span = 1;
        if (child_box.computed_values().grid_row_start().is_position() && child_box.computed_values().grid_row_end().is_span())
            row_span = child_box.computed_values().grid_row_end().raw_value();
        if (child_box.computed_values().grid_column_start().is_position() && child_box.computed_values().grid_column_end().is_span())
            column_span = child_box.computed_values().grid_column_end().raw_value();
        if (child_box.computed_values().grid_row_end().is_position() && child_box.computed_values().grid_row_start().is_span()) {
            row_span = child_box.computed_values().grid_row_start().raw_value();
            row_start = row_end - row_span;
        }
        if (child_box.computed_values().grid_column_end().is_position() && child_box.computed_values().grid_column_start().is_span()) {
            column_span = child_box.computed_values().grid_column_start().raw_value();
            column_start = column_end - column_span;
        }

        // If a name is given as a <custom-ident>, only lines with that name are counted. If not enough
        // lines with that name exist, all implicit grid lines on the side of the explicit grid
        // corresponding to the search direction are assumed to have that name for the purpose of counting
        // this span.

        // https://drafts.csswg.org/css-grid/#grid-placement-auto
        // auto
        // The property contributes nothing to the grid item’s placement, indicating auto-placement or a
        // default span of one. (See § 8 Placing Grid Items, above.)

        // https://drafts.csswg.org/css-grid/#grid-placement-errors
        // 8.3.1. Grid Placement Conflict Handling
        // If the placement for a grid item contains two lines, and the start line is further end-ward than
        // the end line, swap the two lines. If the start line is equal to the end line, remove the end
        // line.
        if (child_box.computed_values().grid_row_start().is_position() && child_box.computed_values().grid_row_end().is_position()) {
            if (row_start > row_end)
                swap(row_start, row_end);
            if (row_start != row_end)
                row_span = row_end - row_start;
        }
        if (child_box.computed_values().grid_column_start().is_position() && child_box.computed_values().grid_column_end().is_position()) {
            if (column_start > column_end)
                swap(column_start, column_end);
            if (column_start != column_end)
                column_span = column_end - column_start;
        }

        // If the placement contains two spans, remove the one contributed by the end grid-placement
        // property.
        if (child_box.computed_values().grid_row_start().is_span() && child_box.computed_values().grid_row_end().is_span())
            row_span = child_box.computed_values().grid_row_start().raw_value();
        if (child_box.computed_values().grid_column_start().is_span() && child_box.computed_values().grid_column_end().is_span())
            column_span = child_box.computed_values().grid_column_start().raw_value();

        // FIXME: If the placement contains only a span for a named line, replace it with a span of 1.

        row_start -= 1;
        column_start -= 1;
        positioned_boxes.append({ child_box, row_start, row_span, column_start, column_span });

        occupation_grid.maybe_add_row(row_start + row_span);
        occupation_grid.maybe_add_column(column_start + column_span);
        occupation_grid.set_occupied(column_start, column_start + column_span, row_start, row_start + row_span);
        boxes_to_place.remove(i);
        i--;
    }

    // 2. Process the items locked to a given row.
    // FIXME: Do "dense" packing
    for (size_t i = 0; i < boxes_to_place.size(); i++) {
        auto const& child_box = boxes_to_place[i];
        if (is_auto_positioned_row(child_box.computed_values().grid_row_start(), child_box.computed_values().grid_row_end()))
            continue;

        int row_start = child_box.computed_values().grid_row_start().raw_value();
        int row_end = child_box.computed_values().grid_row_end().raw_value();

        // https://drafts.csswg.org/css-grid/#line-placement
        // 8.3. Line-based Placement: the grid-row-start, grid-column-start, grid-row-end, and grid-column-end properties

        // https://drafts.csswg.org/css-grid/#grid-placement-slot
        // FIXME: <custom-ident>
        // First attempt to match the grid area’s edge to a named grid area: if there is a grid line whose
        // line name is <custom-ident>-start (for grid-*-start) / <custom-ident>-end (for grid-*-end),
        // contributes the first such line to the grid item’s placement.

        // Note: Named grid areas automatically generate implicitly-assigned line names of this form, so
        // specifying grid-row-start: foo will choose the start edge of that named grid area (unless another
        // line named foo-start was explicitly specified before it).

        // Otherwise, treat this as if the integer 1 had been specified along with the <custom-ident>.

        // https://drafts.csswg.org/css-grid/#grid-placement-int
        // [ <integer [−∞,−1]> | <integer [1,∞]> ] && <custom-ident>?
        // Contributes the Nth grid line to the grid item’s placement. If a negative integer is given, it
        // instead counts in reverse, starting from the end edge of the explicit grid.
        if (row_end < 0)
            row_end = occupation_grid.row_count() + row_end + 2;

        // If a name is given as a <custom-ident>, only lines with that name are counted. If not enough
        // lines with that name exist, all implicit grid lines are assumed to have that name for the purpose
        // of finding this position.

        // An <integer> value of zero makes the declaration invalid.

        // https://drafts.csswg.org/css-grid/#grid-placement-span-int
        // span && [ <integer [1,∞]> || <custom-ident> ]
        // Contributes a grid span to the grid item’s placement such that the corresponding edge of the grid
        // item’s grid area is N lines from its opposite edge in the corresponding direction. For example,
        // grid-column-end: span 2 indicates the second grid line in the endward direction from the
        // grid-column-start line.
        int row_span = 1;
        if (child_box.computed_values().grid_row_start().is_position() && child_box.computed_values().grid_row_end().is_span())
            row_span = child_box.computed_values().grid_row_end().raw_value();
        if (child_box.computed_values().grid_row_end().is_position() && child_box.computed_values().grid_row_start().is_span()) {
            row_span = child_box.computed_values().grid_row_start().raw_value();
            row_start = row_end - row_span;
            // FIXME: Remove me once have implemented spans overflowing into negative indexes, e.g., grid-row: span 2 / 1
            if (row_start < 0)
                row_start = 1;
        }

        // If a name is given as a <custom-ident>, only lines with that name are counted. If not enough
        // lines with that name exist, all implicit grid lines on the side of the explicit grid
        // corresponding to the search direction are assumed to have that name for the purpose of counting
        // this span.

        // https://drafts.csswg.org/css-grid/#grid-placement-auto
        // auto
        // The property contributes nothing to the grid item’s placement, indicating auto-placement or a
        // default span of one. (See § 8 Placing Grid Items, above.)

        // https://drafts.csswg.org/css-grid/#grid-placement-errors
        // 8.3.1. Grid Placement Conflict Handling
        // If the placement for a grid item contains two lines, and the start line is further end-ward than
        // the end line, swap the two lines. If the start line is equal to the end line, remove the end
        // line.
        if (child_box.computed_values().grid_row_start().is_position() && child_box.computed_values().grid_row_end().is_position()) {
            if (row_start > row_end)
                swap(row_start, row_end);
            if (row_start != row_end)
                row_span = row_end - row_start;
        }
        // FIXME: Have yet to find the spec for this.
        if (!child_box.computed_values().grid_row_start().is_position() && child_box.computed_values().grid_row_end().is_position() && row_end == 1)
            row_start = 1;

        // If the placement contains two spans, remove the one contributed by the end grid-placement
        // property.
        if (child_box.computed_values().grid_row_start().is_span() && child_box.computed_values().grid_row_end().is_span())
            row_span = child_box.computed_values().grid_row_start().raw_value();

        // FIXME: If the placement contains only a span for a named line, replace it with a span of 1.

        row_start -= 1;
        occupation_grid.maybe_add_row(row_start + row_span);

        int column_start = 0;
        auto column_span = child_box.computed_values().grid_column_start().is_span() ? child_box.computed_values().grid_column_start().raw_value() : 1;
        // https://drafts.csswg.org/css-grid/#auto-placement-algo
        // 8.5. Grid Item Placement Algorithm
        // 3.3. If the largest column span among all the items without a definite column position is larger
        // than the width of the implicit grid, add columns to the end of the implicit grid to accommodate
        // that column span.
        occupation_grid.maybe_add_column(column_span);
        bool found_available_column = false;
        for (int column_index = column_start; column_index < occupation_grid.column_count(); column_index++) {
            if (!occupation_grid.is_occupied(column_index, row_start)) {
                found_available_column = true;
                column_start = column_index;
                break;
            }
        }
        if (!found_available_column) {
            column_start = occupation_grid.column_count();
            occupation_grid.maybe_add_column(column_start + column_span);
        }
        occupation_grid.set_occupied(column_start, column_start + column_span, row_start, row_start + row_span);

        positioned_boxes.append({ child_box, row_start, row_span, column_start, column_span });
        boxes_to_place.remove(i);
        i--;
    }

    // 3. Determine the columns in the implicit grid.
    // NOTE: "implicit grid" here is the same as the occupation_grid

    // 3.1. Start with the columns from the explicit grid.
    // NOTE: Done in step 1.

    // 3.2. Among all the items with a definite column position (explicitly positioned items, items
    // positioned in the previous step, and items not yet positioned but with a definite column) add
    // columns to the beginning and end of the implicit grid as necessary to accommodate those items.
    // NOTE: "Explicitly positioned items" and "items positioned in the previous step" done in step 1
    // and 2, respectively. Adding columns for "items not yet positioned but with a definite column"
    // will be done in step 4.

    // 4. Position the remaining grid items.
    // For each grid item that hasn't been positioned by the previous steps, in order-modified document
    // order:
    auto auto_placement_cursor_x = 0;
    auto auto_placement_cursor_y = 0;
    for (size_t i = 0; i < boxes_to_place.size(); i++) {
        auto const& child_box = boxes_to_place[i];
        // 4.1. For sparse packing:
        // FIXME: no distinction made. See #4.2

        // 4.1.1. If the item has a definite column position:
        if (!is_auto_positioned_column(child_box.computed_values().grid_column_start(), child_box.computed_values().grid_column_end())) {
            int column_start = child_box.computed_values().grid_column_start().raw_value();
            int column_end = child_box.computed_values().grid_column_end().raw_value();

            // https://drafts.csswg.org/css-grid/#line-placement
            // 8.3. Line-based Placement: the grid-row-start, grid-column-start, grid-row-end, and grid-column-end properties

            // https://drafts.csswg.org/css-grid/#grid-placement-slot
            // FIXME: <custom-ident>
            // First attempt to match the grid area’s edge to a named grid area: if there is a grid line whose
            // line name is <custom-ident>-start (for grid-*-start) / <custom-ident>-end (for grid-*-end),
            // contributes the first such line to the grid item’s placement.

            // Note: Named grid areas automatically generate implicitly-assigned line names of this form, so
            // specifying grid-row-start: foo will choose the start edge of that named grid area (unless another
            // line named foo-start was explicitly specified before it).

            // Otherwise, treat this as if the integer 1 had been specified along with the <custom-ident>.

            // https://drafts.csswg.org/css-grid/#grid-placement-int
            // [ <integer [−∞,−1]> | <integer [1,∞]> ] && <custom-ident>?
            // Contributes the Nth grid line to the grid item’s placement. If a negative integer is given, it
            // instead counts in reverse, starting from the end edge of the explicit grid.
            if (column_end < 0)
                column_end = occupation_grid.column_count() + column_end + 2;

            // If a name is given as a <custom-ident>, only lines with that name are counted. If not enough
            // lines with that name exist, all implicit grid lines are assumed to have that name for the purpose
            // of finding this position.

            // An <integer> value of zero makes the declaration invalid.

            // https://drafts.csswg.org/css-grid/#grid-placement-span-int
            // span && [ <integer [1,∞]> || <custom-ident> ]
            // Contributes a grid span to the grid item’s placement such that the corresponding edge of the grid
            // item’s grid area is N lines from its opposite edge in the corresponding direction. For example,
            // grid-column-end: span 2 indicates the second grid line in the endward direction from the
            // grid-column-start line.
            int column_span = 1;
            auto row_span = child_box.computed_values().grid_row_start().is_span() ? child_box.computed_values().grid_row_start().raw_value() : 1;
            if (child_box.computed_values().grid_column_start().is_position() && child_box.computed_values().grid_column_end().is_span())
                column_span = child_box.computed_values().grid_column_end().raw_value();
            if (child_box.computed_values().grid_column_end().is_position() && child_box.computed_values().grid_column_start().is_span()) {
                column_span = child_box.computed_values().grid_column_start().raw_value();
                column_start = column_end - column_span;
                // FIXME: Remove me once have implemented spans overflowing into negative indexes, e.g., grid-column: span 2 / 1
                if (column_start < 0)
                    column_start = 1;
            }
            // FIXME: Have yet to find the spec for this.
            if (!child_box.computed_values().grid_column_start().is_position() && child_box.computed_values().grid_column_end().is_position() && column_end == 1)
                column_start = 1;

            // If a name is given as a <custom-ident>, only lines with that name are counted. If not enough
            // lines with that name exist, all implicit grid lines on the side of the explicit grid
            // corresponding to the search direction are assumed to have that name for the purpose of counting
            // this span.

            // https://drafts.csswg.org/css-grid/#grid-placement-auto
            // auto
            // The property contributes nothing to the grid item’s placement, indicating auto-placement or a
            // default span of one. (See § 8 Placing Grid Items, above.)

            // https://drafts.csswg.org/css-grid/#grid-placement-errors
            // 8.3.1. Grid Placement Conflict Handling
            // If the placement for a grid item contains two lines, and the start line is further end-ward than
            // the end line, swap the two lines. If the start line is equal to the end line, remove the end
            // line.
            if (child_box.computed_values().grid_column_start().is_position() && child_box.computed_values().grid_column_end().is_position()) {
                if (column_start > column_end)
                    swap(column_start, column_end);
                if (column_start != column_end)
                    column_span = column_end - column_start;
            }

            // If the placement contains two spans, remove the one contributed by the end grid-placement
            // property.
            if (child_box.computed_values().grid_column_start().is_span() && child_box.computed_values().grid_column_end().is_span())
                column_span = child_box.computed_values().grid_column_start().raw_value();

            // FIXME: If the placement contains only a span for a named line, replace it with a span of 1.

            column_start -= 1;

            // 4.1.1.1. Set the column position of the cursor to the grid item's column-start line. If this is
            // less than the previous column position of the cursor, increment the row position by 1.
            if (column_start < auto_placement_cursor_x)
                auto_placement_cursor_y++;
            auto_placement_cursor_x = column_start;

            occupation_grid.maybe_add_column(auto_placement_cursor_x + column_span);
            occupation_grid.maybe_add_row(auto_placement_cursor_y + row_span);

            // 4.1.1.2. Increment the cursor's row position until a value is found where the grid item does not
            // overlap any occupied grid cells (creating new rows in the implicit grid as necessary).
            while (true) {
                if (!occupation_grid.is_occupied(column_start, auto_placement_cursor_y)) {
                    break;
                }
                auto_placement_cursor_y++;
                occupation_grid.maybe_add_row(auto_placement_cursor_y + row_span);
            }
            // 4.1.1.3. Set the item's row-start line to the cursor's row position, and set the item's row-end
            // line according to its span from that position.
            occupation_grid.set_occupied(column_start, column_start + column_span, auto_placement_cursor_y, auto_placement_cursor_y + row_span);

            positioned_boxes.append({ child_box, auto_placement_cursor_y, row_span, column_start, column_span });
        }
        // 4.1.2. If the item has an automatic grid position in both axes:
        else {
            // 4.1.2.1. Increment the column position of the auto-placement cursor until either this item's grid
            // area does not overlap any occupied grid cells, or the cursor's column position, plus the item's
            // column span, overflow the number of columns in the implicit grid, as determined earlier in this
            // algorithm.
            auto column_start = 0;
            auto column_span = child_box.computed_values().grid_column_start().is_span() ? child_box.computed_values().grid_column_start().raw_value() : 1;
            // https://drafts.csswg.org/css-grid/#auto-placement-algo
            // 8.5. Grid Item Placement Algorithm
            // 3.3. If the largest column span among all the items without a definite column position is larger
            // than the width of the implicit grid, add columns to the end of the implicit grid to accommodate
            // that column span.
            occupation_grid.maybe_add_column(column_span);
            auto row_start = 0;
            auto row_span = child_box.computed_values().grid_row_start().is_span() ? child_box.computed_values().grid_row_start().raw_value() : 1;
            auto found_unoccupied_area = false;
            for (int row_index = auto_placement_cursor_y; row_index < occupation_grid.row_count(); row_index++) {
                for (int column_index = auto_placement_cursor_x; column_index < occupation_grid.column_count(); column_index++) {
                    if (column_span + column_index <= occupation_grid.column_count()) {
                        auto found_all_available = true;
                        for (int span_index = 0; span_index < column_span; span_index++) {
                            if (occupation_grid.is_occupied(column_index + span_index, row_index))
                                found_all_available = false;
                        }
                        if (found_all_available) {
                            found_unoccupied_area = true;
                            column_start = column_index;
                            row_start = row_index;
                            goto finish;
                        }
                    }
                    auto_placement_cursor_x = 0;
                }
                auto_placement_cursor_x = 0;
                auto_placement_cursor_y++;
            }
        finish:

            // 4.1.2.2. If a non-overlapping position was found in the previous step, set the item's row-start
            // and column-start lines to the cursor's position. Otherwise, increment the auto-placement cursor's
            // row position (creating new rows in the implicit grid as necessary), set its column position to the
            // start-most column line in the implicit grid, and return to the previous step.
            if (!found_unoccupied_area) {
                row_start = occupation_grid.row_count();
                occupation_grid.maybe_add_row(occupation_grid.row_count() + 1);
            }

            occupation_grid.set_occupied(column_start, column_start + column_span, row_start, row_start + row_span);
            positioned_boxes.append({ child_box, row_start, row_span, column_start, column_span });
        }
        boxes_to_place.remove(i);
        i--;

        // FIXME: 4.2. For dense packing:
    }

    for (auto& positioned_box : positioned_boxes) {
        auto& child_box_state = m_state.get_mutable(positioned_box.box);
        if (child_box_state.content_height() > positioned_box.computed_height)
            positioned_box.computed_height = child_box_state.content_height();
        if (auto independent_formatting_context = layout_inside(positioned_box.box, LayoutMode::Normal, available_space))
            independent_formatting_context->parent_context_did_dimension_child_root_box();
        if (child_box_state.content_height() > positioned_box.computed_height)
            positioned_box.computed_height = child_box_state.content_height();
    }

    // https://drafts.csswg.org/css-grid/#overview-sizing
    // 2.3. Sizing the Grid
    // Once the grid items have been placed, the sizes of the grid tracks (rows and columns) are
    // calculated, accounting for the sizes of their contents and/or available space as specified in
    // the grid definition.

    // https://www.w3.org/TR/css-grid-2/#layout-algorithm
    // 12. Grid Sizing
    // This section defines the grid sizing algorithm, which determines the size of all grid tracks and,
    // by extension, the entire grid.

    // Each track has specified minimum and maximum sizing functions (which may be the same). Each
    // sizing function is either:

    // - A fixed sizing function (<length> or resolvable <percentage>).
    // - An intrinsic sizing function (min-content, max-content, auto, fit-content()).
    // - A flexible sizing function (<flex>).

    // The grid sizing algorithm defines how to resolve these sizing constraints into used track sizes.
    for (int x = 0; x < column_repeat_count; ++x) {
        for (auto& meta_grid_track_size : box.computed_values().grid_template_columns().meta_grid_track_sizes())
            m_grid_columns.append({ meta_grid_track_size.min_grid_track_size(), meta_grid_track_size.max_grid_track_size() });
    }
    for (int x = 0; x < row_repeat_count; ++x) {
        for (auto& meta_grid_track_size : box.computed_values().grid_template_rows().meta_grid_track_sizes())
            m_grid_rows.append({ meta_grid_track_size.min_grid_track_size(), meta_grid_track_size.max_grid_track_size() });
    }

    for (int column_index = m_grid_columns.size(); column_index < occupation_grid.column_count(); column_index++)
        m_grid_columns.append({ CSS::GridTrackSize::make_auto(), CSS::GridTrackSize::make_auto() });
    for (int row_index = m_grid_rows.size(); row_index < occupation_grid.row_count(); row_index++)
        m_grid_rows.append({ CSS::GridTrackSize::make_auto(), CSS::GridTrackSize::make_auto() });

    // https://www.w3.org/TR/css-grid-2/#algo-overview
    // 12.1. Grid Sizing Algorithm

    // 1. First, the track sizing algorithm is used to resolve the sizes of the grid columns.
    // In this process, any grid item which is subgridded in the grid container’s inline axis is treated
    // as empty and its grid items (the grandchildren) are treated as direct children of the grid
    // container (their grandparent). This introspection is recursive.

    // Items which are subgridded only in the block axis, and whose grid container size in the inline
    // axis depends on the size of its contents are also introspected: since the size of the item in
    // this dimension can be dependent on the sizing of its subgridded tracks in the other, the size
    // contribution of any such item to this grid’s column sizing (see Resolve Intrinsic Track Sizes) is
    // taken under the provision of having determined its track sizing only up to the same point in the
    // Grid Sizing Algorithm as this itself. E.g. for the first pass through this step, the item will
    // have its tracks sized only through this first step; if a second pass of this step is triggered
    // then the item will have completed a first pass through steps 1-3 as well as the second pass of
    // this step prior to returning its size for consideration in this grid’s column sizing. Again, this
    // introspection is recursive.

    // If calculating the layout of a grid item in this step depends on the available space in the block
    // axis, assume the available space that it would have if any row with a definite max track sizing
    // function had that size and all other rows were infinite. If both the grid container and all
    // tracks have definite sizes, also apply align-content to find the final effective size of any gaps
    // spanned by such items; otherwise ignore the effects of track alignment in this estimation.

    // 2. Next, the track sizing algorithm resolves the sizes of the grid rows.
    // In this process, any grid item which is subgridded in the grid container’s block axis is treated
    // as empty and its grid items (the grandchildren) are treated as direct children of the grid
    // container (their grandparent). This introspection is recursive.

    // As with sizing columns, items which are subgridded only in the inline axis, and whose grid
    // container size in the block axis depends on the size of its contents are also introspected. (As
    // with sizing columns, the size contribution to this grid’s row sizing is taken under the provision
    // of having determined its track sizing only up to this corresponding point in the algorithm; and
    // again, this introspection is recursive.)

    // To find the inline-axis available space for any items whose block-axis size contributions require
    // it, use the grid column sizes calculated in the previous step. If the grid container’s inline
    // size is definite, also apply justify-content to account for the effective column gap sizes.

    // 3. Then, if the min-content contribution of any grid item has changed based on the row sizes and
    // alignment calculated in step 2, re-resolve the sizes of the grid columns with the new min-content
    // and max-content contributions (once only).

    // To find the block-axis available space for any items whose inline-axis size contributions require
    // it, use the grid row sizes calculated in the previous step. If the grid container’s block size is
    // definite, also apply align-content to account for the effective row gap sizes

    // 4. Next, if the min-content contribution of any grid item has changed based on the column sizes and
    // alignment calculated in step 3, re-resolve the sizes of the grid rows with the new min-content
    // and max-content contributions (once only).

    // To find the inline-axis available space for any items whose block-axis size contributions require
    // it, use the grid column sizes calculated in the previous step. If the grid container’s inline
    // size is definite, also apply justify-content to account for the effective column gap sizes.

    // 5. Finally, the grid container is sized using the resulting size of the grid as its content size,
    // and the tracks are aligned within the grid container according to the align-content and
    // justify-content properties.

    // Once the size of each grid area is thus established, the grid items are laid out into their
    // respective containing blocks. The grid area’s width and height are considered definite for this
    // purpose.

    // https://www.w3.org/TR/css-grid-2/#algo-track-sizing
    // 12.3. Track Sizing Algorithm

    // The remainder of this section is the track sizing algorithm, which calculates from the min and
    // max track sizing functions the used track size. Each track has a base size, a <length> which
    // grows throughout the algorithm and which will eventually be the track’s final size, and a growth
    // limit, a <length> which provides a desired maximum size for the base size. There are 5 steps:

    // 1. Initialize Track Sizes
    // 2. Resolve Intrinsic Track Sizes
    // 3. Maximize Tracks
    // 4. Expand Flexible Tracks
    // 5. Expand Stretched auto Tracks

    // https://www.w3.org/TR/css-grid-2/#algo-init
    // 12.4. Initialize Track Sizes

    // Initialize each track’s base size and growth limit.
    for (auto& grid_column : m_grid_columns) {
        // For each track, if the track’s min track sizing function is:
        switch (grid_column.min_track_sizing_function.type()) {
        // - A fixed sizing function
        // Resolve to an absolute length and use that size as the track’s initial base size.
        case CSS::GridTrackSize::Type::Length:
            if (!grid_column.min_track_sizing_function.length().is_auto())
                grid_column.base_size = grid_column.min_track_sizing_function.length().to_px(box);
            break;
        case CSS::GridTrackSize::Type::Percentage:
            grid_column.base_size = grid_column.min_track_sizing_function.percentage().as_fraction() * box_state.content_width();
            break;
        // - An intrinsic sizing function
        // Use an initial base size of zero.
        case CSS::GridTrackSize::Type::FlexibleLength:
            break;
        default:
            VERIFY_NOT_REACHED();
        }

        // For each track, if the track’s max track sizing function is:
        switch (grid_column.max_track_sizing_function.type()) {
        // - A fixed sizing function
        // Resolve to an absolute length and use that size as the track’s initial growth limit.
        case CSS::GridTrackSize::Type::Length:
            if (!grid_column.max_track_sizing_function.length().is_auto())
                grid_column.growth_limit = grid_column.max_track_sizing_function.length().to_px(box);
            else
                // - An intrinsic sizing function
                // Use an initial growth limit of infinity.
                grid_column.growth_limit = -1;
            break;
        case CSS::GridTrackSize::Type::Percentage:
            grid_column.growth_limit = grid_column.max_track_sizing_function.percentage().as_fraction() * box_state.content_width();
            break;
        // - A flexible sizing function
        // Use an initial growth limit of infinity.
        case CSS::GridTrackSize::Type::FlexibleLength:
            grid_column.growth_limit = -1;
            break;
        default:
            VERIFY_NOT_REACHED();
        }

        // In all cases, if the growth limit is less than the base size, increase the growth limit to match
        // the base size.
        if (grid_column.growth_limit != -1 && grid_column.growth_limit < grid_column.base_size)
            grid_column.growth_limit = grid_column.base_size;
    }

    // Initialize each track’s base size and growth limit.
    for (auto& grid_row : m_grid_rows) {
        // For each track, if the track’s min track sizing function is:
        switch (grid_row.min_track_sizing_function.type()) {
        // - A fixed sizing function
        // Resolve to an absolute length and use that size as the track’s initial base size.
        case CSS::GridTrackSize::Type::Length:
            if (!grid_row.min_track_sizing_function.length().is_auto())
                grid_row.base_size = grid_row.min_track_sizing_function.length().to_px(box);
            break;
        case CSS::GridTrackSize::Type::Percentage:
            grid_row.base_size = grid_row.min_track_sizing_function.percentage().as_fraction() * box_state.content_height();
            break;
        // - An intrinsic sizing function
        // Use an initial base size of zero.
        case CSS::GridTrackSize::Type::FlexibleLength:
            break;
        default:
            VERIFY_NOT_REACHED();
        }

        // For each track, if the track’s max track sizing function is:
        switch (grid_row.max_track_sizing_function.type()) {
        // - A fixed sizing function
        // Resolve to an absolute length and use that size as the track’s initial growth limit.
        case CSS::GridTrackSize::Type::Length:
            if (!grid_row.max_track_sizing_function.length().is_auto())
                grid_row.growth_limit = grid_row.max_track_sizing_function.length().to_px(box);
            else
                // - An intrinsic sizing function
                // Use an initial growth limit of infinity.
                grid_row.growth_limit = -1;
            break;
        case CSS::GridTrackSize::Type::Percentage:
            grid_row.growth_limit = grid_row.max_track_sizing_function.percentage().as_fraction() * box_state.content_height();
            break;
        // - A flexible sizing function
        // Use an initial growth limit of infinity.
        case CSS::GridTrackSize::Type::FlexibleLength:
            grid_row.growth_limit = -1;
            break;
        default:
            VERIFY_NOT_REACHED();
        }

        // In all cases, if the growth limit is less than the base size, increase the growth limit to match
        // the base size.
        if (grid_row.growth_limit != -1 && grid_row.growth_limit < grid_row.base_size)
            grid_row.growth_limit = grid_row.base_size;
    }

    // https://www.w3.org/TR/css-grid-2/#algo-content
    // 12.5. Resolve Intrinsic Track Sizes
    // This step resolves intrinsic track sizing functions to absolute lengths. First it resolves those
    // sizes based on items that are contained wholly within a single track. Then it gradually adds in
    // the space requirements of items that span multiple tracks, evenly distributing the extra space
    // across those tracks insofar as possible.

    // FIXME: 1. Shim baseline-aligned items so their intrinsic size contributions reflect their baseline
    // alignment. For the items in each baseline-sharing group, add a “shim” (effectively, additional
    // margin) on the start/end side (for first/last-baseline alignment) of each item so that, when
    // start/end-aligned together their baselines align as specified.

    // Consider these “shims” as part of the items’ intrinsic size contribution for the purpose of track
    // sizing, below. If an item uses multiple intrinsic size contributions, it can have different shims
    // for each one.

    // 2. Size tracks to fit non-spanning items: For each track with an intrinsic track sizing function and
    // not a flexible sizing function, consider the items in it with a span of 1:
    int index = 0;
    for (auto& grid_column : m_grid_columns) {
        if (!grid_column.min_track_sizing_function.is_intrinsic_track_sizing()) {
            ++index;
            continue;
        }

        Vector<Box const&> boxes_of_column;
        for (auto& positioned_box : positioned_boxes) {
            if (positioned_box.column == index && positioned_box.column_span == 1)
                boxes_of_column.append(positioned_box.box);
        }

        // - For min-content minimums:
        // If the track has a min-content min track sizing function, set its base size to the maximum of the
        // items’ min-content contributions, floored at zero.
        // FIXME: Not implemented yet min-content.

        // - For max-content minimums:
        // If the track has a max-content min track sizing function, set its base size to the maximum of the
        // items’ max-content contributions, floored at zero.
        // FIXME: Not implemented yet max-content.

        // - For auto minimums:
        // If the track has an auto min track sizing function and the grid container is being sized under a
        // min-/max-content constraint, set the track’s base size to the maximum of its items’ limited
        // min-/max-content contributions (respectively), floored at zero. The limited min-/max-content
        // contribution of an item is (for this purpose) its min-/max-content contribution (accordingly),
        // limited by the max track sizing function (which could be the argument to a fit-content() track
        // sizing function) if that is fixed and ultimately floored by its minimum contribution (defined
        // below).
        // FIXME: Not implemented yet min-/max-content.

        // Otherwise, set the track’s base size to the maximum of its items’ minimum contributions, floored
        // at zero. The minimum contribution of an item is the smallest outer size it can have.
        // Specifically, if the item’s computed preferred size behaves as auto or depends on the size of its
        // containing block in the relevant axis, its minimum contribution is the outer size that would
        // result from assuming the item’s used minimum size as its preferred size; else the item’s minimum
        // contribution is its min-content contribution. Because the minimum contribution often depends on
        // the size of the item’s content, it is considered a type of intrinsic size contribution.
        float grid_column_width = 0;
        for (auto& box_of_column : boxes_of_column)
            grid_column_width = max(grid_column_width, calculate_min_content_width(box_of_column));
        grid_column.base_size = grid_column_width;

        // - For min-content maximums:
        // If the track has a min-content max track sizing function, set its growth limit to the maximum of
        // the items’ min-content contributions.
        // FIXME: Not implemented yet min-content maximums.

        // - For max-content maximums:
        // If the track has a max-content max track sizing function, set its growth limit to the maximum of
        // the items’ max-content contributions. For fit-content() maximums, furthermore clamp this growth
        // limit by the fit-content() argument.
        // FIXME: Not implemented yet max-content maximums.

        // In all cases, if a track’s growth limit is now less than its base size, increase the growth limit
        // to match the base size.
        if (grid_column.growth_limit != -1 && grid_column.growth_limit < grid_column.base_size)
            grid_column.growth_limit = grid_column.base_size;

        ++index;
    }

    index = 0;
    for (auto& grid_row : m_grid_rows) {
        if (!grid_row.min_track_sizing_function.is_intrinsic_track_sizing()) {
            ++index;
            continue;
        }

        Vector<PositionedBox&> positioned_boxes_of_row;
        for (auto& positioned_box : positioned_boxes) {
            if (positioned_box.row == index && positioned_box.row_span == 1)
                positioned_boxes_of_row.append(positioned_box);
        }

        // - For min-content minimums:
        // If the track has a min-content min track sizing function, set its base size to the maximum of the
        // items’ min-content contributions, floored at zero.
        // FIXME: Not implemented yet min-content.

        // - For max-content minimums:
        // If the track has a max-content min track sizing function, set its base size to the maximum of the
        // items’ max-content contributions, floored at zero.
        // FIXME: Not implemented yet max-content.

        // - For auto minimums:
        // If the track has an auto min track sizing function and the grid container is being sized under a
        // min-/max-content constraint, set the track’s base size to the maximum of its items’ limited
        // min-/max-content contributions (respectively), floored at zero. The limited min-/max-content
        // contribution of an item is (for this purpose) its min-/max-content contribution (accordingly),
        // limited by the max track sizing function (which could be the argument to a fit-content() track
        // sizing function) if that is fixed and ultimately floored by its minimum contribution (defined
        // below).
        // FIXME: Not implemented yet min-/max-content.

        // Otherwise, set the track’s base size to the maximum of its items’ minimum contributions, floored
        // at zero. The minimum contribution of an item is the smallest outer size it can have.
        // Specifically, if the item’s computed preferred size behaves as auto or depends on the size of its
        // containing block in the relevant axis, its minimum contribution is the outer size that would
        // result from assuming the item’s used minimum size as its preferred size; else the item’s minimum
        // contribution is its min-content contribution. Because the minimum contribution often depends on
        // the size of the item’s content, it is considered a type of intrinsic size contribution.
        float grid_row_height = 0;
        for (auto& positioned_box : positioned_boxes_of_row)
            grid_row_height = max(grid_row_height, positioned_box.computed_height);
        grid_row.base_size = grid_row_height;

        // - For min-content maximums:
        // If the track has a min-content max track sizing function, set its growth limit to the maximum of
        // the items’ min-content contributions.
        // FIXME: Not implemented yet min-content maximums.

        // - For max-content maximums:
        // If the track has a max-content max track sizing function, set its growth limit to the maximum of
        // the items’ max-content contributions. For fit-content() maximums, furthermore clamp this growth
        // limit by the fit-content() argument.
        // FIXME: Not implemented yet max-content maximums.

        // In all cases, if a track’s growth limit is now less than its base size, increase the growth limit
        // to match the base size.
        if (grid_row.growth_limit != -1 && grid_row.growth_limit < grid_row.base_size)
            grid_row.growth_limit = grid_row.base_size;
        ++index;
    }

    // https://www.w3.org/TR/css-grid-2/#auto-repeat
    // The auto-fit keyword behaves the same as auto-fill, except that after grid item placement any
    // empty repeated tracks are collapsed. An empty track is one with no in-flow grid items placed into
    // or spanning across it. (This can result in all tracks being collapsed, if they’re all empty.)
    if (box.computed_values().grid_template_columns().is_auto_fit()) {
        auto idx = 0;
        for (auto& grid_column : m_grid_columns) {
            // A collapsed track is treated as having a fixed track sizing function of 0px, and the gutters on
            // either side of it—including any space allotted through distributed alignment—collapse.
            if (!occupation_grid.is_occupied(idx, 0)) {
                grid_column.base_size = 0;
                grid_column.growth_limit = 0;
            }
            idx++;
        }
    }

    // 3. Increase sizes to accommodate spanning items crossing content-sized tracks: Next, consider the
    // items with a span of 2 that do not span a track with a flexible sizing function.
    // FIXME: Content-sized tracks not implemented (min-content, etc.)

    // 3.1. For intrinsic minimums: First increase the base size of tracks with an intrinsic min track sizing
    // function by distributing extra space as needed to accommodate these items’ minimum contributions.

    // If the grid container is being sized under a min- or max-content constraint, use the items’
    // limited min-content contributions in place of their minimum contributions here. (For an item
    // spanning multiple tracks, the upper limit used to calculate its limited min-/max-content
    // contribution is the sum of the fixed max track sizing functions of any tracks it spans, and is
    // applied if it only spans such tracks.)

    // 3.2. For content-based minimums: Next continue to increase the base size of tracks with a min track
    // sizing function of min-content or max-content by distributing extra space as needed to account
    // for these items' min-content contributions.

    // 3.3. For max-content minimums: Next, if the grid container is being sized under a max-content
    // constraint, continue to increase the base size of tracks with a min track sizing function of auto
    // or max-content by distributing extra space as needed to account for these items' limited
    // max-content contributions.

    // In all cases, continue to increase the base size of tracks with a min track sizing function of
    // max-content by distributing extra space as needed to account for these items' max-content
    // contributions.

    // 3.4. If at this point any track’s growth limit is now less than its base size, increase its growth
    // limit to match its base size.

    // 3.5. For intrinsic maximums: Next increase the growth limit of tracks with an intrinsic max track
    // sizing function by distributing extra space as needed to account for these items' min-content
    // contributions. Mark any tracks whose growth limit changed from infinite to finite in this step as
    // infinitely growable for the next step.

    // 3.6. For max-content maximums: Lastly continue to increase the growth limit of tracks with a max track
    // sizing function of max-content by distributing extra space as needed to account for these items'
    // max-content contributions. However, limit the growth of any fit-content() tracks by their
    // fit-content() argument.

    // Repeat incrementally for items with greater spans until all items have been considered.

    // FIXME: 4. Increase sizes to accommodate spanning items crossing flexible tracks: Next, repeat the previous
    // step instead considering (together, rather than grouped by span size) all items that do span a
    // track with a flexible sizing function while

    // - distributing space only to flexible tracks (i.e. treating all other tracks as having a fixed
    // sizing function)

    // - if the sum of the flexible sizing functions of all flexible tracks spanned by the item is greater
    // than zero, distributing space to such tracks according to the ratios of their flexible sizing
    // functions rather than distributing space equally

    // FIXME: 5. If any track still has an infinite growth limit (because, for example, it had no items placed in
    // it or it is a flexible track), set its growth limit to its base size.

    // https://www.w3.org/TR/css-grid-2/#extra-space
    // 12.5.1. Distributing Extra Space Across Spanned Tracks
    // To distribute extra space by increasing the affected sizes of a set of tracks as required by a
    // set of intrinsic size contributions,
    float sum_of_track_sizes = 0;
    for (auto& it : m_grid_columns)
        sum_of_track_sizes += it.base_size;

    // 1. Maintain separately for each affected base size or growth limit a planned increase, initially
    // set to 0. (This prevents the size increases from becoming order-dependent.)

    // 2. For each considered item,

    // 2.1. Find the space to distribute: Subtract the corresponding size (base size or growth limit) of
    // every spanned track from the item’s size contribution to find the item’s remaining size
    // contribution. (For infinite growth limits, substitute the track’s base size.) This is the space
    // to distribute. Floor it at zero.

    // For base sizes, the limit is its growth limit. For growth limits, the limit is infinity if it is
    // marked as infinitely growable, and equal to the growth limit otherwise. If the affected size was
    // a growth limit and the track is not marked infinitely growable, then each item-incurred increase
    // will be zero.
    // extra-space = max(0, size-contribution - ∑track-sizes)
    for (auto& grid_column : m_grid_columns)
        grid_column.space_to_distribute = max(0, (grid_column.growth_limit == -1 ? grid_column.base_size : grid_column.growth_limit) - grid_column.base_size);

    auto remaining_free_space = box_state.content_width() - sum_of_track_sizes;
    // 2.2. Distribute space up to limits: Find the item-incurred increase for each spanned track with an
    // affected size by: distributing the space equally among such tracks, freezing a track’s
    // item-incurred increase as its affected size + item-incurred increase reaches its limit (and
    // continuing to grow the unfrozen tracks as needed).
    auto count_of_unfrozen_tracks = 0;
    for (auto& grid_column : m_grid_columns) {
        if (grid_column.space_to_distribute > 0)
            count_of_unfrozen_tracks++;
    }
    while (remaining_free_space > 0) {
        if (count_of_unfrozen_tracks == 0)
            break;
        auto free_space_to_distribute_per_track = remaining_free_space / count_of_unfrozen_tracks;

        for (auto& grid_column : m_grid_columns) {
            if (grid_column.space_to_distribute == 0)
                continue;
            // 2.4. For each affected track, if the track’s item-incurred increase is larger than the track’s planned
            // increase set the track’s planned increase to that value.
            if (grid_column.space_to_distribute <= free_space_to_distribute_per_track) {
                grid_column.planned_increase += grid_column.space_to_distribute;
                remaining_free_space -= grid_column.space_to_distribute;
                grid_column.space_to_distribute = 0;
            } else {
                grid_column.space_to_distribute -= free_space_to_distribute_per_track;
                grid_column.planned_increase += free_space_to_distribute_per_track;
                remaining_free_space -= free_space_to_distribute_per_track;
            }
        }

        count_of_unfrozen_tracks = 0;
        for (auto& grid_column : m_grid_columns) {
            if (grid_column.space_to_distribute > 0)
                count_of_unfrozen_tracks++;
        }
        if (remaining_free_space == 0)
            break;
    }

    // 2.3. Distribute space beyond limits: If space remains after all tracks are frozen, unfreeze and
    // continue to distribute space to the item-incurred increase of…

    // - when accommodating minimum contributions or accommodating min-content contributions: any affected
    // track that happens to also have an intrinsic max track sizing function; if there are no such
    // tracks, then all affected tracks.

    // - when accommodating max-content contributions: any affected track that happens to also have a
    // max-content max track sizing function; if there are no such tracks, then all affected tracks.

    // - when handling any intrinsic growth limit: all affected tracks.

    // For this purpose, the max track sizing function of a fit-content() track is treated as
    // max-content until it reaches the limit specified as the fit-content() argument, after which it is
    // treated as having a fixed sizing function of that argument.

    // This step prioritizes the distribution of space for accommodating space required by the
    // tracks’ min track sizing functions beyond their current growth limits based on the types of their
    // max track sizing functions.

    // 3. Update the tracks' affected sizes by adding in the planned increase so that the next round of
    // space distribution will account for the increase. (If the affected size is an infinite growth
    // limit, set it to the track’s base size plus the planned increase.)
    for (auto& grid_column : m_grid_columns)
        grid_column.base_size += grid_column.planned_increase;
    // FIXME: Do for rows.

    // https://www.w3.org/TR/css-grid-2/#algo-grow-tracks
    // 12.6. Maximize Tracks

    // If the free space is positive, distribute it equally to the base sizes of all tracks, freezing
    // tracks as they reach their growth limits (and continuing to grow the unfrozen tracks as needed).
    auto free_space = get_free_space_x(box);
    while (free_space > 0) {
        auto free_space_to_distribute_per_track = free_space / m_grid_columns.size();
        for (auto& grid_column : m_grid_columns) {
            if (grid_column.growth_limit != -1)
                grid_column.base_size = min(grid_column.growth_limit, grid_column.base_size + free_space_to_distribute_per_track);
            else
                grid_column.base_size = grid_column.base_size + free_space_to_distribute_per_track;
        }
        if (get_free_space_x(box) == free_space)
            break;
        free_space = get_free_space_x(box);
    }

    free_space = get_free_space_y(box);
    while (free_space > 0) {
        auto free_space_to_distribute_per_track = free_space / m_grid_rows.size();
        for (auto& grid_row : m_grid_rows)
            grid_row.base_size = min(grid_row.growth_limit, grid_row.base_size + free_space_to_distribute_per_track);
        if (get_free_space_y(box) == free_space)
            break;
        free_space = get_free_space_y(box);
    }
    if (free_space == -1) {
        for (auto& grid_row : m_grid_rows) {
            if (grid_row.growth_limit != -1)
                grid_row.base_size = grid_row.growth_limit;
        }
    }

    // For the purpose of this step: if sizing the grid container under a max-content constraint, the
    // free space is infinite; if sizing under a min-content constraint, the free space is zero.

    // If this would cause the grid to be larger than the grid container’s inner size as limited by its
    // max-width/height, then redo this step, treating the available grid space as equal to the grid
    // container’s inner size when it’s sized to its max-width/height.

    // https://drafts.csswg.org/css-grid/#algo-flex-tracks
    // 12.7. Expand Flexible Tracks
    // This step sizes flexible tracks using the largest value it can assign to an fr without exceeding
    // the available space.

    // First, find the grid’s used flex fraction:
    auto column_flex_factor_sum = 0;
    for (auto& grid_column : m_grid_columns) {
        if (grid_column.min_track_sizing_function.is_flexible_length())
            column_flex_factor_sum++;
    }
    // See 12.7.1.
    // Let flex factor sum be the sum of the flex factors of the flexible tracks. If this value is less
    // than 1, set it to 1 instead.
    if (column_flex_factor_sum < 1)
        column_flex_factor_sum = 1;

    // See 12.7.1.
    float sized_column_widths = 0;
    for (auto& grid_column : m_grid_columns) {
        if (!grid_column.min_track_sizing_function.is_flexible_length())
            sized_column_widths += grid_column.base_size;
    }
    // Let leftover space be the space to fill minus the base sizes of the non-flexible grid tracks.
    double free_horizontal_space = box_state.content_width() - sized_column_widths;

    // If the free space is zero or if sizing the grid container under a min-content constraint:
    // The used flex fraction is zero.
    // FIXME: Add min-content constraint check.

    // Otherwise, if the free space is a definite length:
    // The used flex fraction is the result of finding the size of an fr using all of the grid tracks
    // and a space to fill of the available grid space.
    if (free_horizontal_space > 0) {
        for (auto& grid_column : m_grid_columns) {
            if (grid_column.min_track_sizing_function.is_flexible_length()) {
                // See 12.7.1.
                // Let the hypothetical fr size be the leftover space divided by the flex factor sum.
                auto hypothetical_fr_size = static_cast<double>(1.0 / column_flex_factor_sum) * free_horizontal_space;
                // For each flexible track, if the product of the used flex fraction and the track’s flex factor is
                // greater than the track’s base size, set its base size to that product.
                grid_column.base_size = max(grid_column.base_size, hypothetical_fr_size);
            }
        }
    }

    // First, find the grid’s used flex fraction:
    auto row_flex_factor_sum = 0;
    for (auto& grid_row : m_grid_rows) {
        if (grid_row.min_track_sizing_function.is_flexible_length())
            row_flex_factor_sum++;
    }
    // See 12.7.1.
    // Let flex factor sum be the sum of the flex factors of the flexible tracks. If this value is less
    // than 1, set it to 1 instead.
    if (row_flex_factor_sum < 1)
        row_flex_factor_sum = 1;

    // See 12.7.1.
    float sized_row_heights = 0;
    for (auto& grid_row : m_grid_rows) {
        if (!grid_row.min_track_sizing_function.is_flexible_length())
            sized_row_heights += grid_row.base_size;
    }
    // Let leftover space be the space to fill minus the base sizes of the non-flexible grid tracks.
    double free_vertical_space = box_state.content_height() - sized_row_heights;

    // If the free space is zero or if sizing the grid container under a min-content constraint:
    // The used flex fraction is zero.
    // FIXME: Add min-content constraint check.

    // Otherwise, if the free space is a definite length:
    // The used flex fraction is the result of finding the size of an fr using all of the grid tracks
    // and a space to fill of the available grid space.
    if (free_vertical_space > 0) {
        for (auto& grid_row : m_grid_rows) {
            if (grid_row.min_track_sizing_function.is_flexible_length()) {
                // See 12.7.1.
                // Let the hypothetical fr size be the leftover space divided by the flex factor sum.
                auto hypothetical_fr_size = static_cast<double>(1.0 / row_flex_factor_sum) * free_vertical_space;
                // For each flexible track, if the product of the used flex fraction and the track’s flex factor is
                // greater than the track’s base size, set its base size to that product.
                grid_row.base_size = max(grid_row.base_size, hypothetical_fr_size);
            }
        }
    }

    // Otherwise, if the free space is an indefinite length:
    // FIXME: No tracks will have indefinite length as per current implementation.

    // The used flex fraction is the maximum of:
    // For each flexible track, if the flexible track’s flex factor is greater than one, the result of
    // dividing the track’s base size by its flex factor; otherwise, the track’s base size.

    // For each grid item that crosses a flexible track, the result of finding the size of an fr using
    // all the grid tracks that the item crosses and a space to fill of the item’s max-content
    // contribution.

    // If using this flex fraction would cause the grid to be smaller than the grid container’s
    // min-width/height (or larger than the grid container’s max-width/height), then redo this step,
    // treating the free space as definite and the available grid space as equal to the grid container’s
    // inner size when it’s sized to its min-width/height (max-width/height).

    // For each flexible track, if the product of the used flex fraction and the track’s flex factor is
    // greater than the track’s base size, set its base size to that product.

    // https://drafts.csswg.org/css-grid/#algo-find-fr-size
    // 12.7.1. Find the Size of an fr

    // This algorithm finds the largest size that an fr unit can be without exceeding the target size.
    // It must be called with a set of grid tracks and some quantity of space to fill.

    // 1. Let leftover space be the space to fill minus the base sizes of the non-flexible grid tracks.

    // 2. Let flex factor sum be the sum of the flex factors of the flexible tracks. If this value is less
    // than 1, set it to 1 instead.

    // 3. Let the hypothetical fr size be the leftover space divided by the flex factor sum.

    // FIXME: 4. If the product of the hypothetical fr size and a flexible track’s flex factor is less than the
    // track’s base size, restart this algorithm treating all such tracks as inflexible.

    // 5. Return the hypothetical fr size.

    // https://drafts.csswg.org/css-grid/#algo-stretch
    // 12.8. Stretch auto Tracks

    // When the content-distribution property of the grid container is normal or stretch in this axis,
    // this step expands tracks that have an auto max track sizing function by dividing any remaining
    // positive, definite free space equally amongst them. If the free space is indefinite, but the grid
    // container has a definite min-width/height, use that size to calculate the free space for this
    // step instead.
    float used_horizontal_space = 0;
    for (auto& grid_column : m_grid_columns) {
        if (!(grid_column.max_track_sizing_function.is_length() && grid_column.max_track_sizing_function.length().is_auto()))
            used_horizontal_space += grid_column.base_size;
    }

    float remaining_horizontal_space = box_state.content_width() - used_horizontal_space;
    auto count_of_auto_max_column_tracks = 0;
    for (auto& grid_column : m_grid_columns) {
        if (grid_column.max_track_sizing_function.is_length() && grid_column.max_track_sizing_function.length().is_auto())
            count_of_auto_max_column_tracks++;
    }
    for (auto& grid_column : m_grid_columns) {
        if (grid_column.max_track_sizing_function.is_length() && grid_column.max_track_sizing_function.length().is_auto())
            grid_column.base_size = max(grid_column.base_size, remaining_horizontal_space / count_of_auto_max_column_tracks);
    }

    float used_vertical_space = 0;
    for (auto& grid_row : m_grid_rows) {
        if (!(grid_row.max_track_sizing_function.is_length() && grid_row.max_track_sizing_function.length().is_auto()))
            used_vertical_space += grid_row.base_size;
    }

    float remaining_vertical_space = box_state.content_height() - used_vertical_space;
    auto count_of_auto_max_row_tracks = 0;
    for (auto& grid_row : m_grid_rows) {
        if (grid_row.max_track_sizing_function.is_length() && grid_row.max_track_sizing_function.length().is_auto())
            count_of_auto_max_row_tracks++;
    }
    for (auto& grid_row : m_grid_rows) {
        if (grid_row.max_track_sizing_function.is_length() && grid_row.max_track_sizing_function.length().is_auto())
            grid_row.base_size = max(grid_row.base_size, remaining_vertical_space / count_of_auto_max_row_tracks);
    }

    auto layout_box = [&](int row_start, int row_end, int column_start, int column_end, Box const& child_box) -> void {
        auto& child_box_state = m_state.get_mutable(child_box);
        float x_start = 0;
        float x_end = 0;
        float y_start = 0;
        float y_end = 0;
        for (int i = 0; i < column_start; i++)
            x_start += m_grid_columns[i].base_size;
        for (int i = 0; i < column_end; i++)
            x_end += m_grid_columns[i].base_size;
        for (int i = 0; i < row_start; i++)
            y_start += m_grid_rows[i].base_size;
        for (int i = 0; i < row_end; i++)
            y_end += m_grid_rows[i].base_size;
        child_box_state.set_content_width(x_end - x_start);
        child_box_state.set_content_height(y_end - y_start);
        child_box_state.offset = { x_start, y_start };
    };

    for (auto& positioned_box : positioned_boxes) {
        auto resolved_span = positioned_box.row + positioned_box.row_span > static_cast<int>(m_grid_rows.size()) ? static_cast<int>(m_grid_rows.size()) - positioned_box.row : positioned_box.row_span;
        layout_box(positioned_box.row, positioned_box.row + resolved_span, positioned_box.column, positioned_box.column + positioned_box.column_span, positioned_box.box);
    }

    float total_y = 0;
    for (auto& grid_row : m_grid_rows)
        total_y += grid_row.base_size;
    m_automatic_content_height = total_y;
}

float GridFormattingContext::automatic_content_height() const
{
    return m_automatic_content_height;
}

bool GridFormattingContext::is_auto_positioned_row(CSS::GridTrackPlacement const& grid_row_start, CSS::GridTrackPlacement const& grid_row_end) const
{
    return is_auto_positioned_track(grid_row_start, grid_row_end);
}

bool GridFormattingContext::is_auto_positioned_column(CSS::GridTrackPlacement const& grid_column_start, CSS::GridTrackPlacement const& grid_column_end) const
{
    return is_auto_positioned_track(grid_column_start, grid_column_end);
}

bool GridFormattingContext::is_auto_positioned_track(CSS::GridTrackPlacement const& grid_track_start, CSS::GridTrackPlacement const& grid_track_end) const
{
    return grid_track_start.is_auto_positioned() && grid_track_end.is_auto_positioned();
}

float GridFormattingContext::get_free_space_x(Box const& box)
{
    // https://www.w3.org/TR/css-grid-2/#algo-terms
    // free space: Equal to the available grid space minus the sum of the base sizes of all the grid
    // tracks (including gutters), floored at zero. If available grid space is indefinite, the free
    // space is indefinite as well.
    // FIXME: do indefinite space
    auto sum_base_sizes = 0;
    for (auto& grid_column : m_grid_columns)
        sum_base_sizes += grid_column.base_size;
    auto& box_state = m_state.get_mutable(box);
    return max(0, box_state.content_width() - sum_base_sizes);
}

float GridFormattingContext::get_free_space_y(Box const& box)
{
    // https://www.w3.org/TR/css-grid-2/#algo-terms
    // free space: Equal to the available grid space minus the sum of the base sizes of all the grid
    // tracks (including gutters), floored at zero. If available grid space is indefinite, the free
    // space is indefinite as well.
    auto sum_base_sizes = 0;
    for (auto& grid_row : m_grid_rows)
        sum_base_sizes += grid_row.base_size;
    auto& box_state = m_state.get_mutable(box);
    if (box_state.has_definite_height())
        return max(0, absolute_content_rect(box, m_state).height() - sum_base_sizes);
    return -1;
}

OccupationGrid::OccupationGrid(int column_count, int row_count)
{
    Vector<bool> occupation_grid_row;
    for (int column_index = 0; column_index < max(column_count, 1); column_index++)
        occupation_grid_row.append(false);
    for (int row_index = 0; row_index < max(row_count, 1); row_index++)
        m_occupation_grid.append(occupation_grid_row);
}

void OccupationGrid::maybe_add_column(int needed_number_of_columns)
{
    if (needed_number_of_columns <= column_count())
        return;
    auto column_count_before_modification = column_count();
    for (auto& occupation_grid_row : m_occupation_grid)
        for (int idx = 0; idx < needed_number_of_columns - column_count_before_modification; idx++)
            occupation_grid_row.append(false);
}

void OccupationGrid::maybe_add_row(int needed_number_of_rows)
{
    if (needed_number_of_rows <= row_count())
        return;

    Vector<bool> new_occupation_grid_row;
    for (int idx = 0; idx < column_count(); idx++)
        new_occupation_grid_row.append(false);

    for (int idx = 0; idx < needed_number_of_rows - row_count(); idx++)
        m_occupation_grid.append(new_occupation_grid_row);
}

void OccupationGrid::set_occupied(int column_start, int column_end, int row_start, int row_end)
{
    for (int row_index = 0; row_index < row_count(); row_index++) {
        if (row_index >= row_start && row_index < row_end) {
            for (int column_index = 0; column_index < column_count(); column_index++) {
                if (column_index >= column_start && column_index < column_end)
                    set_occupied(column_index, row_index);
            }
        }
    }
}

void OccupationGrid::set_occupied(int column_index, int row_index)
{
    m_occupation_grid[row_index][column_index] = true;
}

bool OccupationGrid::is_occupied(int column_index, int row_index)
{
    return m_occupation_grid[row_index][column_index];
}

}
