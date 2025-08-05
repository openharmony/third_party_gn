// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/graph/include/module.h"

Module::Module(const std::string& name, const std::string& path, const Item* item) : Node(name, path)
{
    item_ = item;
}

const Item* Module::GetItem() const
{
    return item_;
}