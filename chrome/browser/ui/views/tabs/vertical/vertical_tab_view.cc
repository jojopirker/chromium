// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_apis/tab_strip/tab_strip_api_data_model.mojom.h"
#include "components/browser_apis/tab_strip/tab_strip_api_types.mojom.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "third_party/skia/include/core/SkColor.h"

namespace {
constexpr int kVerticalTabHeight = 40;
constexpr int kVerticalTabDefaultWidth = 220;
constexpr int kCornerRadius = 8;
constexpr gfx::Insets kTabPadding = gfx::Insets::VH(6, 12);
constexpr int kIconSize = 16;

std::u16string GetDisplayTitle(const tabs_api::mojom::Tab& tab_data) {
  if (!tab_data.title.empty()) {
    return base::UTF8ToUTF16(tab_data.title);
  }
  return u"Tab";
}

ui::ImageModel GetFaviconImageModel(const gfx::ImageSkia& image,
                                    const ui::ColorProvider* color_provider) {
  if (!image.isNull()) {
    return ui::ImageModel::FromImageSkia(image);
  }

  const SkColor color =
      color_provider ? color_provider->GetColor(ui::kColorIcon) : SK_ColorDKGRAY;
  return ui::ImageModel::FromVectorIcon(vector_icons::kGlobeIcon, color,
                                        kIconSize);
}
}  // namespace

VerticalTabView::VerticalTabView(TabCollectionNode* collection_node)
    : collection_node_(collection_node),
      service_(collection_node->service()) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kTabPadding,
      /*between_child_spacing=*/8));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  SetNotifyEnterExitOnChild(true);

  favicon_view_ = AddChildView(std::make_unique<views::ImageView>());
  favicon_view_->SetPreferredSize(gfx::Size(kIconSize, kIconSize));
  favicon_view_->SetVisible(false);

  title_label_ = AddChildView(std::make_unique<views::Label>());
  title_label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  title_label_->SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);
  title_label_->SetAllowCharacterBreak(true);
  title_label_->SetSubpixelRenderingEnabled(false);
  layout->SetFlexForView(title_label_, 1);

  close_button_ = AddChildView(std::make_unique<views::ImageButton>(
      base::BindRepeating(&VerticalTabView::OnCloseButtonPressed,
                          weak_factory_.GetWeakPtr())));
  close_button_->SetFocusBehavior(FocusBehavior::NEVER);
  close_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_CLOSE_TAB));
  close_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE_TAB));
  close_button_->SetPreferredSize(gfx::Size(kIconSize, kIconSize));

  GetViewAccessibility().SetRole(ax::mojom::Role::kTab);

  node_destroyed_subscription_ =
      collection_node_->RegisterWillDestroyCallback(base::BindOnce(
          &VerticalTabView::ResetCollectionNode, base::Unretained(this)));

  if (collection_node_->data() &&
      collection_node_->data()->is_tab()) {
    UpdateFromData(*collection_node_->data()->get_tab());
  } else {
    const std::u16string placeholder = u"(Tab)";
    title_label_->SetText(placeholder);
    GetViewAccessibility().SetName(placeholder);
    UpdateCloseButtonIcon(GetColorProvider());
  }

  SetFocusBehavior(FocusBehavior::ALWAYS);
}

VerticalTabView::~VerticalTabView() = default;

void VerticalTabView::UpdateFromData(const tabs_api::mojom::Tab& tab_data) {
  if (collection_node_) {
    service_ = collection_node_->service();
  }

  tab_id_ = tab_data.id;

  is_active_ = tab_data.is_active;
  is_selected_ = tab_data.is_selected;

  const std::u16string title = GetDisplayTitle(tab_data);
  title_label_->SetText(title);
  SetTooltipText(title);
  GetViewAccessibility().SetName(title);
  GetViewAccessibility().SetIsSelected(is_selected_);

  const ui::ColorProvider* color_provider = GetColorProvider();
  const ui::ImageModel favicon_model =
      GetFaviconImageModel(tab_data.favicon, color_provider);
  favicon_view_->SetImage(favicon_model);
  favicon_view_->SetVisible(true);

  if (close_button_) {
    close_button_->SetEnabled(!tab_data.is_blocked);
  }

  UpdateVisualState();
}

