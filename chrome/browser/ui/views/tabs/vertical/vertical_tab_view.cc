// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"

#include <algorithm>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/tabs/tab_group_theme.h"
#include "chrome/browser/ui/tabs/tab_strip_api/utilities/tab_strip_api_utilities.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service.h"
#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_apis/tab_strip/types/position.h"
#include "components/browser_apis/tab_strip/tab_strip_api_data_model.mojom.h"
#include "components/browser_apis/tab_strip/tab_strip_api_types.mojom.h"
#include "components/tab_groups/tab_group_color.h"
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
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_utils.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "third_party/skia/include/core/SkColor.h"

namespace {
constexpr int kVerticalTabHeight = 40;
constexpr int kVerticalTabDefaultWidth = 220;
constexpr int kCornerRadius = 8;
constexpr gfx::Insets kTabPadding = gfx::Insets::VH(6, 12);
constexpr int kIconSize = 16;
constexpr int kPinnedTabPreferredWidth = 48;
constexpr int kGroupIndicatorWidth = 4;
constexpr SkColor kDragPlaceholderBorderColor =
    SkColorSetARGB(160, 66, 133, 244);
constexpr SkColor kDragPlaceholderFillColor =
    SkColorSetARGB(48, 66, 133, 244);

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

  group_indicator_ = AddChildView(std::make_unique<views::View>());
  group_indicator_->SetPreferredSize(
      gfx::Size(kGroupIndicatorWidth, kVerticalTabHeight));
  group_indicator_->SetVisible(false);

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

  UpdatePinnedState();

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
  const int width = is_pinned_ ? kPinnedTabPreferredWidth
                               : (available_size.width().is_bounded()
                                      ? available_size.width().value()
                                      : kVerticalTabDefaultWidth);
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
    drag_pending_ = true;
    dragging_ = false;
    drop_index_.reset();
    drop_container_ = nullptr;
    drop_placeholder_ = nullptr;
    drag_start_index_ = 0;
    drag_start_point_ = event.location();
    drag_start_root_location_ = event.root_location();
    RequestFocus();
    return true;
  }
  return View::OnMousePressed(event);
}

void VerticalTabView::OnMouseReleased(const ui::MouseEvent& event) {
  views::View::OnMouseReleased(event);
  if (dragging_) {
    CompleteDrag(event);
    return;
  }

  drag_pending_ = false;

  if (!event.IsOnlyLeftMouseButton() || !tab_id_.has_value()) {
    return;
  }

  if (close_button_ && close_button_->GetVisible() &&
      close_button_->bounds().Contains(event.location())) {
    return;
  }

  ActivateTab();
}

bool VerticalTabView::OnMouseDragged(const ui::MouseEvent& event) {
  if (!drag_pending_ && !dragging_) {
    return View::OnMouseDragged(event);
  }

  if (!dragging_) {
    gfx::Vector2d delta = event.location() - drag_start_point_;
    if (!View::ExceededDragThreshold(delta)) {
      return true;
    }
    StartDrag();
  }

  if (dragging_) {
    UpdateDrag(event);
  }
  return true;
}

bool VerticalTabView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_RETURN || event.key_code() == ui::VKEY_SPACE) {
    ActivateTab();
    return true;
  }
  return View::OnKeyPressed(event);
}

