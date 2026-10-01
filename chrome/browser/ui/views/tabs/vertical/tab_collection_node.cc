// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/tab_collection_node.h"

#include <algorithm>
#include <vector>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/tabs/tab_strip_api/utilities/tab_strip_api_utilities.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_pinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_split_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_unpinned_tab_container_view.h"
#include "components/browser_apis/tab_strip/tab_strip_api_types.mojom.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace {

TabCollectionNode::ViewFactory& GetViewFactory() {
  static base::NoDestructor<TabCollectionNode::ViewFactory> factory;
  return *factory;
}

class CollectionTestViewImpl : public views::View {
 public:
  explicit CollectionTestViewImpl(TabCollectionNode* node) {
    node->set_add_child_to_node(
        base::BindRepeating<TabCollectionNode::CustomAddChildView>(
            &views::View::AddChildView, base::Unretained(this)));
  }
  ~CollectionTestViewImpl() override = default;
};

}  // anonymous namespace

base::CallbackListSubscription TabCollectionNode::RegisterWillDestroyCallback(
    base::OnceClosure callback) {
  return on_will_destroy_callback_list_.Add(std::move(callback));
}

// static
void TabCollectionNode::SetViewFactoryForTesting(ViewFactory factory) {
  GetViewFactory() = std::move(factory);
}

// static
std::unique_ptr<views::View> TabCollectionNode::CreateViewForNode(
    TabCollectionNode* node_for_view) {
  if (GetViewFactory()) {
    return GetViewFactory().Run(node_for_view);
  }
  switch (node_for_view->GetType()) {
    case Type::kTabStrip:
      return std::make_unique<VerticalTabStripView>(node_for_view);
    case Type::kPinnedTabs:
      return std::make_unique<VerticalPinnedTabContainerView>(node_for_view);
    case Type::kUnpinnedTabs:
      return std::make_unique<VerticalUnpinnedTabContainerView>(node_for_view);
    case Type::kSplitTab:
      return std::make_unique<VerticalSplitTabView>(node_for_view);
    case Type::kTabGroup:
      // TODO(crbug.com/442567916): support tab groups.
      break;
    case Type::kTab:
      return std::make_unique<VerticalTabView>(node_for_view);
  }
  return std::make_unique<CollectionTestViewImpl>(node_for_view);
}

TabCollectionNode::TabCollectionNode(tabs_api::mojom::DataPtr data,
                                     tabs_api::TabStripService* service)
    : data_(std::move(data)), service_(service) {}

TabCollectionNode::~TabCollectionNode() {
  on_will_destroy_callback_list_.Notify();
}

std::unique_ptr<views::View> TabCollectionNode::Initialize(
    std::vector<tabs_api::mojom::ContainerPtr> child_containers) {
  CHECK(children_.empty());
  children_.reserve(child_containers.size());

  std::unique_ptr<views::View> node_view = CreateAndSetView();

  for (auto& child_container : child_containers) {
    auto child_node = std::make_unique<TabCollectionNode>(
        std::move(child_container->data), service_);
    auto child_node_view =
        child_node->Initialize(std::move(child_container->children));
    AddChild(std::move(child_node_view), std::move(child_node),
             children_.size());
  }

  return node_view;
}

void TabCollectionNode::SetData(base::PassKey<TabCollectionNode> pass_key,
                                tabs_api::mojom::DataPtr data) {
  data_ = std::move(data);
  if (!node_view_) {
    return;
  }

  switch (data_->which()) {
    case Type::kTab:
      if (auto* tab_view =
              views::AsViewClass<VerticalTabView>(node_view_)) {
        tab_view->UpdateFromData(*data_->get_tab());
      }
      break;
    default:
      break;
  }
}

// TODO(crbug.com/450976282): Consider having a map at the root level, or using
// path in the API, in order to not have to iterate through the whole collection
// node structure.
TabCollectionNode* TabCollectionNode::GetNodeForId(
    const tabs_api::NodeId& node_id) {
  if (tabs_api::utils::GetNodeId(*data_) == node_id) {
    return this;
  }

  for (const auto& child : children_) {
    if (TabCollectionNode* node = child->GetNodeForId(node_id)) {
      return node;
    }
  }

  return nullptr;
}

void TabCollectionNode::AddNewChild(base::PassKey<TabCollectionNode> pass_key,
                                    tabs_api::mojom::DataPtr data,
                                    size_t model_index) {
  auto child_node =
      std::make_unique<TabCollectionNode>(std::move(data), service_);
  auto child_node_view = child_node->CreateAndSetView();
  AddChild(std::move(child_node_view), std::move(child_node), model_index);
}

bool TabCollectionNode::RemoveNodeById(const tabs_api::NodeId& node_id) {
  for (size_t i = 0; i < children_.size(); ++i) {
    if (tabs_api::utils::GetNodeId(*children_[i]->data_) == node_id) {
      if (node_view_ && children_[i]->node_view_) {
        node_view_->RemoveChildViewT(children_[i]->node_view_);
        node_view_->InvalidateLayout();
      }
      children_[i]->parent_ = nullptr;
      children_.erase(children_.begin() + i);
      return true;
    }
    if (children_[i]->RemoveNodeById(node_id)) {
      return true;
    }
  }
  return false;
}

bool TabCollectionNode::MoveChild(TabCollectionNode* child_node,
                                  size_t target_index) {
  auto it = std::find_if(children_.begin(), children_.end(),
                         [child_node](const std::unique_ptr<TabCollectionNode>& entry) {
                           return entry.get() == child_node;
                         });
  if (it == children_.end()) {
    return false;
  }

  size_t current_index = static_cast<size_t>(std::distance(children_.begin(), it));
  if (current_index == target_index) {
    return true;
  }

  auto node_ptr = std::move(*it);
  children_.erase(it);

  if (target_index > children_.size()) {
    target_index = children_.size();
  }

  children_.insert(children_.begin() + target_index, std::move(node_ptr));

  if (node_view_ && child_node->node_view_) {
    node_view_->ReorderChildView(child_node->node_view_,
                                 static_cast<int>(target_index));
    node_view_->InvalidateLayout();
  }
  return true;
}

std::vector<views::View*> TabCollectionNode::GetDirectChildren() const {
  std::vector<views::View*> child_views;
  child_views.reserve(children_.size());
  for (const auto& child : children_) {
    child_views.push_back(child->node_view_);
  }
  return child_views;
}

std::unique_ptr<views::View> TabCollectionNode::CreateAndSetView() {
  auto node_view = CreateViewForNode(this);
  node_view_ = node_view.get();
  return node_view;
}

void TabCollectionNode::AddChild(std::unique_ptr<views::View> child_node_view,
                                 std::unique_ptr<TabCollectionNode> child_node,
                                 size_t model_index) {
  child_node->parent_ = this;
  children_.insert(children_.begin() + model_index, std::move(child_node));
  // Add child view after inserting the child node into children_, as adding the
  // view may depend on the order of the node in children_.
  if (add_child_to_node_) {
    add_child_to_node_.Run(std::move(child_node_view));
  } else {
    node_view_->AddChildView(std::move(child_node_view));
  }
}
