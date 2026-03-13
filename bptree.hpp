#ifndef BPTREE_HPP
#define BPTREE_HPP

#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>

using namespace std;

const int MAX_KEY_SIZE = 65;
const int M = 100;  // Order of B+ tree (number of children pointers)
const int MIN_KEYS = M / 2 - 1;
const int MAX_KEYS = M - 1;

struct KeyValue {
    char key[MAX_KEY_SIZE];
    int value;

    KeyValue() {
        memset(key, 0, sizeof(key));
        value = 0;
    }

    KeyValue(const char* k, int v) : value(v) {
        strncpy(key, k, MAX_KEY_SIZE - 1);
        key[MAX_KEY_SIZE - 1] = '\0';
    }

    bool operator<(const KeyValue& other) const {
        int cmp = strcmp(key, other.key);
        if (cmp != 0) return cmp < 0;
        return value < other.value;
    }

    bool operator==(const KeyValue& other) const {
        return strcmp(key, other.key) == 0 && value == other.value;
    }

    int compare_key(const char* k) const {
        return strcmp(key, k);
    }
};

struct Node {
    bool is_leaf;
    int n;  // number of keys
    KeyValue keys[MAX_KEYS + 1];  // +1 for temporary during split
    int children[M + 1];  // file positions, +1 for temporary during split
    int next;  // next leaf (only for leaves)

    Node() : is_leaf(true), n(0), next(-1) {
        for (int i = 0; i <= M; i++) children[i] = -1;
    }
};

class BPlusTree {
private:
    fstream file;
    string filename;
    int root_pos;
    int file_end;

    void write_header() {
        file.seekp(0);
        file.write(reinterpret_cast<char*>(&root_pos), sizeof(int));
        file.write(reinterpret_cast<char*>(&file_end), sizeof(int));
        file.flush();
    }

    void read_header() {
        file.seekg(0);
        file.read(reinterpret_cast<char*>(&root_pos), sizeof(int));
        file.read(reinterpret_cast<char*>(&file_end), sizeof(int));
    }

    int allocate_node() {
        int pos = file_end;
        file_end += sizeof(Node);
        return pos;
    }

    void write_node(int pos, const Node& node) {
        file.seekp(pos);
        file.write(reinterpret_cast<const char*>(&node), sizeof(Node));
        file.flush();
    }

    Node read_node(int pos) {
        Node node;
        file.seekg(pos);
        file.read(reinterpret_cast<char*>(&node), sizeof(Node));
        return node;
    }

    void insert_in_leaf(Node& leaf, const KeyValue& kv) {
        int i = leaf.n - 1;
        while (i >= 0 && kv < leaf.keys[i]) {
            leaf.keys[i + 1] = leaf.keys[i];
            i--;
        }
        leaf.keys[i + 1] = kv;
        leaf.n++;
    }

    void insert_in_parent(int left_pos, const KeyValue& key, int right_pos) {
        if (left_pos == root_pos) {
            // Create new root
            Node new_root;
            new_root.is_leaf = false;
            new_root.n = 1;
            new_root.keys[0] = key;
            new_root.children[0] = left_pos;
            new_root.children[1] = right_pos;

            root_pos = allocate_node();
            write_node(root_pos, new_root);
            return;
        }

        // Find parent (simplified - in practice would need parent pointers or stack)
        // For now, we'll rebuild from root
    }

