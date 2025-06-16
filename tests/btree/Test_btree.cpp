#include "btree.h"

#include <assert.h>
#include <iostream>

int main(){
    // Create a BTree
    BTree<int, int> btree(3);
    std::cout << "Insert keys into BTree\n";
    std::vector<int> test_data;
    for (int i = 0; i < 10; ++i){
        test_data.push_back(i * 10);
        btree.insert(i, &test_data[i]);
    }

    return 0;
}