// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MODULE_H_
#define MODULE_H_

#include "gn/graph/include/node.h"
#include "gn/item.h"

class Item;

class Module : public Node {
public:

    Module(const std::string& name, const std::string& path, const Item* item);
    ~Module();

    const Item* GetItem() const;
    bool operator==(const Module& other) const {
        return this->GetName() == other.GetName();
    }

private:
    const Item* item_;
};

#endif // MODULE_H_
