// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_unpinned_tab_container_view.h"

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
constexpr int kTabVerticalPadding = 2;
constexpr char16_t kUnpinnedPlaceholderText[] = u"Sample unpinned tab";
}  // namespace

VerticalUnpinnedTabContainerView::VerticalUnpinnedTabContainerView(
    TabCollectionNode* collection_node)
    : collection_node_(collection_node) {
  SetLayoutManager(std::make_unique<views::DelegatingLayoutManager>(this));
  placeholder_label_ =
      AddChildView(std::make_unique<views::Label>(kUnpinnedPlaceholderText));
  placeholder_label_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_LEFT);
  placeholder_label_->SetBackground(
      views::CreateSolidBackground(ui::kColorFrameActive));
  placeholder_label_->SetEnabledColor(ui::kColorLabelForeground);
  placeholder_label_->SetAutoColorReadabilityEnabled(false);
  placeholder_label_->SetSubpixelRenderingEnabled(false);
  placeholder_label_->SetPaintToLayer();
  placeholder_label_->layer()->SetFillsBoundsOpaquely(true);

  node_destroyed_subscription_ = collection_node_->RegisterWillDestroyCallback(
      base::BindOnce(&VerticalUnpinnedTabContainerView::ResetCollectionNode,
                     base::Unretained(this)));
}

VerticalUnpinnedTabContainerView::~VerticalUnpinnedTabContainerView() = default;

views::ProposedLayout VerticalUnpinnedTabContainerView::CalculateProposedLayout(
    const views::SizeBounds& size_bounds) const {
  views::ProposedLayout layouts;
  int width = 0;
  int height = 0;
  int horizontal_padding =
      GetLayoutConstant(VERTICAL_TAB_STRIP_HORIZONTAL_PADDING);

  const auto children = collection_node_->GetDirectChildren();
  const bool has_children = !children.empty();

  if (!has_children) {
    const int available_width =
        size_bounds.width().is_bounded()
            ? std::max(0, size_bounds.width().value() - horizontal_padding)
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
    layouts.host_size =
        gfx::Size(available_width + horizontal_padding, placeholder_bounds.height());
    return layouts;
  }

  // Layout children in order. Children will have their preferred height and
  // fill available width.
  for (auto* child : children) {
    gfx::Rect bounds = gfx::Rect(child->GetPreferredSize());
    bounds.set_y(height);
    // If fully bounded, child views should respect width constraints and take
    // up the available width excluding trailing horizontal padding.
    if (size_bounds.is_fully_bounded()) {
      bounds.set_width(size_bounds.width().value() - horizontal_padding);
    }
    layouts.child_layouts.emplace_back(child, child->GetVisible(), bounds);
    height += bounds.height() + kTabVerticalPadding;
    width = std::max(width, bounds.width() + horizontal_padding);
  }
  // Remove excess padding if needed.
  if (!children.empty()) {
    height -= kTabVerticalPadding;
  }

  layouts.host_size = gfx::Size(width, height);
  layouts.child_layouts.emplace_back();
  auto& placeholder_layout = layouts.child_layouts.back();
  placeholder_layout.child_view = placeholder_label_;
  placeholder_layout.visible = false;
  return layouts;
}

void VerticalUnpinnedTabContainerView::ResetCollectionNode() {
  collection_node_ = nullptr;
}

BEGIN_METADATA(VerticalUnpinnedTabContainerView)
END_METADATA
