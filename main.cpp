#include <iostream>
#include "bptree.hpp"

using namespace std;

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    BPlusTree tree("bptree.dat");

    int n;
    cin >> n;

    for (int i = 0; i < n; i++) {
        string cmd;
        cin >> cmd;

        if (cmd == "insert") {
            string key;
            int value;
            cin >> key >> value;
            tree.insert(key.c_str(), value);
        } else if (cmd == "find") {
            string key;
            cin >> key;
            vector<int> result = tree.find(key.c_str());

            if (result.empty()) {
                cout << "null\n";
            } else {
                for (size_t i = 0; i < result.size(); i++) {
                    if (i > 0) cout << " ";
                    cout << result[i];
                }
                cout << "\n";
            }
        } else if (cmd == "delete") {
            string key;
            int value;
            cin >> key >> value;
            tree.remove(key.c_str(), value);
        }
    }

    return 0;
}
