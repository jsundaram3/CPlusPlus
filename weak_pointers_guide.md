# Weak Pointers — A Practical Guide

## 1. What Is a `weak_ptr`?

A `std::weak_ptr` (defined in `<memory>`) is a **smart pointer** that **observes** but does **not own** the object managed by a `std::shared_ptr`. It does not participate in reference counting — it cannot create, destroy, or independently control the lifetime of the managed object.

Key properties:
- **Non-owning observer** — does not increment or decrement the reference count
- **Cannot be used directly** — must be converted to a `shared_ptr` via `.lock()` before accessing the object
- **Expired-aware** — can detect when the observed object has been destroyed
- **Copyable and assignable** — copying a `weak_ptr` does not affect reference counts
- **Thread-safe for lock/expire** — checking expiration and locking is atomic

```cpp
// === weak_ptr version (observes without owning) ===
{
    auto strongPtr = std::make_shared<int>(42);
    std::weak_ptr<int> weakPtr = strongPtr;  // observe, don't own

    std::cout << strongPtr.use_count();  // prints: 1 (weak_ptr doesn't count)

    // Must lock to access the object
    if (auto locked = weakPtr.lock()) {
        std::cout << *locked << "\n";  // prints: 42
    }

    // strongPtr still alive — lock() returns a valid shared_ptr
}  // strongPtr destroyed here — object is freed

// weakPtr is now expired — lock() returns an empty shared_ptr
if (auto locked = weakPtr.lock()) {
    // This block is NOT entered — object is gone
} else {
    std::cout << "Object has been destroyed\n";
}

// === Raw pointer equivalent (no safety, no expiration check) ===
{
    int* rawPtr = new int(42);

    int* observer = rawPtr;  // just a raw pointer — no ownership info

    // Can you tell if the object is still alive? No!
    // If rawPtr is deleted, observer becomes a dangling pointer — UB on access!

    std::cout << *observer << "\n";  // prints: 42 — but for how long?

    delete rawPtr;  // observer is now dangling — accessing it is undefined behavior!
    // There is no built-in way to detect this!
}
```

---

## 2. Creating Weak Pointers

### 2.1 From a `shared_ptr` (the only valid source)

```cpp
// === weak_ptr created from shared_ptr ===
auto strongPtr = std::make_shared<std::string>("hello");
std::weak_ptr<std::string> weakPtr = strongPtr;

// weakPtr observes strongPtr — use_count is still 1
std::cout << strongPtr.use_count();  // prints: 1

// === Raw pointer equivalent (no tracking possible) ===
std::string* rawPtr = new std::string("hello");
std::string* observer = rawPtr;
// No way to know if rawPtr is still valid or how many "observers" exist
```

### 2.2 Default Construction (empty weak_ptr)

```cpp
// === Default-constructed weak_ptr (observes nothing) ===
std::weak_ptr<int> emptyWeak;

// Cannot lock — will always return empty shared_ptr
if (auto locked = emptyWeak.lock()) {
    // Never entered
} else {
    std::cout << "Empty weak_ptr — nothing to observe\n";
}

// === Raw pointer equivalent ===
int* rawPtr = nullptr;
// nullptr is already "invalid" — no object to observe
```

### 2.3 Locking a `weak_ptr`

The `.lock()` method creates a `shared_ptr` from a `weak_ptr`, incrementing the reference count if the object is still alive:

```cpp
// === Locking a valid weak_ptr ===
auto strongPtr = std::make_shared<int>(42);
std::weak_ptr<int> weakPtr = strongPtr;

{
    std::shared_ptr<int> locked = weakPtr.lock();  // creates shared_ptr
    if (locked) {
        std::cout << *locked << "\n";  // prints: 42
    }
}  // locked shared_ptr destroyed — ref count drops back to 1

// === Locking an expired weak_ptr ===
auto strongPtr2 = std::make_shared<int>(99);
std::weak_ptr<int> weakPtr2 = strongPtr2;

strongPtr2.reset();  // object destroyed — weakPtr2 is now expired

auto locked2 = weakPtr2.lock();  // returns empty shared_ptr
if (!locked2) {
    std::cout << "weak_ptr expired — object no longer exists\n";
}

// === Raw pointer equivalent (no lock mechanism — dangling pointer risk) ===
int* rawPtr = new int(42);
int* observer = rawPtr;

// No mechanism to "lock" — just access directly
std::cout << *observer << "\n";  // prints: 42

delete rawPtr;  // observer is now dangling
// No way to detect this — accessing *observer is UB!
```