gfx::Size VerticalTabView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int width =
      available_size.width().is_bounded()
          ? available_size.width().value()
          : kVerticalTabDefaultWidth;
  return gfx::Size(width, kVerticalTabHeight);
}

void VerticalTabView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateVisualState();
}

void VerticalTabView::OnMouseEntered(const ui::MouseEvent& event) {
  views::View::OnMouseEntered(event);
  is_hovered_ = true;
  UpdateVisualState();
}

void VerticalTabView::OnMouseExited(const ui::MouseEvent& event) {
  views::View::OnMouseExited(event);
  is_hovered_ = false;
  UpdateVisualState();
}

bool VerticalTabView::OnMousePressed(const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton()) {
    RequestFocus();
    return true;
  }
  return View::OnMousePressed(event);
}

void VerticalTabView::OnMouseReleased(const ui::MouseEvent& event) {
  views::View::OnMouseReleased(event);
  if (!event.IsOnlyLeftMouseButton() || !tab_id_.has_value()) {
    return;
  }

  if (close_button_ && close_button_->GetVisible() &&
      close_button_->bounds().Contains(event.location())) {
    return;
  }

  ActivateTab();
}

bool VerticalTabView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_RETURN || event.key_code() == ui::VKEY_SPACE) {
    ActivateTab();
    return true;
  }
  return View::OnKeyPressed(event);
}

void VerticalTabView::ResetCollectionNode() {
  collection_node_ = nullptr;
}

void VerticalTabView::UpdateVisualState() {
  const ui::ColorProvider* color_provider = GetColorProvider();
  constexpr SkColor kFallbackActiveBackground = SkColorSetARGB(255, 224, 233, 248);
  SkColor active_background =
      color_provider ? color_utils::AlphaBlend(
                           color_provider->GetColor(ui::kColorFrameActive),
                           SK_ColorWHITE, static_cast<SkAlpha>(0x30))
                     : kFallbackActiveBackground;
  SkColor foreground =
      color_provider ? color_provider->GetColor(ui::kColorLabelForeground)
                     : SkColorSetRGB(0x20, 0x20, 0x20);
  SkColor hover_background =
      color_provider ? color_provider->GetColor(ui::kColorButtonBackground)
                     : SkColorSetARGB(30, 0, 0, 0);

  if (is_active_) {
    SetBackground(views::CreateRoundedRectBackground(active_background,
                                                     kCornerRadius));
    title_label_->SetEnabledColor(foreground);
  } else {
    if (is_hovered_) {
      SetBackground(views::CreateRoundedRectBackground(hover_background,
                                                       kCornerRadius));
    } else {
      SetBackground(nullptr);
    }
    title_label_->SetEnabledColor(foreground);
  }

  gfx::FontList font_list = title_label_->font_list();
  font_list = font_list.Derive(0, gfx::Font::NORMAL,
                               is_active_ ? gfx::Font::Weight::SEMIBOLD
                                          : gfx::Font::Weight::NORMAL);
  title_label_->SetFontList(font_list);

  UpdateCloseButtonIcon(color_provider);
}

void VerticalTabView::UpdateCloseButtonIcon(
    const ui::ColorProvider* color_provider) {
  if (!close_button_) {
    return;
  }

  const SkColor icon_color =
      color_provider ? color_provider->GetColor(ui::kColorIcon) : SK_ColorBLACK;
  close_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kCloseRoundedIcon, icon_color));
  close_button_->SetImageModel(
      views::Button::STATE_HOVERED,
      ui::ImageModel::FromVectorIcon(vector_icons::kCloseRoundedIcon, icon_color));
  close_button_->SetImageModel(
      views::Button::STATE_PRESSED,
      ui::ImageModel::FromVectorIcon(vector_icons::kCloseRoundedIcon, icon_color));
}

void VerticalTabView::ActivateTab() {
  if (!service_ || !tab_id_.has_value()) {
    return;
  }
  [[maybe_unused]] auto activate_result =
      service_->ActivateTab(tab_id_.value());
}

void VerticalTabView::OnCloseButtonPressed() {
  if (!service_ || !tab_id_.has_value()) {
    return;
  }
  std::vector<tabs_api::NodeId> ids;
  ids.push_back(tab_id_.value());
  [[maybe_unused]] auto close_result = service_->CloseTabs(ids);
}

BEGIN_METADATA(VerticalTabView)
END_METADATA
