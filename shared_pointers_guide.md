# Shared Pointers — A Practical Guide

## 1. What Is a `shared_ptr`?

A `std::shared_ptr` (defined in `<memory>`) is a **smart pointer** that **shares ownership** of a dynamically allocated object. When the last `shared_ptr` owning the object is destroyed, the object is automatically deleted.

Key properties:
- **Shared ownership** — multiple `shared_ptr` instances can own the same object
- **Reference counting** — internally tracks how many `shared_ptr` instances share ownership
- **Copyable** — copying a `shared_ptr` increments the reference count
- **Automatic cleanup** — the object is deleted when the last `shared_ptr` is destroyed
- **Thread-safe reference counting** — incrementing/decrementing the count is atomic

```cpp
// === shared_ptr version (automatic cleanup with shared ownership) ===
{
    std::shared_ptr<int> ptr1 = std::make_shared<int>(42);
    std::cout << ptr1.use_count();  // prints: 1

    {
        std::shared_ptr<int> ptr2 = ptr1;  // copy — shares ownership
        std::cout << ptr1.use_count();     // prints: 2
        std::cout << ptr2.use_count();     // prints: 2
    }  // ptr2 destroyed, ref count drops to 1

    std::cout << ptr1.use_count();  // prints: 1
}  // ptr1 destroyed, ref count drops to 0 — int is freed here

// === Raw pointer equivalent (no shared ownership, manual cleanup) ===
{
    int* ptr1 = new int(42);
    // No built-in way to track how many pointers "own" this

    {
        int* ptr2 = ptr1;  // just another pointer — no ref count!
        // How many owners? You must track manually.
    }

    delete ptr1;  // must remember to delete — exactly once!
    // If you delete twice, undefined behavior (double free)!
    // If you forget, memory leak!
}
```

---

## 2. Creating Shared Pointers

### 2.1 Using `std::make_shared` (C++11, **preferred**)

```cpp
// === shared_ptr version ===
std::shared_ptr<int> num = std::make_shared<int>(42);
std::shared_ptr<std::string> str = std::make_shared<std::string>("hello");
std::shared_ptr<double> arr = std::make_shared<double>(3.14);

// === Raw pointer equivalent ===
int* num = new int(42);
std::string* str = new std::string("hello");
double* arr = new double(3.14);
// ... use them ...
delete num;
delete str;
delete arr;
```

### 2.2 Direct Construction (use with caution)

```cpp
// === shared_ptr wrapping a raw pointer ===
int* raw = new int(42);
std::shared_ptr<int> ptr(raw);  // shared_ptr now owns it

// === Raw pointer equivalent (you manage everything) ===
int* raw = new int(42);
// ... use raw ...
delete raw;  // must remember to delete
```

### 2.3 Managing Resources Other Than `new`/`delete`

```cpp
// === shared_ptr version (automatic fclose) ===
std::shared_ptr<FILE> file(
    fopen("data.txt", "r"),
    &fclose
);

if (file) {
    // use file.get() to access the raw FILE*
}
// fclose is called when the last shared_ptr goes out of scope

// === Raw pointer equivalent (manual fclose required) ===
FILE* file = fopen("data.txt", "r");

if (file) {
    // use file
}
fclose(file);  // must remember to close — or file descriptor leaks!
```

> 💡 **Tip:** `std::make_shared` is preferred over direct `new` because it performs a **single memory allocation** for both the object and the control block, whereas `std::shared_ptr<T>(new T(...))` requires **two allocations** (one for T, one for the control block).

---

## 3. Copying and Reference Counting

Since `shared_ptr` is copyable, copying shares ownership:

