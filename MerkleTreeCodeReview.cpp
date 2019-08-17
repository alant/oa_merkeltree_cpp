// Implements an append-only Merkle tree
// (see https://en.wikipedia.org/wiki/Merkle_tree for definition)
// storing a 32 byte hash for each node.

#include <iostream>
#include <map>
#include <memory>

// picosha2.h provides function:
// template <typename InContainer>
// std::string hash256_hex_string(const InContainer& src);
// where InContainer must provide begin() and end() with ForwardIter compatible
// semantics.
#include "./picosha2.h"

// Class for each Node in Merkle Tree
class MerkleNode {
protected:
  std::shared_ptr<MerkleNode> left_, right_, parent_;
  std::string hash_;
  std::shared_ptr<std::string> value_;

public:
  // Leaf Node
  MerkleNode(const std::string &value)
      : value_(new std::string(value)), left_(nullptr), right_(nullptr),
        parent_(nullptr) {
    this->hash_ = picosha2::hash256_hex_string(*value_);
  }

  // Intermediate Node
  MerkleNode(MerkleNode *left, MerkleNode *right)
      : left_(left), right_(right), value_(nullptr), parent_(nullptr) {
    std::string hash_input = "";
    hash_ = picosha2::hash256_hex_string(left_->hash() + right_->hash());
  }

  std::shared_ptr<MerkleNode> left() const { return left_; }
  std::shared_ptr<MerkleNode> right() const { return right_; }
  std::shared_ptr<MerkleNode> parent() const { return parent_; }
  std::string value() const { return *value_.get(); }
  std::string hash() const { return hash_; }

  void set_parent(std::shared_ptr<MerkleNode> parent) { parent_ = parent; };
};

class StreamingMerkleTree {
protected:
  std::shared_ptr<MerkleNode> root_;
  std::map<int, std::shared_ptr<MerkleNode> > frontier_;

  std::shared_ptr<MerkleNode> getSibling(std::shared_ptr<MerkleNode> node) {
    std::shared_ptr<MerkleNode> parent = node->parent();
    if (parent) {
      if (parent->left() == node && parent->right()) {
        return parent->right();
      } else if (parent->right() == node && parent->left()) {
        return parent->left();
      }
    }
    return nullptr;
  }

  virtual void updateRoot() {
    std::vector<std::shared_ptr<MerkleNode> > level;
    for (std::map<int, std::shared_ptr<MerkleNode> >::iterator it =
             frontier_.begin();
         it != frontier_.end(); ++it) {
      level.push_back(it->second);
    }

    while (level.size() > 1) {
      std::vector<std::shared_ptr<MerkleNode> > temp;
      for (int i = 0; i < level.size() - 1; i += 2) {
        MerkleNode *left = level[i].get();
        MerkleNode *right = level[i + 1].get();
        auto hashed = std::make_shared<MerkleNode>(left, right);
        level[i]->set_parent(hashed);
        level[i + 1]->set_parent(hashed);
        std::cout << "root " << hashed << " = " << level[i]->parent() << " = "
                  << level[i + 1]->parent() << '\n';
        temp.push_back(hashed);
      }
      if (level.size() % 2 == 1) {
        MerkleNode *left = level[level.size() - 1].get();
        MerkleNode *right = nullptr;
        auto hashed = std::make_shared<MerkleNode>(left, right);
        level[level.size() - 1]->set_parent(hashed);

        temp.push_back(hashed);
      }
      level = temp;
    }
    std::cout << "setting root " << root_ << level[0] << '\n';
    root_ = level[0];
  }

public:
  StreamingMerkleTree() : root_(nullptr){};

  virtual void push_back(std::shared_ptr<MerkleNode> node) {
    std::shared_ptr<MerkleNode> combine(node);
    int size = 1;
    while (frontier_.count(size) > 0) {
      MerkleNode *left = combine.get();
      MerkleNode *right = frontier_[size].get();
      auto temp = std::make_shared<MerkleNode>(left, right);
      combine->set_parent(temp);
      frontier_[size]->set_parent(temp);
      combine = temp;
      frontier_.erase(size);
      size *= 2;
    }
    frontier_[size] = combine;
    updateRoot();
  }

  virtual std::vector<std::string>
  generateProof(std::shared_ptr<MerkleNode> node) {
    auto current = node;
    std::vector<std::string> proof;
    while (current != root_) {
      std::shared_ptr<MerkleNode> sibling = getSibling(current);
      proof.push_back(sibling->hash());
      current = current->parent();
    }
    proof.push_back(root_->hash());
    return proof;
  }

  std::map<int, std::shared_ptr<MerkleNode> > frontier() const {
    return frontier_;
  }
};

// Tests
int main(int argc, const char *argv[]) {

  auto node1 = std::make_shared<MerkleNode>("1 transaction");
  auto node2 = std::make_shared<MerkleNode>("2 transaction");
  auto node3 = std::make_shared<MerkleNode>("3 transaction");
  auto node4 = std::make_shared<MerkleNode>("4 transaction");

  StreamingMerkleTree *tree = new StreamingMerkleTree();
  tree->push_back(node1);
  std::cout << "1 node " << tree->frontier().count(1)
            << tree->frontier().count(2) << '\n';
  tree->push_back(node2);
  std::cout << "2 node " << tree->frontier().count(1)
            << tree->frontier().count(2) << '\n';
  tree->push_back(node3);
  std::cout << "3 node " << tree->frontier().count(1)
            << tree->frontier().count(2) << '\n';
  tree->push_back(node4);
  std::cout << "4 node " << tree->frontier().count(1)
            << tree->frontier().count(2) << tree->frontier().count(4) << '\n';

  std::vector<std::string> proof = tree->generateProof(node1);
  std::cout << "Proof Size: " << proof.size() << '\n';
}
