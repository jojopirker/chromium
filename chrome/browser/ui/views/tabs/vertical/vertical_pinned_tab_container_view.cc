// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_pinned_tab_container_view.h"

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/text_constants.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/delegating_layout_manager.h"
#include "ui/views/layout/proposed_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {
constexpr int kTabVerticalPadding = 4;
constexpr char16_t kPinnedPlaceholderText[] = u"Sample pinned tab";
}  // namespace

VerticalPinnedTabContainerView::VerticalPinnedTabContainerView(
    TabCollectionNode* collection_node)
    : collection_node_(collection_node) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));
  placeholder_label_ =
      AddChildView(std::make_unique<views::Label>(kPinnedPlaceholderText));
  placeholder_label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  placeholder_label_->SetBackground(
      views::CreateSolidBackground(ui::kColorFrameActive));
  placeholder_label_->SetEnabledColor(ui::kColorLabelForeground);
  placeholder_label_->SetAutoColorReadabilityEnabled(false);
  placeholder_label_->SetSubpixelRenderingEnabled(false);
  placeholder_label_->SetPaintToLayer();
  placeholder_label_->layer()->SetFillsBoundsOpaquely(true);

  node_destroyed_subscription_ = collection_node_->RegisterWillDestroyCallback(
      base::BindOnce(&VerticalPinnedTabContainerView::ResetCollectionNode,
                     base::Unretained(this)));
}

VerticalPinnedTabContainerView::~VerticalPinnedTabContainerView() = default;

views::ProposedLayout VerticalPinnedTabContainerView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  int total_width = 0;
  int total_height = 0;

  const auto children = collection_node_->GetDirectChildren();
  const bool has_children = !children.empty();

  if (!has_children) {
    const int available_width =
        size_bounds.width().is_bounded()
            ? size_bounds.width().value()
            : placeholder_label_->GetPreferredSize().width();
    const int placeholder_height =
        placeholder_label_->GetHeightForWidth(available_width);
    gfx::Rect placeholder_bounds(
        0, 0, available_width,
        placeholder_height > 0 ? placeholder_height
                               : placeholder_label_->GetPreferredSize().height());
    layouts.child_layouts.emplace_back();
    auto& placeholder_layout = layouts.child_layouts.back();
    placeholder_layout.child_view = placeholder_label_;
    placeholder_layout.visible = true;
    placeholder_layout.bounds = placeholder_bounds;
    layouts.host_size = placeholder_bounds.size();
    return layouts;
  }

  int x = 0;
  int y = 0;
  int children_on_row = children.size();

  // Child width will be uniform and match the largest child's width.
  int child_width = 0;
  for (auto* child : children) {
    // TODO(corising): look into caching this value and only recomputing if the
    // children change.
    child_width = std::max(child_width, child->GetPreferredSize().width());
  }
  // If the width is bounded, calculate how many children can fit on a row.
  // Since all children are allocated the same width this will be the same for
  // every row.
  if (size_bounds.width().is_bounded()) {
    int available_width =
        size_bounds.width().value() -
        GetLayoutConstant(VERTICAL_TAB_STRIP_HORIZONTAL_PADDING);

    if (available_width > 0) {
      children_on_row = std::floor((available_width - child_width) /
                                   (child_width + kTabVerticalPadding)) +
                        1;

      // Allocate extra space to the tabs.
      available_width -= (children_on_row * child_width) +
                         (kTabVerticalPadding * (children_on_row - 1));
      child_width += std::floor(available_width / children_on_row);
    } else {
      children_on_row = 1;
    }
  }

  int row_index = 0;
  for (auto* child : children) {
    gfx::Rect bounds = gfx::Rect(child->GetPreferredSize());
    bounds.set_width(child_width);
    if (row_index != 0) {
      x += kTabVerticalPadding;
    }
    bounds.set_x(x);
    bounds.set_y(y);
    x += bounds.width();
    total_width = std::max(total_width, x);
    total_height = std::max(total_height, (y + bounds.height()));
    layouts.child_layouts.emplace_back(child, child->GetVisible(), bounds);
    row_index++;
    if (row_index >= children_on_row) {
      y = total_height + kTabVerticalPadding;
      row_index = 0;
      x = 0;
    }
  }
  layouts.host_size = gfx::Size(total_width, total_height);
  layouts.child_layouts.emplace_back();
  auto& placeholder_layout = layouts.child_layouts.back();
  placeholder_layout.child_view = placeholder_label_;
  placeholder_layout.visible = false;
  return layouts;
}

void VerticalPinnedTabContainerView::ResetCollectionNode() {
  collection_node_ = nullptr;
}

BEGIN_METADATA(VerticalPinnedTabContainerView)
END_METADATA
