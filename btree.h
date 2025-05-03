#pragma once

#include<unordered_map>
#include<vector>
#include<variant>
#include<string>

/*
*   BTreeNode that stores whether it is a leaf, the key, values and pointer to children
*/
template<typename KeyType, typename ValueType> //Makes it so the types are arbitrary
class BTreeNode {
    public:
        bool is_leaf;
        std::vector<KeyType> keys;
        std::vector<ValueType*> values;
        std::vector<BTreeNode*> children;

        BTreeNode(bool leaf); // Constructor declaration
};

/*
*   BTree that stores the BTreeNodes, ensures it is balanced
*
*/
template <typename KeyType, typename ValueType>
class BTree {
    private:
        BTreeNode<KeyType, ValueType>* root;
        int maxKeysPerNode;  // Maximum keys in each node
        BTree(int maxKeys);  // Constructor declaration
        void insert(const KeyType& key, ValueType* value);
};

/*
* Fraction format for precise comparison
*/
struct fraction {
    int top;
    int bottom;
};


using VariantType = std::variant<int, float, std::string, fraction>;

// extern types for storage
extern std::unordered_map<std::string, BTree<int, int>> hashTreeStorage;
extern std::unordered_map<std::string, BTree<int, int>> intTreeStorage;
extern std::unordered_map<std::string, BTree<std::string, int>> stringTreeStorage;
extern std::unordered_map<std::string, BTree<fraction, int>> decTreeStorage;
extern std::vector<VariantType> memory;