```cpp
// === shared_ptr version (ownership sharing via copy) ===
std::shared_ptr<int> ptr1 = std::make_shared<int>(42);

// Copy — both ptr1 and ptr2 now share ownership
std::shared_ptr<int> ptr2 = ptr1;

// Both report use_count of 2
std::cout << ptr1.use_count();  // prints 2
std::cout << ptr2.use_count();  // prints 2

// Destroying one doesn't delete the object
ptr1.reset();  // deletes nothing — ref count drops to 1, object survives

std::cout << ptr2.use_count();  // prints 1
std::cout << *ptr2 << "\n";     // prints 42 — still valid!

// Object is deleted only when the last shared_ptr is destroyed
ptr2.reset();  // ref count drops to 0 — int is now freed

// === Raw pointer equivalent (no reference counting — you track manually) ===
int* ptr1 = new int(42);
int* ownerCount = new int(1);  // manually track "owners"

// "Copy" — just another pointer, but you must manually update count
int* ptr2 = ptr1;
(*ownerCount)++;  // manually increment count

if (*ownerCount == 2) {
    std::cout << "Two owners\n";
}

// "Destroying" one — manually decrement, don't delete yet
ptr1 = nullptr;  // dangling — but object still alive via ptr2
(*ownerCount)--;

if (*ownerCount > 0) {
    std::cout << "Still " << *ownerCount << " owner(s)\n";
}

std::cout << *ptr2 << "\n";  // prints 42 — still valid

// Only delete when count reaches zero
if (--(*ownerCount) == 0) {
    delete ptr2;  // finally delete the object
}
delete ownerCount;  // don't forget to clean up the counter too!
```

### Common patterns:

```cpp
// === Returning from a function — shared ownership ===
std::shared_ptr<MyClass> createObject() {
    return std::make_shared<MyClass>();
}

// === Raw pointer equivalent (ownership ambiguous!) ===
MyClass* createObject() {
    return new MyClass();  // caller must know to delete!
}

// === Sharing with multiple functions ===
void useObject(std::shared_ptr<MyClass> obj) {
    // function shares ownership — object survives even if original is destroyed
}

std::shared_ptr<MyClass> globalPtr = std::make_shared<MyClass>();

useObject(globalPtr);  // ref count increases during call
useObject(globalPtr);  // ref count increases during call
// Object survives until globalPtr is destroyed — all calls shared safely

// === Raw pointer equivalent (no ownership semantics!) ===
MyClass* globalPtr = new MyClass();

useObject(globalPtr);  // who owns this? who deletes?
useObject(globalPtr);  // if original is deleted, these become dangling!
// Must manually track — error-prone!
```

### Scenarios for `shared_ptr` parameters:

```cpp
// 1. SHARE OWNERSHIP — function shares ownership, object survives beyond call
void shareOwnership(std::shared_ptr<MyClass> obj) {
    // function shares ownership; object survives after return
    // store in a cache, callback, or background task
}
// Usage: shareOwnership(mySharedPtr);

// 2. OBSERVER PATTERN — multiple observers share ownership
class Observer {
    std::shared_ptr<Data> data;
public:
    void onData(std::shared_ptr<Data> d) {
        data = d;  // observer keeps a copy — data lives as long as observer wants
    }
};

// 3. CACHING — cache holds shared_ptr, multiple consumers share
std::shared_ptr<HeavyObject> cache;

std::shared_ptr<HeavyObject> getCached() {
    if (!cache) {
        cache = std::make_shared<HeavyObject>();  // create once
    }
    return cache;  // return shared ownership — caller can keep it
}

// 4. POLYMORPHIC SHARING — shared polymorphic objects
std::shared_ptr<Base> createDerived() {
    return std::make_shared<Derived>();
}
// Usage: auto obj = createDerived(); // shared ownership, polymorphic

// 5. ASYNCHRONOUS OPERATIONS — object survives until async task completes
void startAsyncTask(std::shared_ptr<Context> ctx) {
    std::thread([ctx]() {
        // ctx keeps Context alive until this lambda completes
        // even if original Context is destroyed elsewhere
    }).detach();
}
```

---

## 4. Accessing the Managed Object

```cpp
// === shared_ptr version ===
std::shared_ptr<MyClass> ptr = std::make_shared<MyClass>();

// Dereference
*ptr;

// Access member
ptr->someMethod();

// Raw pointer (for C APIs, etc.)
ptr.get();

// Check if not empty
if (ptr) { /* ... */ }
if (ptr != nullptr) { /* ... */ }

// Reference count
std::cout << ptr.use_count();  // number of shared_ptrs sharing ownership

// === Raw pointer equivalent (same syntax for access) ===
MyClass* ptr = new MyClass();

*ptr;
ptr->someMethod();
// No .get() needed — ptr IS already a raw pointer

if (ptr) { /* ... */ }
if (ptr != nullptr) { /* ... */ }
// No use_count() — you'd need to track manually

delete ptr;  // don't forget!
```

