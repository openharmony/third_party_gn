// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/graph/include/node.h"

Node::Node(const std::string& name, const std::string& path)
{
    name_ = name;
    path_ = path;
}

const std::string& Node::GetName() const
{ 
    return name_; 
}

const std::string& Node::GetPath() const
{
    return path_;
}

const std::vector<Node*>& Node::GetFromList() const
{
    return from_;
}

const std::vector<Node*>& Node::GetToList() const
{
    return to_;
}

void Node::AddFrom(Node* node)
{
    from_.push_back(node);
}

void Node::AddTo(Node* node)
{
    to_.push_back(node);
}