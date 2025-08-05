// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NODE_H_
#define NODE_H_

#include <string>
#include <sys/stat.h>
#include <vector>

class Node {
public:
    Node(const std::string& name, const std::string& path);
    ~Node();

    const std::string& GetName() const;
    const std::string& GetPath() const;
    const std::vector<Node*>& GetFromList() const;
    const std::vector<Node*>& GetToList() const;
    void AddFrom(Node* node);
    void AddTo(Node* node);

private:
    std::string name_;
    std::string path_;
    std::vector<Node*> from_;
    std::vector<Node*> to_;
};

#endif // NODE_H_