> ⚠️ **Never** use `get()` to transfer ownership. It returns a non-owning raw pointer.
>
> 💡 **Tip:** `use_count()` is useful for debugging but **not** for logic — it is not atomic with respect to other operations, so its value may be stale by the time you use it.

---

## 5. Releasing and Resetting

```cpp
// === shared_ptr version ===
std::shared_ptr<int> ptr = std::make_shared<int>(42);

// Reset — decrements ref count, optionally replaces with new object
ptr.reset();           // decrements ref count; if zero, deletes the int
ptr.reset(new int(99));  // decrements old, creates new with 99
ptr.reset();           // decrements again

// Release — gives up ownership, returns raw pointer (caller must manage!)
int* raw = ptr.release();  // ptr is now nullptr
delete raw;               // manual cleanup required — avoid if possible

// === Raw pointer equivalent ===
int* ptr = new int(42);

// Reset equivalent
delete ptr;           // deletes the int, ptr still points to freed memory (dangling!)
ptr = new int(99);    // creates new with 99 (old one already deleted)
delete ptr;           // deletes the 99

// Note: after delete, ptr is a dangling pointer — accessing it is UB!
// shared_ptr automatically sets to nullptr after reset/release
```

---

## 6. Custom Deleters

A custom deleter tells `shared_ptr` how to clean up the managed object:

```cpp
// Lambda deleter
auto fileDeleter = [](FILE* f) {
    if (f) fclose(f);
};

std::shared_ptr<FILE> file(
    fopen("data.txt", "r"),
    fileDeleter
);

// Type-erased deleter (using std::function — has small runtime cost)
std::shared_ptr<int> smartPtr(
    new int(42),
    [](int* p) { delete p; }
);
```

> ⚠️ **Note:** Unlike `unique_ptr`, `shared_ptr`'s deleter type does **not** affect the `shared_ptr<T>` type itself (the deleter is stored in the control block). This means `shared_ptr<T>` with different deleters are the **same type**, making containers easier to work with.

---

## 7. Common Pitfalls & Best Practices

### ❌ Never create a raw pointer from the same object twice

```cpp
// === shared_ptr (dangerous — double free!) ===
int* raw = new int(42);
std::shared_ptr<int> ptr1(raw);
std::shared_ptr<int> ptr2(raw);  // TWO shared_ptrs think they own it — DOUBLE FREE!

// === Raw pointer equivalent (same problem) ===
int* raw = new int(42);
int* ptr1 = raw;
int* ptr2 = raw;
delete ptr1;  // deletes the memory
delete ptr2;  // DOUBLE FREE — undefined behavior!
```

### ✅ Use `std::enable_shared_from_this` for self-referencing

```cpp
// === Good — enable_shared_from_this allows safe self-reference ===
class MyClass : public std::enable_shared_from_this<MyClass> {
public:
    std::shared_ptr<MyClass> getSelf() {
        return shared_from_this();  // safe — uses existing control block
    }
};

auto obj = std::make_shared<MyClass>();
auto self = obj->getSelf();  // shares ownership with obj

// === Bad — creating new shared_ptr from this (double free!) ===
class BadClass {
public:
    std::shared_ptr<BadClass> getSelf() {
        return std::shared_ptr<BadClass>(this);  // DANGER! Different control block!
    }
};
```

### ❌ Avoid circular references

```cpp
// === Problem — circular reference prevents cleanup ===
struct B;  // forward declaration

struct A {
    std::shared_ptr<B> child;  // A owns B
};

struct B {
    std::shared_ptr<A> parent;  // B owns A — CIRCULAR!
};

auto a = std::make_shared<A>();
auto b = std::make_shared<B>();
a->child = b;
b->parent = a;

// use_count(A) = 2, use_count(B) = 2
// Even when a and b go out of scope, ref counts never reach 0!
// Memory leak!

// === Solution — use std::weak_ptr for back-references ===
struct BFixed;

struct AFixed {
    std::shared_ptr<BFixed> child;
};

struct BFixed {
    std::weak_ptr<AFixed> parent;  // weak reference — doesn't increment ref count
};

auto a2 = std::make_shared<AFixed>();
auto b2 = std::make_shared<BFixed>();
a2->child = b2;
b2->parent = a2;

// use_count(A) = 1, use_count(B) = 1
// When a2 and b2 go out of scope, ref counts reach 0 — objects are freed!
```

### ✅ Prefer `make_shared` over direct `new`

