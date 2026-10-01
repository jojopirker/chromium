// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_VIEW_H_

#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/browser_apis/tab_strip/tab_strip_api_types.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace tabs_api {
class TabStripService;
namespace mojom {
class Tab;
}  // namespace mojom
}  // namespace tabs_api

namespace ui {
class ColorProvider;
class KeyEvent;
class MouseEvent;
}  // namespace ui

namespace views {
class ImageButton;
class ImageView;
class Label;
}  // namespace views

class TabCollectionNode;

// View for a vertical tabstrip's tab.
class VerticalTabView : public views::View {
  METADATA_HEADER(VerticalTabView, views::View)

 public:
  explicit VerticalTabView(TabCollectionNode* collection_node);
  VerticalTabView(const VerticalTabView&) = delete;
  VerticalTabView& operator=(const VerticalTabView&) = delete;
  ~VerticalTabView() override;

  void UpdateFromData(const tabs_api::mojom::Tab& tab_data);

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnThemeChanged() override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

 private:
  void ResetCollectionNode();
  void UpdateVisualState() override;
  void UpdateCloseButtonIcon(const ui::ColorProvider* color_provider);
  void ActivateTab();
  void OnCloseButtonPressed();

  raw_ptr<TabCollectionNode> collection_node_ = nullptr;
  raw_ptr<tabs_api::TabStripService> service_ = nullptr;
  raw_ptr<views::ImageView> favicon_view_ = nullptr;
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::ImageButton> close_button_ = nullptr;
  std::optional<tabs_api::NodeId> tab_id_;
  bool is_active_ = false;
  bool is_selected_ = false;
  bool is_hovered_ = false;

  base::CallbackListSubscription node_destroyed_subscription_;
  base::WeakPtrFactory<VerticalTabView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_VERTICAL_VERTICAL_TAB_VIEW_H_
