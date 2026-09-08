# Unique Pointers — A Practical Guide

## 1. What Is a `unique_ptr`?

A `std::unique_ptr` (defined in `<memory>`) is a **smart pointer** that **exclusively owns** a dynamically allocated object. When the `unique_ptr` goes out of scope, it automatically deletes the managed object.

Key properties:
- **Exclusive ownership** — only one `unique_ptr` can point to a given object at a time
- **Non-copyable** — you cannot copy a `unique_ptr` (copy constructor/assignment are `delete`)
- **Moveable** — ownership can be transferred via move semantics
- **Automatic cleanup** — destructor calls the deleter (default: `delete`)

```cpp
// === unique_ptr version (automatic cleanup) ===
{
    std::unique_ptr<int> ptr = std::make_unique<int>(42);
    // ptr automatically deletes the int when it goes out of scope
}  // <-- int is freed here

// === Raw pointer equivalent (manual cleanup) ===
{
    int* ptr = new int(42);
    // YOU must manually delete — if you forget, it leaks!
    delete ptr;  // <-- int is freed here
    // If an exception or early return happens before delete, memory leaks!
}
```

---

## 2. Creating Unique Pointers

### 2.1 Using `std::make_unique` (C++14, **preferred**)

```cpp
// === unique_ptr version ===
std::unique_ptr<int> num = std::make_unique<int>(42);
std::unique_ptr<std::string> str = std::make_unique<std::string>("hello");
std::unique_ptr<double[]> arr = std::make_unique<double[]>(10);  // array support

// === Raw pointer equivalent ===
int* num = new int(42);
std::string* str = new std::string("hello");
double* arr = new double[10];
// ... use them ...
delete num;
delete str;
delete[] arr;  // note: delete[] for arrays

### 2.1.1 Traversing a `unique_ptr` Array

`std::unique_ptr<T[]>` supports subscript operator (`[]`) and pointer arithmetic, just like a raw array:

```cpp
// === Creating and initializing an array ===
std::unique_ptr<double[]> arr = std::make_unique<double[]>(5);

// Initialize elements
for (size_t i = 0; i < 5; ++i) {
    arr[i] = i * 1.5;  // subscript operator works
}

// === Traversal methods ===

// Method 1: Subscript operator with index
for (size_t i = 0; i < 5; ++i) {
    std::cout << arr[i] << " ";  // prints: 0 1.5 3 4.5 6
}
std::cout << "\n";

// Method 2: Range-based for loop (via raw pointer)
for (const double& val : *arr) {
    // Note: *arr gives the first element, but range-for doesn't work on arrays directly
    // You need to use a raw pointer range or iterators
}

// Method 3: Pointer arithmetic (iterate using raw pointer from .get())
double* ptr = arr.get();
for (size_t i = 0; i < 5; ++i) {
    std::cout << *(ptr + i) << " ";  // prints: 0 1.5 3 4.5 6
}
std::cout << "\n";

// Method 4: std::for_each with raw pointer range
std::for_each(arr.get(), arr.get() + 5, [](const double& val) {
    std::cout << val << " ";  // prints: 0 1.5 3 4.5 6
});
std::cout << "\n";

// Method 5: std::ranges (C++20) with std::span or raw pointer range
std::ranges::for_each(std::ranges::subrange(arr.get(), arr.get() + 5),
    [](const double& val) {
        std::cout << val << " ";  // prints: 0 1.5 3 4.5 6
    });
std::cout << "\n";

// === Raw pointer equivalent (same traversal, but manual cleanup) ===
double* rawArr = new double[5];
for (size_t i = 0; i < 5; ++i) {
    rawArr[i] = i * 1.5;
}

// All traversal methods work identically on raw arrays
for (size_t i = 0; i < 5; ++i) {
    std::cout << rawArr[i] << " ";
}
std::cout << "\n";

std::for_each(rawArr, rawArr + 5, [](const double& val) {
    std::cout << val << " ";
});
std::cout << "\n";