    int insert_helper(int node_pos, const KeyValue& kv) {
        Node node = read_node(node_pos);

        if (node.is_leaf) {
            // Insert in leaf
            insert_in_leaf(node, kv);

            if (node.n <= MAX_KEYS) {
                write_node(node_pos, node);
                return -1;  // No split needed
            }

            // Split leaf
            Node new_leaf;
            new_leaf.is_leaf = true;

            int mid = node.n / 2;
            new_leaf.n = node.n - mid;
            node.n = mid;

            for (int i = 0; i < new_leaf.n; i++) {
                new_leaf.keys[i] = node.keys[mid + i];
            }

            new_leaf.next = node.next;
            int new_pos = allocate_node();
            node.next = new_pos;

            write_node(node_pos, node);
            write_node(new_pos, new_leaf);

            return new_pos;  // Return new leaf pos for parent update
        } else {
            // Internal node - find child
            int i = 0;
            while (i < node.n && kv.compare_key(node.keys[i].key) >= 0) {
                i++;
            }

            int new_child = insert_helper(node.children[i], kv);

            if (new_child == -1) {
                return -1;  // No split propagated
            }

            // Insert new child pointer
            Node new_child_node = read_node(new_child);

            for (int j = node.n; j > i; j--) {
                node.keys[j] = node.keys[j - 1];
                node.children[j + 1] = node.children[j];
            }

            node.keys[i] = new_child_node.keys[0];
            node.children[i + 1] = new_child;
            node.n++;

            if (node.n <= MAX_KEYS) {
                write_node(node_pos, node);
                return -1;
            }

            // Split internal node
            Node new_internal;
            new_internal.is_leaf = false;

            int mid = node.n / 2;
            new_internal.n = node.n - mid - 1;
            node.n = mid;

            for (int j = 0; j < new_internal.n; j++) {
                new_internal.keys[j] = node.keys[mid + 1 + j];
                new_internal.children[j] = node.children[mid + 1 + j];
            }
            new_internal.children[new_internal.n] = node.children[node.n + 1];

            int new_pos = allocate_node();
            write_node(node_pos, node);
            write_node(new_pos, new_internal);

            return new_pos;
        }
    }

public:
    BPlusTree(const string& fname) : filename(fname) {
        file.open(filename, ios::in | ios::out | ios::binary);

        if (!file.is_open() || file.peek() == EOF) {
            file.close();
            file.open(filename, ios::out | ios::binary);
            file.close();
            file.open(filename, ios::in | ios::out | ios::binary);

            root_pos = 2 * sizeof(int);
            file_end = root_pos + sizeof(Node);

            Node root;
            write_node(root_pos, root);
            write_header();
        } else {
            read_header();
        }
    }

    ~BPlusTree() {
        if (file.is_open()) {
            write_header();
            file.close();
        }
    }

    void insert(const char* key, int value) {
        KeyValue kv(key, value);

        int new_child = insert_helper(root_pos, kv);

        if (new_child != -1) {
            // Root was split, create new root
            Node new_sibling = read_node(new_child);

            Node new_root;
            new_root.is_leaf = false;
            new_root.n = 1;
            new_root.keys[0] = new_sibling.keys[0];
            new_root.children[0] = root_pos;
            new_root.children[1] = new_child;

            root_pos = allocate_node();
            write_node(root_pos, new_root);
        }
    }

    vector<int> find(const char* key) {
        vector<int> result;

        // Navigate to leftmost leaf containing key
        int pos = root_pos;
        Node node = read_node(pos);

        while (!node.is_leaf) {
            int i = 0;
            while (i < node.n && strcmp(key, node.keys[i].key) >= 0) {
                i++;
            }
            pos = node.children[i];
            node = read_node(pos);
        }

        // Search in leaf nodes (may span multiple leaves)
        bool found_in_prev = false;
        while (pos != -1) {
            bool found_in_this = false;

            for (int i = 0; i < node.n; i++) {
                int cmp = strcmp(node.keys[i].key, key);
                if (cmp == 0) {
                    result.push_back(node.keys[i].value);
                    found_in_this = true;
                } else if (cmp > 0) {
                    // Keys are sorted, no more matches possible
                    sort(result.begin(), result.end());
                    return result;
                }
            }

            if (!found_in_this && found_in_prev) {
                // No match in this leaf, but we found in previous, so we're done
                break;
            }

            found_in_prev = found_in_this;
            pos = node.next;
            if (pos != -1) {
                node = read_node(pos);
            }
        }

        sort(result.begin(), result.end());
        return result;
    }

    void remove(const char* key, int value) {
        // Navigate to leaf
        int pos = root_pos;
        Node node = read_node(pos);

        while (!node.is_leaf) {
            int i = 0;
            while (i < node.n && strcmp(key, node.keys[i].key) >= 0) {
                i++;
            }
            pos = node.children[i];
            node = read_node(pos);
        }

        // Remove from leaf
        for (int i = 0; i < node.n; i++) {
            if (strcmp(node.keys[i].key, key) == 0 && node.keys[i].value == value) {
                for (int j = i; j < node.n - 1; j++) {
                    node.keys[j] = node.keys[j + 1];
                }
                node.n--;
                write_node(pos, node);
                return;
            }
        }
    }
};

#endif