---

## 3. Common Use Cases

### 3.1 Breaking Circular References

The most common use case for `weak_ptr` is breaking circular references between `shared_ptr`:

```cpp
// === Problem — circular reference with shared_ptr ===
struct Child;
struct Parent;

struct Child {
    std::shared_ptr<Parent> parent;  // Child owns Parent
};

struct Parent {
    std::shared_ptr<Child> child;    // Parent owns Child — CIRCULAR!
};

auto parent = std::make_shared<Parent>();
auto child = std::make_shared<Child>();
parent->child = child;
child->parent = parent;

// use_count(Parent) = 2 (parent variable + child.parent)
// use_count(Child) = 2 (child variable + parent.child)
// When both go out of scope, ref counts never reach 0 — MEMORY LEAK!

// === Solution — use weak_ptr for back-reference ===
struct ChildFixed;
struct ParentFixed;

struct ChildFixed {
    std::weak_ptr<ParentFixed> parent;  // observes, doesn't own
};

struct ParentFixed {
    std::shared_ptr<ChildFixed> child;  // owns child
};

auto parent2 = std::make_shared<ParentFixed>();
auto child2 = std::make_shared<ChildFixed>();
parent2->child = child2;
child2->parent = parent2;  // doesn't increment ref count

// use_count(ParentFixed) = 1 (only parent2 variable)
// use_count(ChildFixed) = 1 (only child2 variable)
// When both go out of scope, ref counts reach 0 — objects are freed!
```

### 3.2 Observer Pattern

```cpp
// === weak_ptr for observer — doesn't prevent cleanup ===
class DataModel {
public:
    std::shared_ptr<std::string> data;
};

class Observer {
    std::weak_ptr<DataModel> modelRef;  // observe, don't own
public:
    void updateModel(std::shared_ptr<DataModel> model) {
        modelRef = model;  // just observe
    }

    void display() {
        if (auto model = modelRef.lock()) {
            if (model->data) {
                std::cout << *model->data << "\n";
            }
        } else {
            std::cout << "Model has been destroyed\n";
        }
    }
};

// The model can be destroyed independently — observers handle it gracefully
auto model = std::make_shared<DataModel>();
model->data = std::make_shared<std::string>("Hello");

Observer obs;
obs.updateModel(model);
obs.display();  // prints: Hello

model.reset();  // model destroyed
obs.display();  // prints: Model has been destroyed (no crash!)
```

### 3.3 Cache with Expiration

```cpp
// === weak_ptr for cache — entries expire when no longer referenced ===
class CacheEntry {
public:
    std::string value;
    int timestamp;
};

std::weak_ptr<CacheEntry> cache;

std::shared_ptr<CacheEntry> getOrCreate(const std::string& key) {
    if (auto entry = cache.lock()) {
        // Entry still exists — return it
        return entry;
    }
    // Entry expired — create new one
    auto newEntry = std::make_shared<CacheEntry>();
    newEntry->value = "data_for_" + key;
    newEntry->timestamp = 12345;
    cache = newEntry;  // store as weak_ptr
    return newEntry;
}

// First call — creates entry
auto entry1 = getOrCreate("user1");

// Second call — returns cached entry (same object)
auto entry2 = getOrCreate("user1");
std::cout << entry1.use_count();  // prints: 2 (entry1 + entry2)

// When all strong references are gone, cache entry expires
entry1.reset();
entry2.reset();

// Third call — cache expired, creates new entry
auto entry3 = getOrCreate("user1");
std::cout << entry3.use_count();  // prints: 1
```