```cpp
// === Good — make_shared (single allocation, exception-safe) ===
auto ptr = std::make_shared<MyClass>(arg1, arg2);

// === Avoid — direct new with shared_ptr (two allocations, less efficient) ===
std::shared_ptr<MyClass> ptr(new MyClass(arg1, arg2));
```

### ✅ Use `std::weak_ptr` to observe without owning

```cpp
// === weak_ptr — observes without owning ===
std::weak_ptr<MyClass> weakRef;

{
    auto strongPtr = std::make_shared<MyClass>();
    weakRef = strongPtr;  // doesn't increment ref count

    if (auto locked = weakRef.lock()) {
        // strongPtr is still alive — use *locked
        locked->someMethod();
    } else {
        // strongPtr was destroyed — weak_ptr expired
    }
}
// strongPtr destroyed here
// weakRef.lock() returns empty shared_ptr — object is gone
```

### ❌ Don't use `shared_ptr` for non-owning references

```cpp
// === Bad — shared_ptr for non-owning reference ===
void badExample(MyClass* ptr) {
    std::shared_ptr<MyClass> wrapper(ptr);  // DANGER! ptr may be stack-allocated!
    // wrapper will try to delete ptr when destroyed — undefined behavior!
}

// === Good — raw pointer for non-owning reference ===
void goodExample(MyClass* ptr) {
    // Use raw pointer — no ownership, no delete
    ptr->someMethod();
}
```

---

## 8. When to Use `shared_ptr` vs Other Options

| Scenario | Recommended Type |
|---|---|
| Single owner | `std::unique_ptr<T>` |
| Shared ownership | `std::shared_ptr<T>` |
| Non-owning reference | Raw pointer `T*` or `std::optional<T>` |
| Observer (no ownership) | `T*` or `std::weak_ptr<T>` (if observing a `shared_ptr`) |
| Stack allocation preferred | Just use the object directly |
| Break circular references | `std::weak_ptr<T>` |

### Rule of thumb:
1. **Prefer stack allocation** — no pointer needed
2. **If dynamic allocation is needed, use `unique_ptr`** — it's the default choice
3. **Only use `shared_ptr`** when multiple owners truly exist
4. **Use `weak_ptr`** to break circular references or for non-owning caches
5. **Use raw pointers** for non-owning observations

---

## 9. Performance Characteristics

| Aspect | `shared_ptr` | `unique_ptr` |
|---|---|---|
| Size | ~2x raw pointer (object ptr + control block ptr) | Same as raw pointer (usually) |
| Deletion cost | Atomic ref count decrement + delete | Direct `delete` |
| Cache locality | May be worse (control block on heap) | Excellent |
| Thread safety | Ref count is atomic/thread-safe | Not needed (exclusive ownership) |
| Allocation cost | Two allocations (object + control block) with `new`; one with `make_shared` | One allocation (object only) |

`shared_ptr` has **more overhead** than `unique_ptr` due to the control block and atomic operations. Use `std::make_shared` to minimize allocation overhead.

---

## 10. Quick Reference Cheat Sheet

```cpp
// === Creation ===
auto ptr = std::make_shared<T>(args...);
// Raw: T* ptr = new T(args...);

// === Access ===
ptr->method();
ptr.get();        // raw pointer (same as just using ptr)
*ptr;             // dereference

// === Ownership sharing ===
auto ptr2 = ptr;  // copy — shares ownership
ptr.use_count();   // number of shared_ptrs sharing

// === Ownership transfer (also works via move) ===
auto ptr2 = std::move(ptr);  // ptr becomes nullptr
// Raw: T* ptr2 = ptr; ptr = nullptr;

ptr.reset();      // release current object (decrement ref count)
// Raw: delete ptr; ptr = nullptr;

ptr = std::make_shared<T>();  // replace with new object
// Raw: delete ptr; ptr = new T();

// === Comparison ===
if (ptr) { /* not null */ }
if (ptr == nullptr) { /* null */ }
// Raw: same syntax works — if (ptr) / if (ptr == nullptr)

// === Weak pointer (observe without owning) ===
std::weak_ptr<T> weak = ptr;
if (auto locked = weak.lock()) {
    // ptr is still alive — use *locked
}
// Raw: N/A — no equivalent for raw pointers

// === Custom deleter ===
std::shared_ptr<T> ptr(obj, deleter);