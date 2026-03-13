#ifndef BPTREE_HPP
#define BPTREE_HPP

#include <fstream>
#include <cstring>
#include <vector>
#include <algorithm>

using namespace std;

const int MAX_KEY_SIZE = 65;
const int ORDER = 200;  // B+ tree order
const int MIN_KEYS = ORDER / 2;

struct KeyValue {
    char key[MAX_KEY_SIZE];
    int value;

    KeyValue() { memset(key, 0, sizeof(key)); value = 0; }
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

class BPlusTree {
private:
    struct Node {
        bool is_leaf;
        int num_entries;
        KeyValue entries[ORDER];
        int children[ORDER + 1];  // positions in file
        int next;  // next leaf node

        Node() : is_leaf(true), num_entries(0), next(-1) {
            for (int i = 0; i <= ORDER; i++) children[i] = -1;
        }
    };

    fstream file;
    string filename;
    int root_pos;
    int file_size;

    void write_header() {
        file.seekp(0);
        file.write(reinterpret_cast<char*>(&root_pos), sizeof(int));
        file.write(reinterpret_cast<char*>(&file_size), sizeof(int));
        file.flush();
    }

    void read_header() {
        file.seekg(0);
        file.read(reinterpret_cast<char*>(&root_pos), sizeof(int));
        file.read(reinterpret_cast<char*>(&file_size), sizeof(int));
    }

    int allocate_node() {
        int pos = file_size;
        file_size += sizeof(Node);
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

    int find_child_index(const Node& node, const KeyValue& kv) {
        int i = 0;
        // Go to right child if kv.key > entry.key
        while (i < node.num_entries && kv.compare_key(node.entries[i].key) > 0) {
            i++;
        }
        return i;
    }

    void split_node(int parent_pos, int child_index, int child_pos) {
        Node parent = read_node(parent_pos);
        Node child = read_node(child_pos);

        Node new_node;
        new_node.is_leaf = child.is_leaf;

        int mid = ORDER / 2;

        // Move half entries to new node
        for (int i = mid; i < child.num_entries; i++) {
            new_node.entries[i - mid] = child.entries[i];
            if (!child.is_leaf) {
                new_node.children[i - mid] = child.children[i];
            }
        }
        if (!child.is_leaf) {
            new_node.children[child.num_entries - mid] = child.children[child.num_entries];
        }

        new_node.num_entries = child.num_entries - mid;
        child.num_entries = mid;

        if (child.is_leaf) {
            new_node.next = child.next;
            child.next = allocate_node();
        }

        // Insert into parent
        for (int i = parent.num_entries; i > child_index; i--) {
            parent.entries[i] = parent.entries[i - 1];
            parent.children[i + 1] = parent.children[i];
        }

        parent.entries[child_index] = new_node.entries[0];
        int new_node_pos = child.is_leaf ? child.next : allocate_node();
        parent.children[child_index + 1] = new_node_pos;
        parent.num_entries++;

        write_node(child_pos, child);
        write_node(new_node_pos, new_node);
        write_node(parent_pos, parent);
    }

    void insert_non_full(int pos, const KeyValue& kv) {
        Node node = read_node(pos);

        if (node.is_leaf) {
            // Insert into leaf
            int i = node.num_entries - 1;
            while (i >= 0 && kv < node.entries[i]) {
                node.entries[i + 1] = node.entries[i];
                i--;
            }
            node.entries[i + 1] = kv;
            node.num_entries++;
            write_node(pos, node);
        } else {
            // Find child
            int i = find_child_index(node, kv);

            Node child = read_node(node.children[i]);
            if (child.num_entries >= ORDER) {
                split_node(pos, i, node.children[i]);
                node = read_node(pos);
                if (kv.compare_key(node.entries[i].key) > 0) {
                    i++;
                }
            }
            insert_non_full(node.children[i], kv);
        }
    }

public:
    BPlusTree(const string& fname) : filename(fname) {
        file.open(filename, ios::in | ios::out | ios::binary);

        if (!file.is_open() || file.peek() == EOF) {
            // Create new file
            file.close();
            file.open(filename, ios::out | ios::binary);
            file.close();
            file.open(filename, ios::in | ios::out | ios::binary);

            root_pos = 2 * sizeof(int);
            file_size = root_pos + sizeof(Node);

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
        Node root = read_node(root_pos);

        if (root.num_entries >= ORDER) {
            // Split root
            Node new_root;
            new_root.is_leaf = false;
            new_root.num_entries = 0;
            new_root.children[0] = root_pos;

            int old_root_pos = root_pos;
            root_pos = allocate_node();

            write_node(root_pos, new_root);
            split_node(root_pos, 0, old_root_pos);
        }

        insert_non_full(root_pos, kv);
    }

    vector<int> find(const char* key) {
        vector<int> result;
        int pos = root_pos;
        Node node = read_node(pos);

        // Navigate to leftmost leaf that could contain the key
        while (!node.is_leaf) {
            int i = 0;
            // Go left when key <= entry.key (to find leftmost occurrence)
            while (i < node.num_entries && strcmp(key, node.entries[i].key) > 0) {
                i++;
            }
            pos = node.children[i];
            node = read_node(pos);
        }

        // Collect all values with matching key from leaf chain
        while (pos != -1) {
            bool found_in_this_leaf = false;

            for (int i = 0; i < node.num_entries; i++) {
                int cmp = strcmp(node.entries[i].key, key);
                if (cmp == 0) {
                    result.push_back(node.entries[i].value);
                    found_in_this_leaf = true;
                } else if (cmp > 0) {
                    // Keys are sorted, no more matches possible
                    sort(result.begin(), result.end());
                    return result;
                }
            }

            // Decision: continue to next leaf?
            if (found_in_this_leaf) {
                // Found matches, there might be more in next leaf
                pos = node.next;
                if (pos != -1) {
                    node = read_node(pos);
                } else {
                    break;
                }
            } else if (result.empty() && node.num_entries > 0 &&
                       strcmp(node.entries[node.num_entries - 1].key, key) < 0) {
                // No matches yet, but last entry < key, so key might be in next leaf
                pos = node.next;
                if (pos != -1) {
                    node = read_node(pos);
                } else {
                    break;
                }
            } else {
                // No matches and we're past where key would be
                break;
            }
        }

        sort(result.begin(), result.end());
        return result;
    }

    void remove(const char* key, int value) {
        // Simple deletion: find and remove from leaf
        int pos = root_pos;
        Node node = read_node(pos);

        // Navigate to leftmost leaf that could contain the key
        while (!node.is_leaf) {
            int i = 0;
            while (i < node.num_entries && strcmp(key, node.entries[i].key) > 0) {
                i++;
            }
            pos = node.children[i];
            node = read_node(pos);
        }

        // Search through leaf chain
        while (pos != -1) {
            for (int i = 0; i < node.num_entries; i++) {
                int cmp = strcmp(node.entries[i].key, key);
                if (cmp == 0 && node.entries[i].value == value) {
                    // Found it, remove
                    for (int j = i; j < node.num_entries - 1; j++) {
                        node.entries[j] = node.entries[j + 1];
                    }
                    node.num_entries--;
                    write_node(pos, node);
                    return;
                } else if (cmp > 0) {
                    // Past the key, not found
                    return;
                }
            }
            pos = node.next;
            if (pos != -1) {
                node = read_node(pos);
            }
        }
    }
};

#endif
