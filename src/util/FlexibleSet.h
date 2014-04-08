#ifndef FLEXIBLESET_H_
#define FLEXIBLESET_H_

#include "dependencies/mtwist.h"
#include "lcommon/typename.h"
#include "lcommon/strformat.h"

#include <google/sparse_hash_set>

// Trivial hash, for google sparse hash:
struct Hasher {
    size_t operator()(int elem) const {
        return elem;
    }
};

// A set that dynamically picks its implementation based on the load.
template<typename T, typename HasherT = Hasher>
struct FlexibleSet {
    enum {
        INITIAL = 0, THRESHOLD = 128
    };

    ~FlexibleSet() {
        clear();
    }

    FlexibleSet(const FlexibleSet& set) = delete;
    FlexibleSet() {
        hash_impl = NULL;
        vector_impl.reserve(INITIAL);
    }

    struct iterator {
        typedef T value_type;
        int slot;
        T elem;
        iterator() :
                slot(0), elem(-1) {
        }
        T get() {
            DEBUG_CHECK(elem != -1, "Getting invalid element!")
            return elem;
        }
    };

    bool pick_random_uniform(MTwist& rng, T& elem) {
        if (UNLIKELY(empty())) {
            return false;
        }
        if (UNLIKELY(hash_impl)) {
            HashSet& elems = *hash_impl;
            int n_hash_slots = elems.rep.table.size();
            int idx;
            while (true) {
                // We will loop until we find a hash-slot that
                // contains a valid instance of 'ElemT'.
                idx = rng.rand_int(n_hash_slots);
                if (elems.rep.table.test(idx)) {
                    elem = elems.rep.table.unsafe_get(idx);
                    if (elem != -1) {
                        return true;
                    }
                }
            }
            ASSERT(false, "Should be unreachable!");
            return false;
        } else {
            int idx = rng.rand_int(vector_impl.size());
            elem = vector_impl[idx];
            return true;
        }
    }

    void print() {
        if (hash_impl) {
            HashSet& elems = *hash_impl;
            printf("{");
            typename HashSet::iterator it = elems.begin();
            for (; it != elems.end(); ++it) {
                printf("%d ", *it);
            }
            printf("}\n");
        } else {
            printf("[");
            typename std::vector<T>::iterator it = vector_impl.begin();
            for (; it != vector_impl.end(); ++it) {
                printf("%d ", *it);
            }
            printf("]\n");
        }
    }
    bool iterate(iterator& iter) {
        if (UNLIKELY(hash_impl)) {
            HashSet& elems = *hash_impl;
            int n_hash_slots = elems.rep.table.size();

            bool found_element = false;
            // Find next available slot
            while (!found_element) {
                if (iter.slot >= n_hash_slots) {
                    // No more elements
                    return false;
                }
                // If not at end, we will loop until we find a hash-slot that
                // contains a valid instance of 'ElemT'.
                if (elems.rep.table.test(iter.slot)) {
                    T elem = elems.rep.table.unsafe_get(iter.slot);
                    if (elem != -1) {
                        iter.elem = elem;
                        found_element = true;
                    }
                }
                iter.slot++;
            }
        } else {
            if (iter.slot >= vector_impl.size()) {
                return false;
            }
            iter.elem = vector_impl[iter.slot];
            iter.slot++;
        }
        return true;
    }

    bool erase(const T& elem) {
        if (UNLIKELY(hash_impl)) {
            return hash_impl->erase(elem);
        }
        // Must scan for element, linearly
        // Luckily, we are below THRESHOLD, and thus should be fairly small
        const int nElem = vector_impl.size();
        for (int i = 0; i < nElem; i++) {
            // Individual case unlikely -- must happen once, though, typically.
            if (UNLIKELY(vector_impl[i] == elem)) {
                vector_impl.erase(vector_impl.begin() + i);
                return true;
            }
        }
        return false;
    }
    bool insert(const T& elem) {
        DEBUG_CHECK(elem != -1, "Inserting invalid element!");

        if (UNLIKELY(vector_impl.size() > THRESHOLD)) {
            switch_to_hash_set();
        }
        if (UNLIKELY(hash_impl)) {
            HashSet& elems = *hash_impl;
            size_t prev_size = elems.size();
            elems.insert(elem);
            return (elems.size() > prev_size);
        }
        // Must scan for element, linearly
        // Luckily, we are below THRESHOLD, and thus should be fairly small
        for (T& data : vector_impl) {
            if (UNLIKELY(data == elem)) {
                // Already in our set!
                return false;
            }
        }
        // Otherwise, simply append to our vector:
        vector_impl.push_back(elem);
        return true;
    }
    bool empty() const {
        return hash_impl ? hash_impl->empty() : vector_impl.empty();
    }
    size_t size() const {
        return hash_impl ? hash_impl->size() : vector_impl.size();
    }
    void clear() {
        delete hash_impl;
        hash_impl = NULL;
        vector_impl = std::vector<T>();
    }
private:
    // If we pass THRESHOLD, we should switch to a different implementation
    void switch_to_hash_set() {
        hash_impl = new HashSet(size() * 2);
        // MUST be done to use erase() with Google's HashSet:
        hash_impl->set_deleted_key((T) -1);
        for (T& val : vector_impl) {
            hash_impl->insert(val);
        }
        // Ensure our vector's memory is reclaimed:
        vector_impl = std::vector<T>();
    }
    typedef google::sparse_hash_set<T, HasherT> HashSet;
    HashSet* hash_impl; // If NULL, use vector_impl
    std::vector<T> vector_impl;
};

#endif