### 3.4 Long-Running Tasks with Safety

```cpp
// === weak_ptr ensures task doesn't keep object alive indefinitely ===
class Task {
    std::weak_ptr<int> dataRef;  // observe, don't own
public:
    void setData(std::shared_ptr<int> data) {
        dataRef = data;
    }

    void execute() {
        if (auto data = dataRef.lock()) {
            std::cout << "Processing: " << *data << "\n";
        } else {
            std::cout << "Data expired — skipping task\n";
        }
    }
};

auto data = std::make_shared<int>(42);
Task task;
task.setData(data);

// Simulate data being destroyed before task completes
data.reset();

// Task handles expiration gracefully
task.execute();  // prints: Data expired — skipping task
```

---

## 4. Accessing the Managed Object

```cpp
// === weak_ptr version ===
auto strongPtr = std::make_shared<MyClass>();
std::weak_ptr<MyClass> weakPtr = strongPtr;

// Method 1: lock() — returns shared_ptr (preferred)
if (auto locked = weakPtr.lock()) {
    locked->someMethod();  // use the shared_ptr
}

// Method 2: lock() with value
std::shared_ptr<MyClass> locked = weakPtr.lock();
if (locked) {
    locked->someMethod();
}

// Method 3: Check expiration first
if (!weakPtr.expired()) {
    auto locked = weakPtr.lock();
    locked->someMethod();
}

// === Raw pointer equivalent (no safety) ===
MyClass* rawPtr = new MyClass();
MyClass* observer = rawPtr;

// No lock mechanism — just access directly (risky!)
observer->someMethod();

// No expired() check — you can't tell if rawPtr is valid
// If rawPtr is deleted, observer is dangling — UB!
delete rawPtr;
```

> ⚠️ **Never** dereference a `weak_ptr` directly. You **must** use `.lock()` to obtain a `shared_ptr` first.

---

## 5. Checking Expiration

```cpp
// === weak_ptr version ===
auto strongPtr = std::make_shared<int>(42);
std::weak_ptr<int> weakPtr = strongPtr;

// Check if the observed object is still alive
if (!weakPtr.expired()) {
    std::cout << "Object is still alive\n";
}

// Destroy the object
strongPtr.reset();

if (weakPtr.expired()) {
    std::cout << "Object has been destroyed\n";
}

// === Raw pointer equivalent (no expiration check possible) ===
int* rawPtr = new int(42);
int* observer = rawPtr;

// No way to check if rawPtr is still valid!
// You'd have to manually track:
bool isAlive = true;  // you must manage this yourself

delete rawPtr;
isAlive = false;  // must remember to update

// If you forget to update isAlive, you might access a dangling pointer!
```

---

## 6. Comparing and Assigning

```cpp
// === weak_ptr comparison and assignment ===
auto strongPtr1 = std::make_shared<int>(42);
auto strongPtr2 = std::make_shared<int>(99);
std::weak_ptr<int> weakPtr1 = strongPtr1;
std::weak_ptr<int> weakPtr2 = strongPtr2;

// Assignment
weakPtr1 = weakPtr2;  // now observes strongPtr2
// No effect on reference counts

// Comparison
if (weakPtr1 == weakPtr2) {
    std::cout << "Both observe the same object\n";
}

if (weakPtr1 != strongPtr1) {
    // weak_ptr can be compared with shared_ptr
    std::cout << "Different objects\n";
}

// === Raw pointer equivalent ===
int* raw1 = new int(42);
int* raw2 = new int(99);
int* observer1 = raw1;
int* observer2 = raw2;

observer1 = observer2;  // now points to raw2
delete raw1;  // observer1 was pointing to raw1 — dangling!

if (observer1 == observer2) {
    std::cout << "Both point to same object\n";
}

delete raw2;
```

---

## 7. Common Pitfalls & Best Practices

### ❌ Don't use `weak_ptr` without `shared_ptr`