delete[] rawArr;  // must remember to delete[] — unique_ptr handles this automatically
```

> 💡 **Tip:** `std::make_unique<T[]>(N)` creates an array of `N` default-initialized elements (for primitive types like `double`, they are **value-initialized** to zero). Use `std::make_unique<T[]>(N)` with `N` as a compile-time constant or `std::make_unique<T[]>(N)` with explicit initialization via `std::fill` or a loop.

> ⚠️ **Note:** Range-based `for` loops do **not** work directly on `unique_ptr<T[]>` because it does not provide `begin()`/`end()` iterators. Use `.get()` with pointer arithmetic, subscript operator, or wrap in `std::span` (C++20) for range-for compatibility.
```

`make_unique` is preferred because:
- **Exception-safe** — no risk of memory leak if an exception is thrown
- **Shorter syntax** — no need to specify the type twice
- **Better performance** — single memory allocation

### 2.2 Direct Construction (use with caution)

```cpp
// === unique_ptr wrapping a raw pointer ===
int* raw = new int(42);
std::unique_ptr<int> ptr(raw);  // unique_ptr now owns it

// === Raw pointer equivalent (you manage everything) ===
int* raw = new int(42);
// ... use raw ...
delete raw;  // must remember to delete
```

### 2.3 Managing Resources Other Than `new`/`delete`

```cpp
// === unique_ptr version (automatic fclose) ===
std::unique_ptr<FILE, decltype(&fclose)> file(
    fopen("data.txt", "r"),
    &fclose
);

if (file) {
    // use file.get() to access the raw FILE*
}
// fclose is called automatically when file goes out of scope

// === Raw pointer equivalent (manual fclose required) ===
FILE* file = fopen("data.txt", "r");

if (file) {
    // use file
}
fclose(file);  // must remember to close — or file descriptor leaks!
```

---

## 3. Transferring Ownership

Since `unique_ptr` is non-copyable, you **move** ownership:

```cpp
// === unique_ptr version (ownership transfer via move) ===
std::unique_ptr<int> ptr1 = std::make_unique<int>(42);

// Transfer ownership — ptr1 is now empty
std::unique_ptr<int> ptr2 = std::move(ptr1);

// ptr1 is now nullptr
if (!ptr1) {
    std::cout << "ptr1 is empty\n";
}

// ptr2 owns the int
std::cout << *ptr2 << "\n";  // prints 42

// === Raw pointer equivalent (you simulate transfer manually) ===
int* ptr1 = new int(42);

// "Transfer" — assign raw pointer, then null out original
int* ptr2 = ptr1;
ptr1 = nullptr;  // you must manually null out the original!

if (!ptr1) {
    std::cout << "ptr1 is empty\n";
}

std::cout << *ptr2 << "\n";  // prints 42
delete ptr2;  // delete once — ptr1 is already nullptr
// If you forget delete ptr2, memory leaks!
```

### Common patterns:

```cpp
// === Returning from a function — ownership transferred to caller ===
std::unique_ptr<MyClass> createObject() {
    return std::make_unique<MyClass>();
}

// === Raw pointer equivalent (caller must delete!) ===
MyClass* createObject() {
    return new MyClass();  // caller responsible for delete!
}

// === Passing ownership to a function ===
void takeOwnership(std::unique_ptr<MyClass> obj) {
    // function now owns the object
    // obj is destructed when this function returns (stack unwinding)
    // destruction triggers delete of the managed object
}

// === Raw pointer equivalent (ambiguous ownership!) ===
void takeOwnership(MyClass* obj) {
    // Who owns obj? Who calls delete? Unclear!
    delete obj;  // must remember — but who should?
}

// === Scenarios for unique_ptr parameters ===

// 1. TRANSFER OWNERSHIP — function takes ownership, caller gives up ownership (relinquishes it)
void transferOwnership(std::unique_ptr<MyClass> obj) {
    // caller passes std::move(ptr)
    // function now owns the object; caller's ptr is null after the call
}
// Usage: transferOwnership(std::move(myPtr));

// 2. POLYMORPHIC FACTORY — factory returns unique_ptr, caller gets ownership
std::unique_ptr<Base> createDerived() {
    return std::make_unique<Derived>();
}
// Usage: auto obj = createDerived(); // caller owns, polymorphic

// 3. OBFUSCATED IMPLEMENTATION — hide concrete type, force heap allocation
std::unique_ptr<Impl> createHiddenImpl() {
    return std::make_unique<Impl>(); // Impl can be forward-declared, not exposed
}

// 4. CONTAINERS — store owned objects in STL containers
std::vector<std::unique_ptr<MyClass>> createObjects(int n) {
    std::vector<std::unique_ptr<MyClass>> result;
    for (int i = 0; i < n; ++i) {
        result.push_back(std::make_unique<MyClass>());
    }
    return result; // ownership transferred to caller via move
}

// 5. EXCEPTION-SAFE WRAPPERS — ensure cleanup even on errors
class ResourceGuard {
    std::unique_ptr<MyClass> obj; // member unique_ptr
public:
    ResourceGuard() : obj(std::make_unique<MyClass>()) {}
    // Destructor auto-cleans — no manual delete needed
};

// === Storing in a container ===
std::vector<std::unique_ptr<MyClass>> container;
container.push_back(std::make_unique<MyClass>());

// === Raw pointer equivalent (container doesn't own — who cleans up?) ===
std::vector<MyClass*> container;
container.push_back(new MyClass());
// ... later, you must manually delete all:
for (auto ptr : container) {
    delete ptr;
}
// If an exception occurs between push_back and the loop, leak!
```

