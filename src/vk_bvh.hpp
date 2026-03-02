#ifndef VK_BVH_HPP
#define VK_BVH_HPP

#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <mutex>

namespace vke {

template <class T>
struct SPNode {
public:
    SPNode(SPNode*parent = nullptr) : volume(T()), depth(0), id(0), parent(parent) {}

    SPNode(const T& volume, unsigned int depth, unsigned int id, SPNode *parent = nullptr) :
        volume(volume), depth(depth), id(id), parent(parent)
    {
    }

    SPNode(const SPNode &other) :
        mtx(), volume(other.volume), depth(other.depth), id(other.id), parent(other.parent)
    {
    }

    SPNode& operator=(const SPNode &other)
    {
        this->volume = (other.volume);
        this->depth = (other.depth);
        this->id = (other.id);
        this->parent = (other.parent);
        this->children = (other.children);
        this->items = (other.items);

        return *this;
    }

    size_t childrenItemsCount()
    {
        size_t count = 0;

        for (auto &child : children)
            count += child.items.size();

        return count;
    }

    void print()
    {
        for(size_t i = 0; i < depth; ++i)
            std::cout << "-";
        std::cout << "> Node " << id << " (depth " << depth << ") ";

        for(size_t item : items)
            std::cout << item << " ";
        std::cout << "\n";

        for(SPNode<T> &child : children)
            child.print();
    }

    std::mutex mtx;

    T volume;

    std::vector<SPNode> children;
    std::set<size_t> items;

    unsigned int depth;
    unsigned int id;

    SPNode *parent;
};

template <class T>
struct SPItem {
    SPItem() {}
    SPItem(const T& volume) : volume(volume) {}

    T volume;
    std::map<unsigned int, SPNode<T>*> address;
};

template <class T>
inline void countNodes(const SPNode<T>& node, size_t &count)
{
    count += node.children.size();

    for (auto &child : node.children)
        countNodes(child, count);
}

template <class T>
inline void countItems(const SPNode<T>& node, size_t &count)
{
    count += node.items.size();

    for (auto &child : node.children)
        countItems(child, count);
}

template <class T>
class SPTree {
public:
    SPTree() : root(T(), 0, 0)
    {
    }

    SPTree(const T& volume, unsigned int maxBinSize, unsigned int maxDepth) :
        root(volume, 0, 0), maxBinSize(maxBinSize), maxDepth(maxDepth)
    {
    }

    ~SPTree()
    {
        root.children.clear();
        items.clear();
    }

    void insert(size_t id, const T &volume)
    {
        items[id].volume = volume;
        recursiveInsert(&root, id, volume);
    }

    void remove(size_t id)
    {
        // remove from current nodes
        auto it = items.find(id);

        if (it == items.end())
            return;

        for (auto& kv : it->second.address) {
            std::lock_guard<std::mutex> lock(kv.second->mtx);
            kv.second->items.erase(id);
        }

        items.erase(it);
    }

    std::vector<SPNode<T>*> nodes()
    {
        std::vector<SPNode<T>*> list;

        getNodes(root, list);

        return list;
    }

    void getNodes(SPNode<T>& node, std::vector<SPNode<T>*> &list)
    {
        list.push_back(&node);

        for (auto &child : node.children)
            getNodes(child, list);
    }

    void cleanupNode(SPNode<T>*node)
    {
        if (!node)
            return;

        size_t count = 0;
        countItems(*node, count);

        if (count == 0) {
            node->children.clear();
            cleanupNode(node->parent);
        }
    }

    void update(size_t id, const T &volume)
    {
        // remove from current nodes
        auto it = items.find(id);

        if (it != items.end()) {
            for (auto addressIt = it->second.address.begin(); addressIt != it->second.address.end();) {
                auto &kv = *addressIt;

                if(!volume.intersect(kv.second->volume)) {

                    std::lock_guard<std::mutex> lock(kv.second->mtx);

                        kv.second->items.erase(id);
                        if (kv.second->items.empty() && kv.second->parent) {
                            nodesToClean.insert(kv.second->parent);
                        }

                    it->second.address.erase(addressIt++);
                } else {
                    ++addressIt;
                }
            }
        }

        insert(id, volume);
    }

    void removeEmptyNodes()
    {
        for (auto &node : nodesToClean)
            cleanupNode(node);

        nodesToClean.clear();
    }

    template <class C>
    std::vector<size_t> neighbors(const C &thing)
    {
        std::vector<size_t> list;
        recursiveSearch<C>(&root, thing, list);

        return list;
    }

    template <class C>
    std::set<size_t> uniqueNeighbors(const C &thing)
    {
        std::set<size_t> list;
        recursiveUniqueSearch<C>(&root, thing, list);

        return list;
    }

    void print()
    {
        root.print();
    }

    size_t nodesCount()
    {
        size_t count = 0;
        countNodes(root, count);

        return count;
    }

    size_t itemsCount()
    {
        size_t count = 0;
        countItems(root, count);

        return count;
    }

    bool isOutOfBounds(size_t id)
    {
        auto it = items.find(id);

        if (it != items.end()) {
            if (it->second.volume.intersect(root.volume))
                return false;
        }

        return true;
    }

    SPNode<T> root;

private:
    void recursiveInsert(SPNode<T> *node, size_t id, const T &volume)
    {
        if(!node->volume.intersect(volume))
            return;

        // only insert in leafs
        if(!node->children.size()) {
            {
                std::lock_guard<std::mutex> lock(node->mtx);
                node->items.insert(id);
            }

            items[id].address.insert(std::make_pair(node->id, node));

            // try to flush leaf if full
            unsigned int depth = node->depth + 1;

            {
                std::lock_guard<std::mutex> lock(node->mtx);

                if(node->items.size() > maxBinSize && depth < maxDepth) {
                    std::vector<T> subdiv = node->volume.subdivide();

                    for(const T& volume : subdiv)
                        node->children.push_back(SPNode<T>(volume, depth, nextNodeId++, node));

                    for(size_t itemId : node->items) {
                        SPItem<T> *item = &items[itemId];

                        for(SPNode<T> &child : node->children)
                            recursiveInsert(&child, itemId, item->volume);

                        item->address.erase(node->id);
                    }

                    node->items.clear();

                    return;
                }
            }

        }

        for(SPNode<T> &child : node->children)
            recursiveInsert(&child, id, volume);
    }

    template <class C>
    void recursiveSearch(SPNode<T> *node, const C &thing, std::vector<size_t> &list)
    {
        if(!node->volume.intersect(thing))
            return;

        std::copy(node->items.begin(), node->items.end(), std::back_inserter(list));

        for(SPNode<T> &child : node->children)
            recursiveSearch<C>(&child, thing, list);
    }

    template <class C>
    void recursiveUniqueSearch(SPNode<T> *node, const C &thing, std::set<size_t> &list)
    {
        if(!node->volume.intersect(thing))
            return;

        for (auto &item : node->items)
            list.insert(item);

        for(SPNode<T> &child : node->children)
            recursiveUniqueSearch<C>(&child, thing, list);
    }

    unsigned int nextNodeId = 1;
    unsigned int maxBinSize = 0;
    unsigned int maxDepth = 0;

    std::map<size_t, SPItem<T>> items;
    std::set<SPNode<T>*> nodesToClean;
};

#include "vk_volume.hpp"

typedef SPTree<vke::Volume> Octree;
typedef SPTree<vke::Box> Quadtree;

} // end namespace vke

#endif // VK_BVH_HPP