```cpp
// === Bad — weak_ptr observing nothing meaningful ===
int* raw = new int(42);
// std::weak_ptr<int> badWeak(raw);  // COMPILE ERROR — can't construct from raw!

// === Correct — always derive from shared_ptr ===
auto strongPtr = std::make_shared<int>(42);
std::weak_ptr<int> goodWeak = strongPtr;
```

### ❌ Don't forget to lock before use

```cpp
// === Bad — trying to use weak_ptr directly ===
std::weak_ptr<MyClass> weakPtr;
// weakPtr->someMethod();  // COMPILE ERROR — weak_ptr has no -> operator!

// === Correct — lock first ===
if (auto locked = weakPtr.lock()) {
    locked->someMethod();  // use the shared_ptr
}
```

### ✅ Always check for expiration in multi-threaded code

```cpp
// === Good — lock is atomic with respect to expiration ===
std::weak_ptr<MyClass> weakPtr;

void safeAccess() {
    auto locked = weakPtr.lock();  // atomic: checks expiration + increments ref count
    if (locked) {
        // locked is guaranteed valid for the lifetime of this shared_ptr
        locked->someMethod();
    }
    // locked destroyed here — ref count decremented
}

// === Bad — non-atomic check-then-act ===
void unsafeAccess() {
    if (!weakPtr.expired()) {
        // Between expired() check and lock(), another thread could destroy the object!
        auto locked = weakPtr.lock();  // might return empty shared_ptr!
        locked->someMethod();  // potential use-after-free if locked is empty!
    }
}
```

### ✅ Use `weak_ptr` for optional back-references

```cpp
// === Good — weak_ptr for parent reference (no ownership) ===
class Node {
    std::shared_ptr<Node> children;
    std::weak_ptr<Node> parent;  // observes parent, doesn't own
};

// === Bad — shared_ptr for parent reference (circular reference) ===
class BadNode {
    std::shared_ptr<BadNode> children;
    std::shared_ptr<BadNode> parent;  // CIRCULAR REFERENCE!
};
```

### ✅ Prefer `lock()` over `expired()` + `lock()`

```cpp
// === Good — single atomic operation ===
if (auto locked = weakPtr.lock()) {
    locked->someMethod();
}

// === Avoid — two separate operations (race condition in multi-threaded code) ===
if (!weakPtr.expired()) {
    auto locked = weakPtr.lock();
    if (locked) {
        locked->someMethod();
    }
}
```

### ❌ Don't use `weak_ptr` as a replacement for raw pointers

```cpp
// === Bad — weak_ptr for simple observation ===
void process(MyClass* obj) {
    std::weak_ptr<MyClass> weak(obj);  // COMPILE ERROR — can't construct from raw!
}

// === Good — raw pointer for simple observation ===
void process(MyClass* obj) {
    // Use raw pointer — no need for weak_ptr if there's no shared_ptr to observe
    obj->someMethod();
}
```

---

## 8. When to Use `weak_ptr` vs Other Options

| Scenario | Recommended Type |
|---|---|
| Simple observation (no ownership) | Raw pointer `T*` or `T&` |
| Observing a `shared_ptr` without owning | `std::weak_ptr<T>` |
| Optional back-reference (break cycles) | `std::weak_ptr<T>` |
| Cache with expiration | `std::weak_ptr<T>` |
| Observer pattern with shared objects | `std::weak_ptr<T>` |
| Task that may outlive its data | `std::weak_ptr<T>` |
| Guaranteed non-null reference | `T&` |
| May be null reference | Raw pointer `T*` |

### Rule of thumb:
1. **Use raw pointers** for simple, non-owning observations
2. **Use `weak_ptr`** when observing objects managed by `shared_ptr`
3. **Use `weak_ptr`** to break circular references in tree/graph structures
4. **Use `weak_ptr`** when you need to know if the observed object has been destroyed

---

## 9. Performance Characteristics