---

## 4. Accessing the Managed Object

```cpp
// === unique_ptr version ===
std::unique_ptr<MyClass> ptr = std::make_unique<MyClass>();

// Dereference
*ptr;

// Access member
ptr->someMethod();

// Raw pointer (for C APIs, etc.)
ptr.get();

// Check if not empty
if (ptr) { /* ... */ }
if (ptr != nullptr) { /* ... */ }

// === Raw pointer equivalent (same syntax for access) ===
MyClass* ptr = new MyClass();

*ptr;
ptr->someMethod();
// No .get() needed — ptr IS already a raw pointer

if (ptr) { /* ... */ }
if (ptr != nullptr) { /* ... */ }

delete ptr;  // don't forget!
```

> ⚠️ **Never** use `get()` to transfer ownership. It returns a non-owning raw pointer.

---

## 5. Releasing and Resetting

```cpp
// === unique_ptr version ===
std::unique_ptr<int> ptr = std::make_unique<int>(42);

// Reset — deletes current object, optionally replaces with new one
ptr.reset();           // deletes the int, ptr becomes nullptr
ptr.reset(new int(99));  // deletes old, creates new with 99
ptr.reset();           // deletes the 99

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
// unique_ptr automatically sets to nullptr after reset/release
```

---

## 6. Custom Deleters

A custom deleter is part of the `unique_ptr` type:

```cpp
// Lambda deleter (zero overhead if stateless)
auto fileDeleter = [](FILE* f) {
    if (f) fclose(f);
};

std::unique_ptr<FILE, decltype(fileDeleter)> file(
    fopen("data.txt", "r"),
    fileDeleter
);

// Type-erased deleter (using std::function — has small runtime cost)
std::unique_ptr<int, std::function<void(int*)>> smartPtr(
    new int(42),
    [](int* p) { delete p; }
);
```

> ⚠️ Custom deleters affect the type. `unique_ptr<T, Deleter1>` and `unique_ptr<T, Deleter2>` are **different types**. This can complicate containers. Prefer stateless deleters (zero overhead) when possible.

---

## 7. Common Pitfalls & Best Practices

### ❌ Never create multiple `unique_ptr` from the same raw pointer

```cpp
// === unique_ptr (still dangerous — don't do this!) ===
int* raw = new int(42);
std::unique_ptr<int> ptr1(raw);
std::unique_ptr<int> ptr2(raw);  // DOUBLE FREE — undefined behavior!

// === Raw pointer equivalent (same problem) ===
int* raw = new int(42);
int* ptr1 = raw;
int* ptr2 = raw;
delete ptr1;  // deletes the memory
delete ptr2;  // DOUBLE FREE — undefined behavior!
```

### ❌ Never use `unique_ptr` in a `std::vector` with reallocation

```cpp
std::vector<std::unique_ptr<int>> vec;
vec.push_back(std::make_unique<int>(1));
// If vector reallocates, it needs to copy elements — unique_ptr is not copyable!
// Use std::move:
vec.push_back(std::move(vec.back()));  // if moving existing element
```