void VerticalTabView::OnMouseCaptureLost() {
  views::View::OnMouseCaptureLost();
  if (!dragging_) {
    drag_pending_ = false;
    return;
  }
  ResetDragState(drag_start_index_);
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

  UpdateGroupIndicator(color_provider);

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

void VerticalTabView::UpdatePinnedState() {
  bool pinned = false;
  if (collection_node_) {
    if (TabCollectionNode* parent = collection_node_->parent()) {
      pinned = parent->GetType() == TabCollectionNode::Type::kPinnedTabs;
    }
  }

  is_pinned_ = pinned;
  title_label_->SetVisible(!is_pinned_);
  if (close_button_) {
    close_button_->SetVisible(!is_pinned_);
  }
  if (group_indicator_) {
    group_indicator_->SetVisible(!is_pinned_ && in_tab_group_);
  }
  InvalidateLayout();
}

void VerticalTabView::UpdateGroupIndicator(
    const ui::ColorProvider* color_provider) {
  if (!group_indicator_) {
    return;
  }

  in_tab_group_ = false;
  TabCollectionNode* group_node =
      FindAncestorOfType(TabCollectionNode::Type::kTabGroup);
  if (!group_node || !group_node->data() ||
      !group_node->data()->is_tab_group()) {
    group_indicator_->SetVisible(false);
    return;
  }

  const auto& tab_group_ptr = group_node->data()->get_tab_group();
  if (!tab_group_ptr) {
    group_indicator_->SetVisible(false);
    return;
  }

  in_tab_group_ = true;
  const tab_groups::TabGroupVisualData& group_data = (*tab_group_ptr).data;
  tab_group_color_id_ = group_data.color();

  const bool active_frame = GetWidget() ? GetWidget()->IsActive() : false;
  ui::ColorId color_id =
      GetTabGroupTabStripColorId(tab_group_color_id_, active_frame);
  const SkColor indicator_color =
      color_provider ? color_provider->GetColor(color_id) : SK_ColorTRANSPARENT;

  group_indicator_->SetBackground(
      views::CreateSolidBackground(indicator_color));
  group_indicator_->SetPreferredSize(
      gfx::Size(kGroupIndicatorWidth, kVerticalTabHeight));
  group_indicator_->SetVisible(!is_pinned_);
}

TabCollectionNode* VerticalTabView::FindAncestorOfType(
    TabCollectionNode::Type type) const {
  TabCollectionNode* current =
      collection_node_ ? collection_node_->parent() : nullptr;
  while (current) {
    if (current->GetType() == type) {
      return current;
    }
    current = current->parent();
  }
  return nullptr;
}

void VerticalTabView::StartDrag() {
  drag_pending_ = false;
  dragging_ = true;
  drop_container_ = parent();
  drop_placeholder_ = nullptr;
  drop_index_.reset();
  is_hovered_ = false;
  UpdateVisualState();
  if (drop_container_) {
    int current_index = drop_container_->GetIndexOf(this);
    if (current_index >= 0) {
      drag_start_index_ = static_cast<size_t>(current_index);
      auto placeholder = std::make_unique<views::View>();
      placeholder->SetPreferredSize(bounds().size());

      const ui::ColorProvider* color_provider = GetColorProvider();
      SkColor border_color = color_provider
                                 ? color_provider->GetColor(ui::kColorFocusRing)
                                 : kDragPlaceholderBorderColor;
      SkColor fill_color =
          color_provider ? SkColorSetA(border_color, 48)
                         : kDragPlaceholderFillColor;
      placeholder->SetBackground(
          views::CreateRoundedRectBackground(fill_color, kCornerRadius));
      placeholder->SetBorder(
          views::CreateRoundedRectBorder(2, kCornerRadius, border_color));

      drop_placeholder_ = drop_container_->AddChildView(std::move(placeholder));
      drop_container_->ReorderChildView(drop_placeholder_, current_index);
      drop_container_->ReorderChildView(
          this, drop_container_->children().size() - 1);
      SetProperty(views::kViewIgnoredByLayoutKey, true);
      drop_index_ = drag_start_index_;
      drop_container_->InvalidateLayout();
    }
  }

  SetPaintToLayer();
  if (layer()) {
    layer()->SetFillsBoundsOpaquely(false);
    layer()->SetOpacity(1.0f);
    layer()->SetTransform(gfx::Transform());
  }

  if (views::Widget* widget = GetWidget()) {
    widget->SetCapture(this);
  }
}

void VerticalTabView::UpdateDrag(const ui::MouseEvent& event) {
  if (!dragging_) {
    return;
  }

  drop_index_ = CalculateDropIndex(event.location());
  if (!drop_container_) {
    return;
  }

  gfx::Vector2d delta = event.root_location() - drag_start_root_location_;
  if (layer()) {
    gfx::Transform transform;
    transform.Translate(0, static_cast<float>(delta.y()));
    layer()->SetTransform(transform);
  }

  if (!drop_index_.has_value()) {
    return;
  }

  if (drop_placeholder_) {
    int placeholder_index = drop_container_->GetIndexOf(drop_placeholder_);
    if (placeholder_index < 0 ||
        static_cast<size_t>(placeholder_index) != drop_index_.value()) {
      drop_container_->ReorderChildView(
          drop_placeholder_, static_cast<int>(drop_index_.value()));
      drop_container_->InvalidateLayout();
    }
  }
}

void VerticalTabView::CompleteDrag(const ui::MouseEvent& event) {
  if (!dragging_) {
    ResetDragState(/*final_index=*/std::nullopt);
    return;
  }

  if (!drop_index_.has_value()) {
    drop_index_ = CalculateDropIndex(event.location());
  }

  if (!collection_node_ || !collection_node_->parent() ||
      !drop_index_.has_value()) {
    ResetDragState(drag_start_index_);
    return;
  }

  TabCollectionNode* parent_node = collection_node_->parent();
  const auto& siblings = parent_node->children();
  auto it = std::find_if(
      siblings.begin(), siblings.end(),
      [this](const std::unique_ptr<TabCollectionNode>& child) {
        return child.get() == collection_node_;
      });
  if (it == siblings.end()) {
    ResetDragState(drag_start_index_);
    return;
  }

  size_t current_index =
      static_cast<size_t>(std::distance(siblings.begin(), it));
  size_t target_index = drop_index_.value();

  // Clamp target_index to the number of siblings with associated views.
  size_t max_index = 0;
  for (const auto& sibling : siblings) {
    if (sibling.get() == collection_node_) {
      continue;
    }
    if (!sibling->node_view()) {
      continue;
    }
    ++max_index;
  }
  if (target_index > max_index) {
    target_index = max_index;
  }

  if (target_index == current_index) {
    ResetDragState(current_index);
    return;
  }

  std::optional<tabs_api::NodeId> parent_id;
  if (parent_node->data()) {
    parent_id =
        tabs_api::NodeId(tabs_api::utils::GetNodeId(*parent_node->data()));
  }
  tabs_api::Position position(target_index, parent_id);

  bool move_succeeded = false;
  if (service_ && tab_id_.has_value()) {
    auto move_result = service_->MoveNode(tab_id_.value(), position);
    move_succeeded = move_result.has_value();
  }

  if (move_succeeded) {
    parent_node->MoveChild(collection_node_, target_index);
  } else {
    target_index = current_index;
  }

  ResetDragState(target_index);
}

void VerticalTabView::ResetDragState(std::optional<size_t> final_index) {
  if (dragging_) {
    if (views::Widget* widget = GetWidget()) {
      if (widget->HasCapture()) {
        widget->ReleaseCapture();
      }
    }
  }

  if (layer()) {
    layer()->SetTransform(gfx::Transform());
    DestroyLayer();
  }

  if (drop_container_) {
    if (drop_placeholder_) {
      drop_container_->RemoveChildViewT(drop_placeholder_);
      drop_placeholder_ = nullptr;
    }
    if (final_index.has_value()) {
      drop_container_->ReorderChildView(
          this, static_cast<int>(final_index.value()));
      drag_start_index_ = final_index.value();
    }
    drop_container_->InvalidateLayout();
  }

  dragging_ = false;
  drag_pending_ = false;
  drop_container_ = nullptr;
  drop_placeholder_ = nullptr;
  drop_index_.reset();
  SetProperty(views::kViewIgnoredByLayoutKey, false);
}

size_t VerticalTabView::CalculateDropIndex(const gfx::Point& location) const {
  if (!collection_node_ || !collection_node_->parent()) {
    return 0u;
  }

  TabCollectionNode* parent_node = collection_node_->parent();
  views::View* parent_view = parent_node->node_view();
  if (!parent_view) {
    return 0u;
  }

  gfx::Point location_in_parent = location;
  views::View::ConvertPointToTarget(this, parent_view, &location_in_parent);

  size_t index = 0;
  for (const auto& sibling : parent_node->children()) {
    if (sibling.get() == collection_node_) {
      continue;
    }
    views::View* sibling_view = sibling->node_view();
    if (!sibling_view || !sibling_view->GetVisible()) {
      continue;
    }

    const int midpoint = sibling_view->bounds().CenterPoint().y();
    if (location_in_parent.y() < midpoint) {
      break;
    }
    ++index;
  }
  return index;
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