| Aspect | `weak_ptr` | `shared_ptr` | Raw Pointer |
|---|---|---|---|
| Size | Same as `shared_ptr` (~2x raw pointer) | ~2x raw pointer | 1x raw pointer |
| Construction | From `shared_ptr` only | From `new` or `make_shared` | Direct |
| Lock cost | Atomic ref count increment | None (already owned) | None |
| Expired check | Read control block flag | N/A | N/A |
| Thread safety | `lock()` and `expired()` are atomic | Ref count is atomic | Not thread-safe |
| Memory overhead | Shares control block with `shared_ptr` | Control block + object | None |

`weak_ptr` has **similar size** to `shared_ptr` because it shares the same control block. The `.lock()` operation involves an atomic increment, which has a small cost but is generally negligible.

---

## 10. Quick Reference Cheat Sheet

```cpp
// === Creation ===
auto strongPtr = std::make_shared<T>(args...);
std::weak_ptr<T> weakPtr = strongPtr;
// Raw: T* ptr = new T(args...); T* observer = ptr;

// === Locking (convert to shared_ptr) ===
auto locked = weakPtr.lock();  // returns shared_ptr<T>
if (locked) {
    locked->method();  // use the shared_ptr
}
// Raw: observer->method(); (no safety check possible)

// === Check expiration ===
if (weakPtr.expired()) {
    // Object has been destroyed
}
// Raw: No equivalent — no way to check validity

// === Assignment ===
weakPtr = strongPtr;  // now observes strongPtr
// Raw: observer = ptr; (no ownership semantics)

// === Comparison ===
if (weakPtr1 == weakPtr2) { /* same observed object */ }
if (weakPtr == strongPtr) { /* compares observed object */ }
// Raw: observer1 == observer2 (pointer comparison)

// === Reset (clears the weak_ptr) ===
weakPtr.reset();  // weak_ptr no longer observes anything
// Raw: observer = nullptr; (but no link to original)
```

---

## 11. Complete Example: Tree Structure with `weak_ptr`

```cpp
#include <iostream>
#include <memory>
#include <vector>

class TreeNode {
public:
    std::string name;
    std::shared_ptr<TreeNode> parent;       // ownership: parent owns children
    std::vector<std::shared_ptr<TreeNode>> children;  // owned by this node

    // For back-reference: child observes parent (no ownership)
    // Note: In practice, you'd store weak_ptr in a separate field
    std::weak_ptr<TreeNode> weakParent;

    TreeNode(const std::string& n) : name(n) {}

    void addChild(std::shared_ptr<TreeNode> child) {
        child->parent = shared_from_this();  // parent reference via shared_ptr
        child->weakParent = shared_from_this();  // also store as weak_ptr
        children.push_back(std::move(child));
    }

    void printTree(int indent = 0) {
        std::cout << std::string(indent, ' ') << name << "\n";
        for (const auto& child : children) {
            child->printTree(indent + 2);
        }
    }
};

// Usage:
// auto root = std::make_shared<TreeNode>("Root");
// auto child1 = std::make_shared<TreeNode>("Child1");
// auto child2 = std::make_shared<TreeNode>("Child2");
// root->addChild(child1);
// root->addChild(child2);
// root->printTree();
//
// Output:
// Root
//   Child1
//   Child2
//
// When root goes out of scope, the entire tree is freed.
// Children's weakParent references expire automatically.
```

---

## 12. Summary

| Feature | `weak_ptr` |
|---|---|
| **Ownership** | Non-owning observer |
| **Reference count** | Does not affect it |
| **Lifetime** | Tied to the observed `shared_ptr` |
| **Direct access** | Not allowed — must use `.lock()` |
| **Main use cases** | Breaking cycles, observers, caches, optional back-references |
| **Thread safety** | `lock()` and `expired()` are atomic |
| **Size** | Same as `shared_ptr` (shares control block) |

`weak_ptr` is the tool you reach for when you need to **observe** an object managed by `shared_ptr` **without** influencing its lifetime. It's essential for breaking circular references and implementing safe observer patterns in C++.