Actually, `std::vector<std::unique_ptr<T>>` **does work** because `push_back` accepts an rvalue reference. But be careful with `insert` and `emplace` — always use `std::move` for existing elements.

### ✅ Prefer `make_unique` over direct `new`

```cpp
// === Good — make_unique (exception-safe) ===
auto ptr = std::make_unique<MyClass>(arg1, arg2);

// === Raw pointer equivalent ===
MyClass* ptr = new MyClass(arg1, arg2);
// ... use ptr ...
delete ptr;  // must remember

// === Avoid — direct new with unique_ptr (verbose, no benefit) ===
std::unique_ptr<MyClass> ptr(new MyClass(arg1, arg2));
```

### ✅ Use `std::unique_ptr` as a class member for PIMPL idiom

```cpp
class MyClass {
private:
    struct Impl;  // forward declaration
    std::unique_ptr<Impl> pImpl;  // hides implementation details
};
```

### ✅ Use `std::make_unique` for exception safety

```cpp
// === Dangerous — both versions can leak if process() throws ===
// unique_ptr version:
void badExample() {
    process(new int(1), new int(2));  // potential leak!
}

// Raw pointer version (same problem):
void badExampleRaw() {
    process(new int(1), new int(2));  // potential leak!
}

// Safe — make_unique guarantees cleanup:
void goodExample() {
    auto a = std::make_unique<int>(1);
    auto b = std::make_unique<int>(2);
    process(a.get(), b.get());
}

// Safe raw pointer version (verbose, error-prone):
void goodExampleRaw() {
    int* a = new int(1);
    int* b = new int(2);
    try {
        process(a, b);
    } catch (...) {
        delete a;
        delete b;
        throw;
    }
    delete a;
    delete b;
}
```

---

## 8. When to Use `unique_ptr` vs Other Options

| Scenario | Recommended Type |
|---|---|
| Single owner | `std::unique_ptr<T>` |
| Shared ownership | `std::shared_ptr<T>` |
| Non-owning reference | Raw pointer `T*` or `std::optional<T>` |
| Observer (no ownership) | `T*` or `std::reference_wrapper<T>` |
| Stack allocation preferred | Just use the object directly |

### Rule of thumb:
1. **Prefer stack allocation** — no pointer needed
2. **If dynamic allocation is needed, use `unique_ptr`** — it's the default choice
3. **Only use `shared_ptr`** when multiple owners truly exist
4. **Use raw pointers** for non-owning observations

---

## 9. Performance Characteristics

| Aspect | `unique_ptr` | `shared_ptr` |
|---|---|---|
| Size | Same as raw pointer (usually) | ~2x raw pointer (control block) |
| Deletion cost | Direct `delete` | Atomic ref count decrement |
| Cache locality | Excellent | May be worse (control block) |
| Thread safety | Not needed (exclusive ownership) | Ref count is atomic/thread-safe |

`unique_ptr` has **zero overhead** compared to a raw pointer in most cases. It is the most efficient smart pointer.

---

## 10. Quick Reference Cheat Sheet

```cpp
// === Creation ===
auto ptr = std::make_unique<T>(args...);
// Raw: T* ptr = new T(args...);

auto arr = std::make_unique<T[]>(size);
// Raw: T* arr = new T[size];

// === Access ===
ptr->method();
ptr.get();        // raw pointer (same as just using ptr)
*ptr;             // dereference

// === Ownership transfer ===
auto ptr2 = std::move(ptr);
// Raw: T* ptr2 = ptr; ptr = nullptr;

ptr.reset();      // release current object
// Raw: delete ptr; ptr = nullptr;

ptr = std::make_unique<T>();  // replace with new object
// Raw: delete ptr; ptr = new T();

// === Comparison ===
if (ptr) { /* not null */ }
if (ptr == nullptr) { /* null */ }
// Raw: same syntax works — if (ptr) / if (ptr == nullptr)

// === Custom deleter ===
std::unique_ptr<T, Deleter> ptr(obj, deleter);
// Raw: T* ptr = obj; /* remember to call deleter(ptr) on cleanup */