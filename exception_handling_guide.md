# Exception Handling — A Practical Guide

## 1. What Is Exception Handling?

Exception handling is a C++ mechanism for **handling runtime errors gracefully** without crashing the program. It separates error-handling code from normal business logic, making programs more robust and readable.

Key concepts:
- **`throw`** — signals that an error occurred
- **`try`** — a block of code that might throw an exception
- **`catch`** — a block that handles a specific type of exception
- **Exception objects** — any type thrown and caught (usually derived from `std::exception`)

```cpp
// === Basic try/catch structure ===
try {
    // Code that might throw
    throw std::runtime_error("Something went wrong");
} catch (const std::runtime_error& e) {
    // Handle the error
    std::cerr << "Caught: " << e.what() << std::endl;
}

// === Without exception handling (error codes) ===
int result = someFunction();  // returns -1 on error
if (result == -1) {
    // Handle error — mixed with business logic
}
```

---

## 2. Throwing Exceptions

### 2.1 Deep Dive: How `throw` Works

The `throw` expression is the mechanism by which exceptions are **signaled** in C++. When you write `throw expr;`, the following sequence of events occurs:

#### 2.1.1 Exception Object Creation

```cpp
throw std::runtime_error("File not found");
```

This statement does **three things** internally:

1. **Creates a temporary object** — `std::runtime_error("File not found")` constructs a temporary exception object on the stack
2. **Copies/moves the object** — The temporary is copied into a special exception storage area managed by the C++ runtime (the "exception object")
3. **Begins stack unwinding** — The runtime starts walking back up the call stack, looking for a matching `catch` block

```cpp
// === What happens step by step ===

// Line A: A temporary is created on the stack
// Line B: The temporary is copied into exception storage
// Line C: Stack unwinding begins — frames are popped until a catch is found

throw std::runtime_error("File not found");  // A -> B -> C (unwind)
```

#### 2.1.2 Why Throw by Value?

```cpp
// === CORRECT: Throw by value ===
throw std::runtime_error("error");

// === What happens: ===
// 1. Temporary std::runtime_error is created on the stack
// 2. It is COPIED into exception storage (managed by runtime)
// 3. Stack unwinding begins
// 4. The catch block catches by reference to the exception storage copy

// === Why not throw by reference? ===
// throw getException();  // DANGER! Returns reference to local — dangling!
// throw myGlobalException;  // Works but prevents polymorphism

// === Why not throw by pointer? ===
// throw new std::runtime_error("error");  // Caller must delete! Leaks if not caught!

// === Catching the thrown object ===
try {
    throw std::runtime_error("error");
} catch (const std::runtime_error& e) {
    // e refers to the COPY in exception storage — safe!
    std::cout << e.what() << std::endl;
}
```

**Key insight:** You throw by value, but catch by `const&`. The thrown object is copied into exception storage, so the original temporary is destroyed during stack unwinding, but the copy in exception storage remains valid until the catch block completes.

#### 2.1.3 Stack Unwinding Explained

```cpp
#include <iostream>
#include <stdexcept>
#include <memory>

class TrackedObject {
    std::string name;
public:
    explicit TrackedObject(const std::string& n) : name(n) {
        std::cout << "[" << name << "] constructed" << std::endl;
    }
    ~TrackedObject() {
        std::cout << "[" << name << "] destroyed (unwinding)" << std::endl;
    }
};

void levelC() {
    auto objC = std::make_unique<TrackedObject>("C");
    std::cout << "[levelC] About to throw" << std::endl;
    throw std::runtime_error("Error from levelC");
    // objC will be destroyed here during unwinding
}

void levelB() {
    auto objB = std::make_unique<TrackedObject>("B");
    std::cout << "[levelB] Calling levelC()" << std::endl;
    levelC();  // Throws — unwinding starts
    std::cout << "[levelB] This is NEVER reached" << std::endl;
    // objB would be destroyed here if levelC didn't throw
}

void levelA() {
    auto objA = std::make_unique<TrackedObject>("A");
    std::cout << "[levelA] Calling levelB()" << std::endl;
    try {
        levelB();  // Throws from levelC — unwinding passes through levelB to levelA
    } catch (const std::runtime_error& e) {
        std::cout << "[levelA] Caught: " << e.what() << std::endl;
        // objA is destroyed here when levelA returns
    }
    // objA destroyed here
}

int main() {
    try {
        levelA();
    } catch (...) {
        std::cout << "[main] Caught in main (should not happen)" << std::endl;
    }
    std::cout << "[main] Program continues normally" << std::endl;
}

// === Output: ===
// [A] constructed
// [levelA] Calling levelB()
// [B] constructed
// [levelB] Calling levelC()
// [C] constructed
// [levelC] About to throw
// [C] destroyed (unwinding)   <-- C's frame unwound
// [B] destroyed (unwinding)   <-- B's frame unwound
// [levelA] Caught: Error from levelC
// [A] destroyed               <-- A's frame exits normally
// [main] Program continues normally
```

**What stack unwinding does:**
1. When `throw` is executed, the C++ runtime begins **stack unwinding**
2. It walks up the call stack, frame by frame
3. For each frame being unwound:
   - All **automatic objects** (locals) are destroyed in reverse order of construction
   - Smart pointers (`unique_ptr`, `shared_ptr`) release their resources
   - RAII wrappers (locks, file handles) are released
4. If a matching `catch` is found, unwinding stops and control transfers to the handler
5. If no `catch` is found before `main()` returns, `std::terminate()` is called

#### 2.1.4 What Gets Destroyed During Unwinding

```cpp
#include <iostream>
#include <memory>
#include <vector>
#include <stdexcept>

class Resource {
    std::string name;
public:
    explicit Resource(const std::string& n) : name(n) {
        std::cout << "  [" << name << "] acquired" << std::endl;
    }
    ~Resource() {
        std::cout << "  [" << name << "] released" << std::endl;
    }
};

void demonstrateUnwinding() {
    std::cout << "--- Entering function ---" << std::endl;

    Resource rawResource("raw (manual)");
    int* rawPtr = new int(42);
    std::cout << "  [rawPtr] allocated" << std::endl;

    {
        Resource smartResource("unique_ptr");
        auto smartPtr = std::make_unique<int>(99);
        std::cout << "  [smartPtr] allocated" << std::endl;

        std::vector<int> vec{1, 2, 3};
        std::cout << "  [vec] created" << std::endl;

        std::cout << "  --- About to throw ---" << std::endl;
        throw std::runtime_error("Test exception");
        // smartPtr, vec, smartResource, rawResource destroyed in reverse order
        // rawPtr is NOT destroyed -> MEMORY LEAK

        // smartPtr destroyed -> memory freed (unique_ptr destructor)
        // vec destroyed -> elements destroyed, capacity freed (vector destructor)
        // smartResource destroyed -> resource released (RAII destructor)
        // rawResource destroyed -> resource released (RAII destructor - stack object)
        // rawPtr NOT freed! -> MEMORY LEAK (raw pointer, no automatic cleanup)
    }

    std::cout << "--- This is never reached ---" << std::endl;
}

int main() {
    try {
        demonstrateUnwinding();
    } catch (const std::exception& e) {
        std::cout << "Caught: " << e.what() << std::endl;
    }
    std::cout << "Program continues" << std::endl;
}

// === Output: ===
// --- Entering function ---
//   [raw (manual)] acquired
//   [rawPtr] allocated
//   [unique_ptr] acquired
//   [smartPtr] allocated
//   [vec] created
//   --- About to throw ---
//   [vec] released                    <-- destroyed (vector destructor)
//   [smartPtr] released               <-- destroyed (unique_ptr frees memory)
//   [unique_ptr] released             <-- destroyed (RAII resource released)
//   [raw (manual)] released           <-- destroyed (RAII resource released - stack object)
//   [rawPtr] NOT freed!               <-- LEAK! (raw pointer not deleted)
// Caught: Test exception
// Program continues
```

### 2.2 Using `throw` — Quick Reference

```cpp
// === Throwing by value (standard practice) ===
throw std::runtime_error("File not found");
throw 42;                    // primitive types allowed but discouraged
throw MyCustomError("oops"); // custom types also work

// === Throwing from a function ===
int divide(int a, int b) {
    if (b == 0) {
        throw std::invalid_argument("Division by zero");
    }
    return a / b;
}

// === Raw pointer equivalent (error codes) ===
int divide(int a, int b, int* error) {
    if (b == 0) {
        *error = 1;
        return 0;
    }
    *error = 0;
    return a / b;
}
// Caller must check *error every time — easy to forget!
```

### 2.2 The `noexcept` Specifier (C++11)

Use `noexcept` to guarantee a function will **not** throw exceptions. This enables compiler optimizations and affects exception safety guarantees.

```cpp
// === Functions that promise not to throw ===
void safeFunction() noexcept {
    // Cannot throw — if it does, std::terminate() is called
    int x = 42;
}

// === Function that might throw (default) ===
void riskyFunction();  // implicitly noexcept(false)

// === Explicitly allow throwing ===
void mightThrow() noexcept(false) {
    throw std::runtime_error("I told you so");
}

// === With templates (C++11) ===
template<typename T>
void maybeSafe(T& t) noexcept(noexcept(t.clone())) {
    // Only noexcept if t.clone() doesn't throw
    t.clone();
}

// === Raw pointer equivalent (no direct analog) ===
// You can't guarantee a raw function won't crash or leak
void rawFunction() {
    int* p = new int[1000000];  // might throw std::bad_alloc
    delete[] p;
}
```

---

## 3. Catching Exceptions

### 3.1 Basic Catch Blocks

```cpp
// === Catching by const reference (standard practice) ===
try {
    throw std::runtime_error("An error occurred");
} catch (const std::runtime_error& e) {
    std::cerr << "Runtime error: " << e.what() << std::endl;
}

// === Multiple catch blocks (order matters!) ===
try {
    throw std::runtime_error("Runtime problem");
} catch (const std::invalid_argument& e) {
    std::cerr << "Invalid argument: " << e.what() << std::endl;
} catch (const std::runtime_error& e) {
    std::cerr << "Runtime error: " << e.what() << std::endl;
} catch (const std::exception& e) {
    std::cerr << "Standard exception: " << e.what() << std::endl;
} catch (...) {
    std::cerr << "Unknown exception caught" << std::endl;
}

// === Catching specific types ===
try {
    throw 42;
} catch (int e) {
    std::cerr << "Caught int: " << e << std::endl;
} catch (...) {
    std::cerr << "Fallback" << std::endl;
}
```

### 3.2 Catching Order Matters

```cpp
// === CORRECT: Most specific first ===
try {
    throw std::runtime_error("error");
} catch (const std::runtime_error& e) {  // matches first
    std::cerr << "Runtime: " << e.what() << std::endl;
} catch (const std::exception& e) {        // base class
    std::cerr << "Exception: " << e.what() << std::endl;
}

// === WRONG: Base class catches everything ===
try {
    throw std::runtime_error("error");
} catch (const std::exception& e) {         // catches ALL exceptions!
    std::cerr << "Exception: " << e.what() << std::endl;
} catch (const std::runtime_error& e) {     // NEVER reached — dead code!
    std::cerr << "Runtime: " << e.what() << std::endl;
}
```

### 3.3 Rethrowing Exceptions

```cpp
// === Rethrowing to add context ===
try {
    loadData();
} catch (const std::filesystem::filesystem_error& e) {
    std::cerr << "Failed to load data: " << e.what() << std::endl;
    throw;  // rethrow the same exception
}

// === Throwing a new exception with context ===
try {
    loadData();
} catch (const std::exception& e) {
    throw std::runtime_error(std::string("Failed to initialize: ") + e.what());
}

// === Catch and suppress (swallow) ===
try {
    riskyOperation();
} catch (...) {
    // Silently ignore — use sparingly!
}
```

---

## 4. Standard Exception Hierarchy

### 4.1 The Exception Tree

```
std::exception                          (base for all standard exceptions)
├── std::bad_alloc                      (memory allocation failure)
├── std::bad_cast                       (dynamic_cast to reference failed)
├── std::bad_typeid                     (typeid on null pointer)
├── std::bad_weak_ptr                   (constructing shared_ptr from empty weak_ptr)
├── std::logic_error                    (programmable — bug in code)
│   ├── std::domain_error               (domain violation)
│   ├── std::invalid_argument           (invalid argument)
│   ├── std::length_error               (wrong size)
│   ├── std::out_of_range               (out of range)
│   ├── std::overflow_error             (numeric overflow)
│   ├── std::range_error                (range violation)
│   └── std::underflow_error            (numeric underflow)
├── std::runtime_error                  (run-time detectable — external factors)
│   ├── std::filesystem::filesystem_error (filesystem operations)
│   ├── std::invalid_argument
│   ├── std::ios_base::failure            (I/O errors)
│   ├── std::system_error                 (C++11, system errors)
│   └── std::underflow_error
└── std::nested_exception               (C++11, nested exceptions)
```

### 4.2 Using Standard Exceptions

```cpp
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <stdexcept>

// === std::invalid_argument ===
double sqrt_safe(double x) {
    if (x < 0) {
        throw std::invalid_argument("Cannot compute square root of negative number");
    }
    return std::sqrt(x);
}

// === std::out_of_range ===
int getElement(const std::vector<int>& vec, size_t index) {
    if (index >= vec.size()) {
        throw std::out_of_range("Vector index " + std::to_string(index) +
                                " out of range (size: " + std::to_string(vec.size()) + ")");
    }
    return vec.at(index);  // .at() does this check automatically!
}

// === std::bad_alloc ===
void demonstrateBadAlloc() {
    try {
        while (true) {
            std::vector<char> huge(1024 * 1024 * 100);  // 100 MB each iteration
        }
    } catch (const std::bad_alloc& e) {
        std::cerr << "Memory allocation failed: " << e.what() << std::endl;
    }
}

// === std::length_error ===
std::string repeatString(const std::string& s, size_t n) {
    if (n > 1000000) {
        throw std::length_error("String too long after repetition");
    }
    return std::string(n, s[0]);  // simplified for example
}

// === std::overflow_error ===
int factorial(int n) {
    if (n < 0) throw std::invalid_argument("Negative factorial");
    if (n > 12) throw std::overflow_error("Factorial overflow for int");
    int result = 1;
    for (int i = 2; i <= n; ++i) result *= i;
    return result;
}
```

---

## 5. Creating Custom Exceptions

### 5.1 Deriving from `std::exception`

```cpp
#include <exception>
#include <string>

// === Basic custom exception ===
class MyException : public std::exception {
    std::string message;
public:
    explicit MyException(const std::string& msg) : message(msg) {}

    // Must override what() — standard interface
    const char* what() const noexcept override {
        return message.c_str();
    }
};

// === Better: derive from standard exception ===
class ValidationError : public std::runtime_error {
    int errorCode;
public:
    ValidationError(const std::string& msg, int code)
        : std::runtime_error(msg), errorCode(code) {}

    int error_code() const noexcept { return errorCode; }
};

// === Using custom exceptions ===
void validateUserInput(const std::string& input) {
    if (input.empty()) {
        throw ValidationError("Input cannot be empty", 1001);
    }
    if (input.length() > 255) {
        throw ValidationError("Input too long", 1002);
    }
}

// === Catching custom exceptions ===
try {
    validateUserInput("");
} catch (const ValidationError& e) {
    std::cerr << "Validation failed (code " << e.error_code()
              << "): " << e.what() << std::endl;
} catch (const std::exception& e) {
    std::cerr << "Standard exception: " << e.what() << std::endl;
}
```

### 5.2 Adding Rich Error Information (C++11)

```cpp
#include <exception>
#include <string>
#include <source_location>  // C++20, but shown for completeness

// === C++11: Include file and line info manually ===
class RichException : public std::runtime_error {
    std::string file;
    int line;
    std::string function;
public:
    RichException(const std::string& msg,
                  const char* f = __FILE__,
                  int l = __LINE__,
                  const char* func = __FUNCTION__)
        : std::runtime_error(msg), file(f), line(l), function(func) {}

    std::string location() const {
        return file + ":" + std::to_string(line) + " in " + function;
    }
};

// === Usage ===
void processFile(const std::string& path) {
    if (path.empty()) {
        throw RichException("Empty file path");
        // Output: "Empty file path" at "src/main.cpp:42 in processFile"
    }
}
```

---

## 6. Exception Safety Guarantees

### 6.1 The Three Guarantees

```cpp
// === 1. Basic Guarantee ===
// No resources leak. Object remains in a valid (but possibly changed) state.
void basicGuarantee() {
    std::vector<int> v;
    try {
        v.push_back(1);
        v.push_back(2);
        // If any operation fails, v is still valid — no leak
    } catch (...) {
        // v is still usable here
    }
}

// === 2. Strong Guarantee ===
// If an operation succeeds, it fully succeeds. If it fails, state is unchanged.
// Often achieved via copy-and-swap idiom.
class SafeBuffer {
    std::vector<char> data;
public:
    void replace(const std::vector<char>& newData) {
        SafeBuffer temp;          // Create temporary (may throw)
        temp.data = newData;      // Copy data
        std::swap(data, temp.data); // Swap — noexcept, so safe
        // If copy failed, 'this' is unchanged
    }
};

// === 3. Nothrow Guarantee ===
// Operation never throws.
void nothrowGuarantee() noexcept {
    int x = 42;
    // All operations are noexcept — never throws
}
```

### 6.2 Achieving Exception Safety with Smart Pointers

```cpp
#include <memory>
#include <vector>
#include <stdexcept>

class Resource {
public:
    Resource() { /* acquire resource */ }
    ~Resource() { /* release resource */ }
    void use() { /* ... */ }
};

// === Exception-safe resource management ===
void safeOperation() {
    // unique_ptr ensures cleanup even if exception occurs
    auto res = std::make_unique<Resource>();

    riskyOperation();  // might throw
    res->use();        // only reached if riskyOperation succeeded

    // Automatic cleanup — no matter how we exit (normal, exception, return)
}

// === Avoiding resource leaks in complex code ===
void complexOperation() {
    auto res1 = std::make_unique<Resource>();
    auto res2 = std::make_unique<Resource>();

    // If this throws, res1 and res2 are automatically cleaned up
    res1->use();
    riskyOperation();
    res2->use();
}

// === Raw pointer equivalent (error-prone) ===
void unsafeOperation() {
    Resource* res1 = new Resource();
    Resource* res2 = new Resource();

    try {
        res1->use();
        riskyOperation();  // if this throws...
        res2->use();
        delete res2;
    } catch (...) {
        delete res1;  // must remember to clean up EVERY path!
        throw;
    }
    delete res1;
    delete res2;
}
```

---

## 7. Exception-Safe Function Patterns

### 7.1 Copy-and-Swap Idiom

```cpp
#include <algorithm>
#include <vector>

class SafeContainer {
    std::vector<int> data;
public:
    // Assignment operator with strong guarantee
    SafeContainer& operator=(SafeContainer other) noexcept {
        data.swap(other.data);  // swap is noexcept
        return *this;
    }

    // Swap is noexcept — essential for exception safety
    friend void swap(SafeContainer& a, SafeContainer& b) noexcept {
        using std::swap;
        swap(a.data, b.data);
    }
};

// === Why this works ===
// 1. 'other' is created by copy (may throw)
// 2. If copy throws, 'this' is unchanged
// 3. Swap is noexcept — never throws
// 4. 'other' (old data) is cleaned up when it goes out of scope
```

### 7.2 RAII Lock Guards (STL Example)

```cpp
#include <mutex>

std::mutex mtx;

// === RAII lock guard (exception-safe) ===
void safeFunction() {
    std::lock_guard<std::mutex> lock(mtx);
    // Mutex automatically released when lock goes out of scope
    // Even if an exception is thrown!
    riskyOperation();  // mutex still released properly
}

// === Raw equivalent (error-prone) ===
void unsafeFunction() {
    mtx.lock();
    try {
        riskyOperation();
    } catch (...) {
        mtx.unlock();  // must remember this!
        throw;
    }
    mtx.unlock();  // and this!
}
```

---

## 8. Common Pitfalls and Best Practices

### 8.1 Don't Throw from Destructors

```cpp
class BadExample {
    std::string data;
public:
    ~BadExample() {
        // NEVER throw from destructor!
        // If an exception is already propagating, std::terminate() is called
        if (data.empty()) {
            throw std::runtime_error("Empty data");  // DANGER!
        }
    }
};

// === Correct: log and suppress ===
class GoodExample {
    std::string data;
public:
    ~GoodExample() noexcept {
        try {
            if (data.empty()) {
                // Log the error instead of throwing
                // (In real code, use a proper logging mechanism)
            }
        } catch (...) {
            // Suppress all exceptions from destructor
        }
    }
};
```

### 8.2 Catch by Reference, Not by Value

```cpp
// === WRONG: Catches by value — slices derived types, copies exception ===
try {
    throw std::runtime_error("error");
} catch (std::exception e) {  // copies the exception!
    std::cerr << e.what() << std::endl;
}

// === CORRECT: Catch by const reference — no slicing, no copy ===
try {
    throw std::runtime_error("error");
} catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
}
```

### 8.3 Don't Swallow Exceptions Silently

```cpp
// === WRONG: Silent swallow ===
try {
    riskyOperation();
} catch (...) {
    // Nothing — bad!
}

// === CORRECT: Log at minimum ===
try {
    riskyOperation();
} catch (const std::exception& e) {
    std::cerr << "Error in riskyOperation: " << e.what() << std::endl;
    // Optionally: recover, retry, or rethrow
}
```

### 8.4 Use `std::throw_with_nested` and `std::nested_exception` (C++11)

```cpp
#include <exception>
#include <iostream>

void底层Function() {
    throw std::runtime_error("底层 error");
}

void middleFunction() {
    try {
        底层Function();
    } catch (...) {
        std::throw_with_nested(std::runtime_error("middleFunction failed"));
    }
}

void topFunction() {
    try {
        middleFunction();
    } catch (const std::exception& e) {
        std::cerr << "Top level: " << e.what() << std::endl;
        // Print nested chain if present
        const std::nested_exception* nested = dynamic_cast<const std::nested_exception*>(&e);
        while (nested) {
            auto msg = nested->nested_what();
            if (msg) std::cerr << "  Caused by: " << msg() << std::endl;
            nested = dynamic_cast<const std::nested_exception*>(nested->nested_parent());
        }
    }
}
```

### 8.5 Prefer Specific Exceptions Over Generic Ones

```cpp
// === WRONG: Too generic ===
void parseConfig(const std::string& path) {
    try {
        // ... parsing logic ...
    } catch (...) {
        throw std::runtime_error("Failed");  // loses all context
    }
}

// === CORRECT: Specific exception types ===
void parseConfig(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        throw std::filesystem::filesystem_error("File not found", path,
                                                std::errc::no_such_file_or_directory);
    }
    // ...
    if (line.empty()) {
        throw std::runtime_error("Empty line at position " + std::to_string(pos));
    }
}
```

---

## 9. Exception Handling with Smart Pointers

### 9.1 Smart Pointers and Exception Safety

```cpp
#include <memory>
#include <stdexcept>
#include <iostream>

class Widget {
public:
    Widget() { std::cout << "Widget created\n"; }
    ~Widget() { std::cout << "Widget destroyed\n"; }
    void operate() {
        throw std::runtime_error("Widget operation failed");
    }
};

// === unique_ptr: automatic cleanup on exception ===
void safeWithUniquePtr() {
    auto w = std::make_unique<Widget>();
    w->operate();  // throws — but unique_ptr cleans up automatically
    std::cout << "This line is NOT reached\n";
}  // Widget destroyed here even though exception was thrown

// === shared_ptr: cleanup when last reference is gone ===
void safeWithSharedPtr() {
    auto w1 = std::make_shared<Widget>();
    {
        auto w2 = w1;  // shared ownership
        w1->operate();  // throws
    }  // w2 destroyed, ref count = 1
}  // w1 destroyed, ref count = 0 — Widget destroyed

// === Raw pointer equivalent (leaks on exception) ===
void unsafeWithRawPtr() {
    Widget* w = new Widget();
    w->operate();  // throws!
    delete w;      // NEVER reached — memory leak!
}
```

### 9.2 Exception-Safe Factory Functions

```cpp
// === Factory returning unique_ptr (exception-safe) ===
std::unique_ptr<Widget> createWidget() {
    auto w = std::make_unique<Widget>();
    w->initialize();  // if this throws, unique_ptr cleans up
    return w;
}

// === Factory with shared_ptr ===
std::shared_ptr<Widget> createSharedWidget() {
    return std::make_shared<Widget>();
}

// === Raw pointer factory (error-prone) ===
Widget* createRawWidget() {
    Widget* w = new Widget();
    w->initialize();  // if this throws — LEAK!
    return w;         // caller must remember to delete
}
```

---

## 10. Exception Handling Best Practices Summary

| Practice | Reason |
|----------|--------|
| **Throw by value** | Allows polymorphism while keeping the thrown object on the stack |
| **Catch by `const&`** | Avoids slicing of derived types and unnecessary copies |
| **Catch specific before general** | Most-derived handlers must come first |
| **Never throw from destructors** | Causes `std::terminate()` if exception already propagating |
| **Use `noexcept` when possible** | Enables optimizations and clearer contracts |
| **Prefer standard exceptions** | Use `std::runtime_error`, `std::logic_error`, etc. as base classes |
| **Use RAII for resource management** | Smart pointers + RAII wrappers guarantee cleanup |
| **Log exceptions, don't swallow them** | At minimum, log the error message |
| **Use `std::throw_with_nested`** | Preserve exception chain for debugging |
| **Design for exception safety** | Aim for at least the basic guarantee everywhere |

---

## 11. Quick Reference: When to Use What

```cpp
// === Use exceptions for: ===
// - Error conditions that the caller should handle
// - Unexpected states (null input, out of range, etc.)
// - Resource acquisition failures

// === Don't use exceptions for: ===
// - Control flow (use if/else, loops, state machines)
// - Expected conditions (use error codes or optional types)
// - Performance-critical paths (exception handling has overhead)

// === C++17 alternative: std::optional for expected failures ===
#include <optional>
#include <string>

std::optional<int> parseInt(const std::string& s) {
    try {
        return std::stoi(s);
    } catch (const std::exception&) {
        return std::nullopt;  // no exception thrown
    }
}

// === C++23 alternative: std::expected for fallible operations ===
#include <expected>

std::expected<int, std::string> divide(int a, int b) {
    if (b == 0) {
        return std::unexpected("Division by zero");
    }
    return a / b;
}
```

---

## 12. Comparison: Exception Handling vs. Alternatives

```cpp
// === Exception-based (current guide) ===
int divide(int a, int b) {
    if (b == 0) throw std::invalid_argument("Division by zero");
    return a / b;
}
// Caller:
try {
    auto result = divide(10, 0);
} catch (const std::exception& e) {
    handle(e);
}

// === Error code (C-style) ===
enum class Error { OK, DIVISION_BY_ZERO };
int divide(int a, int b, Error& err) {
    if (b == 0) { err = Error::DIVISION_BY_ZERO; return 0; }
    err = Error::OK;
    return a / b;
}
// Caller:
Error err;
auto result = divide(10, 0, err);
if (err != Error::OK) handle(err);
// Easy to forget checking err!

// === std::optional (C++17) ===
#include <optional>
std::optional<int> divide(int a, int b) {
    if (b == 0) return std::nullopt;
    return a / b;
}
// Caller:
if (auto result = divide(10, 0)) {
    use(*result);
} else {
    handleMissing();
}

// === std::expected (C++23) ===
#include <expected>
std::expected<int, std::string> divide(int a, int b) {
    if (b == 0) return std::unexpected("Division by zero");
    return a / b;
}
// Caller:
if (auto result = divide(10, 0)) {
    use(*result);
} else {
    handle(result.error());
}
```

---

## 13. Real-World Scenarios

### 13.1 Exception Handling in UI Module

In a UI application, exceptions must be caught at the top level to prevent crashes and display user-friendly messages.

```cpp
#include <iostream>
#include <string>
#include <stdexcept>
#include <memory>

// === Domain models ===
class User {
    std::string name;
    std::string email;
public:
    User(const std::string& n, const std::string& e)
        : name(n), email(e) {}
    const std::string& getName() const { return name; }
    const std::string& getEmail() const { return email; }
};

// === UI-layer exceptions ===
class UiException : public std::runtime_error {
public:
    explicit UiException(const std::string& msg)
        : std::runtime_error(msg) {}
};

class ValidationException : public UiException {
    std::string field;
public:
    ValidationException(const std::string& msg, const std::string& fld)
        : UiException(msg), field(fld) {}
    const std::string& field_name() const noexcept { return field; }
};

// === Business logic layer ===
class UserService {
public:
    void validateEmail(const std::string& email) {
        if (email.find('@') == std::string::npos) {
            throw ValidationException("Invalid email format", "email");
        }
    }

    std::shared_ptr<User> createUser(const std::string& name,
                                      const std::string& email) {
        if (name.empty()) {
            throw ValidationException("Name cannot be empty", "name");
        }
        validateEmail(email);
        return std::make_shared<User>(name, email);
    }
};

// === UI Module: Presentation layer ===
class UserRegistrationUI {
    UserService service;
public:
    // Top-level exception handler for UI operations
    void handleRegistration(const std::string& name,
                            const std::string& email) {
        try {
            auto user = service.createUser(name, email);
            displaySuccess(user->getName() + " registered successfully!");
        } catch (const ValidationException& e) {
            displayValidationError(e.field_name(), e.what());
        } catch (const UiException& e) {
            displayError("Registration failed: " + std::string(e.what()));
        } catch (const std::exception& e) {
            displayError("Unexpected error: " + std::string(e.what()));
            logCritical(e.what());
        } catch (...) {
            displayError("An unknown error occurred.");
            logCritical("Unknown exception in registration");
        }
    }

private:
    void displaySuccess(const std::string& msg) {
        std::cout << "[SUCCESS] " << msg << std::endl;
    }

    void displayValidationError(const std::string& field,
                                 const std::string& msg) {
        std::cout << "[VALIDATION ERROR] Field '" << field << "': "
                  << msg << std::endl;
    }

    void displayError(const std::string& msg) {
        std::cout << "[ERROR] " << msg << std::endl;
    }

    void logCritical(const std::string& msg) {
        std::cerr << "[CRITICAL LOG] " << msg << std::endl;
    }
};

// === Usage ===
int main() {
    UserRegistrationUI ui;

    // Valid input
    ui.handleRegistration("Alice", "alice@example.com");

    // Invalid email
    ui.handleRegistration("Bob", "invalid-email");

    // Empty name
    ui.handleRegistration("", "charlie@example.com");
}
```

### 13.2 Exception Handling in Business Layer

The business layer should throw domain-specific exceptions and maintain transactional integrity.

```cpp
#include <iostream>
#include <string>
#include <stdexcept>
#include <memory>
#include <vector>
#include <map>

// === Domain exceptions ===
class BusinessLogicError : public std::logic_error {
public:
    explicit BusinessLogicError(const std::string& msg)
        : std::logic_error(msg) {}
};

class InsufficientFundsError : public BusinessLogicError {
    double amount;
    double balance;
public:
    InsufficientFundsError(double requested, double available)
        : BusinessLogicError("Insufficient funds: requested " +
                            std::to_string(requested) +
                            ", available " +
                            std::to_string(available)),
          amount(requested), balance(available) {}

    double requested_amount() const noexcept { return amount; }
    double current_balance() const noexcept { return balance; }
};

class AccountNotFoundError : public BusinessLogicError {
    std::string account_id;
public:
    AccountNotFoundError(const std::string& id)
        : BusinessLogicError("Account not found: " + id),
          account_id(id) {}

    const std::string& account_id() const noexcept { return account_id; }
};

// === Account model ===
class Account {
    std::string id;
    double balance;
public:
    Account(const std::string& id, double initial)
        : id(id), balance(initial) {}

    const std::string& get_id() const noexcept { return id; }
    double get_balance() const noexcept { return balance; }

    void deposit(double amount) {
        if (amount <= 0) throw BusinessLogicError("Deposit must be positive");
        balance += amount;
    }

    void withdraw(double amount) {
        if (amount <= 0) throw BusinessLogicError("Withdrawal must be positive");
        if (amount > balance) {
            throw InsufficientFundsError(amount, balance);
        }
        balance -= amount;
    }
};

// === Business Layer: Account Service ===
class AccountService {
    std::map<std::string, std::shared_ptr<Account>> accounts;

public:
    void createAccount(const std::string& id, double initial) {
        if (accounts.find(id) != accounts.end()) {
            throw BusinessLogicError("Account already exists: " + id);
        }
        accounts[id] = std::make_shared<Account>(id, initial);
    }

    double getBalance(const std::string& id) {
        auto it = accounts.find(id);
        if (it == accounts.end()) {
            throw AccountNotFoundError(id);
        }
        return it->second->get_balance();
    }

    // Transaction with strong exception guarantee
    void transfer(const std::string& from, const std::string& to,
                  double amount) {
        if (amount <= 0) {
            throw BusinessLogicError("Transfer amount must be positive");
        }

        auto fromIt = accounts.find(from);
        auto toIt = accounts.find(to);

        if (fromIt == accounts.end()) {
            throw AccountNotFoundError(from);
        }
        if (toIt == accounts.end()) {
            throw AccountNotFoundError(to);
        }

        try {
            fromIt->second->withdraw(amount);
            toIt->second->deposit(amount);
            logTransaction(from, to, amount, "SUCCESS");
        } catch (...) {
            logTransaction(from, to, amount, "FAILED - ROLLBACK");
            throw;
        }
    }

private:
    void logTransaction(const std::string& from, const std::string& to,
                        double amount, const std::string& status) {
        std::cout << "[TRANSACTION] " << from << " -> " << to
                  << " (" << amount << ") [" << status << "]" << std::endl;
    }
};

// === Business Layer Usage ===
int main() {
    AccountService service;

    service.createAccount("ACC001", 1000.0);
    service.createAccount("ACC002", 500.0);

    try {
        service.transfer("ACC001", "ACC002", 200.0);
        std::cout << "Transfer successful\n";
    } catch (const InsufficientFundsError& e) {
        std::cerr << "Transfer failed: requested " << e.requested_amount()
                  << ", balance " << e.current_balance() << std::endl;
    } catch (const AccountNotFoundError& e) {
        std::cerr << "Account error: " << e.account_id() << std::endl;
    } catch (const BusinessLogicError& e) {
        std::cerr << "Business error: " << e.what() << std::endl;
    }
}
```

### 13.3 Exception Handling in Database Module

Database operations require careful exception handling for connection failures, query errors, and transaction management.

```cpp
#include <iostream>
#include <string>
#include <stdexcept>
#include <memory>
#include <functional>

// === Database-specific exceptions ===
class DatabaseException : public std::runtime_error {
protected:
    int sql_error_code;
public:
    DatabaseException(const std::string& msg, int code = 0)
        : std::runtime_error(msg), sql_error_code(code) {}

    int error_code() const noexcept { return sql_error_code; }
};

class ConnectionException : public DatabaseException {
public:
    ConnectionException(const std::string& msg, int code = 0)
        : DatabaseException(msg, code) {}
};

class QueryException : public DatabaseException {
    std::string query;
public:
    QueryException(const std::string& msg, const std::string& q, int code = 0)
        : DatabaseException(msg, code), query(q) {}

    const std::string& query_text() const noexcept { return query; }
};

class TransactionException : public DatabaseException {
public:
    TransactionException(const std::string& msg, int code = 0)
        : DatabaseException(msg, code) {}
};

// === RAII Connection wrapper ===
class DatabaseConnection {
    std::string connection_string;
    bool connected;
public:
    explicit DatabaseConnection(const std::string& conn_str)
        : connection_string(conn_str), connected(false) {
        connect();
    }

    ~DatabaseConnection() noexcept {
        try {
            disconnect();
        } catch (...) {
            // Suppress exceptions in destructor
        }
    }

    DatabaseConnection(const DatabaseConnection&) = delete;
    DatabaseConnection& operator=(const DatabaseConnection&) = delete;
    DatabaseConnection(DatabaseConnection&& other) noexcept
        : connection_string(std::move(other.connection_string)),
          connected(other.connected) {
        other.connected = false;
    }

    void connect() {
        if (connection_string.empty()) {
            throw ConnectionException("Empty connection string", 1001);
        }
        connected = true;
        std::cout << "[DB] Connected to: " << connection_string << std::endl;
    }

    void disconnect() {
        if (connected) {
            connected = false;
            std::cout << "[DB] Disconnected" << std::endl;
        }
    }

    bool is_connected() const noexcept { return connected; }
};

// === RAII Transaction wrapper ===
class DatabaseTransaction {
    DatabaseConnection& conn;
    bool active;
public:
    explicit DatabaseTransaction(DatabaseConnection& c)
        : conn(c), active(true) {
        if (!conn.is_connected()) {
            throw TransactionException("Cannot start transaction: not connected", 2001);
        }
        std::cout << "[DB] Transaction STARTED" << std::endl;
    }

    ~DatabaseTransaction() noexcept {
        try {
            if (active) {
                rollback();  // Auto-rollback if not committed
            }
        } catch (...) {
            // Suppress in destructor
        }
    }

    void commit() {
        if (!active) throw TransactionException("Transaction already committed/rolled back", 2002);
        active = false;
        std::cout << "[DB] Transaction COMMITTED" << std::endl;
    }

    void rollback() {
        if (active) {
            active = false;
            std::cout << "[DB] Transaction ROLLED BACK" << std::endl;
        }
    }
};

// === Database Module: Repository ===
class UserRepository {
    std::shared_ptr<DatabaseConnection> conn;

public:
    explicit UserRepository(const std::string& conn_str)
        : conn(std::make_shared<DatabaseConnection>(conn_str)) {}

    void insertUser(const std::string& name, const std::string& email) {
        if (!conn->is_connected()) {
            throw ConnectionException("Database not connected", 1002);
        }

        std::string query = "INSERT INTO users (name, email) VALUES ('"
                          + name + "', '" + email + "')";

        try {
            executeQuery(query);
        } catch (const QueryException& e) {
            throw QueryException("Failed to insert user '" + name + "'",
                                query, e.error_code());
        }
    }

    void transferCredits(const std::string& fromId,
                         const std::string& toId,
                         double amount) {
        if (amount <= 0) {
            throw QueryException("Amount must be positive", "");
        }

        DatabaseTransaction txn(*conn);

        try {
            executeQuery("UPDATE accounts SET balance = balance - " +
                        std::to_string(amount) +
                        " WHERE id = '" + fromId + "'");

            executeQuery("UPDATE accounts SET balance = balance + " +
                        std::to_string(amount) +
                        " WHERE id = '" + toId + "'");

            txn.commit();
        } catch (...) {
            throw;
        }
    }

private:
    void executeQuery(const std::string& query) {
        if (query.find("INVALID") != std::string::npos) {
            throw QueryException("Syntax error in query", query, 3001);
        }
        std::cout << "[DB] Executed: " << query.substr(0, 50) << "..." << std::endl;
    }
};

// === Database Layer Usage ===
int main() {
    try {
        UserRepository repo("postgresql://localhost:5432/mydb");

        repo.insertUser("Alice", "alice@example.com");
        repo.transferCredits("ACC001", "ACC002", 100.0);

        try {
            repo.executeQuery("INVALID QUERY");
        } catch (const QueryException& e) {
            std::cerr << "Query failed (code " << e.error_code()
                      << "): " << e.what() << std::endl;
        }

    } catch (const ConnectionException& e) {
        std::cerr << "Connection failed (code " << e.error_code()
                  << "): " << e.what() << std::endl;
    } catch (const TransactionException& e) {
        std::cerr << "Transaction failed (code " << e.error_code()
                  << "): " << e.what() << std::endl;
    } catch (const DatabaseException& e) {
        std::cerr << "Database error (code " << e.error_code()
                  << "): " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
    }
}
```

### 13.4 Layered Architecture Exception Flow

How exceptions propagate through layers in a real application:

```cpp
#include <iostream>
#include <string>
#include <stdexcept>
#include <memory>

// === Layer 1: Data Access Layer (DAL) ===
class DataAccessException : public std::runtime_error {
public:
    explicit DataAccessException(const std::string& msg)
        : std::runtime_error(msg) {}
};

class UserRepository {
public:
    std::string findEmailById(const std::string& userId) {
        if (userId == "INVALID") {
            throw DataAccessException("User not found in database: " + userId);
        }
        return userId + "@example.com";
    }
};

// === Layer 2: Business Logic Layer (BLL) ===
class BusinessRuleViolation : public std::logic_error {
    std::string rule;
public:
    BusinessRuleViolation(const std::string& msg, const std::string& r)
        : std::logic_error(msg), rule(r) {}
    const std::string& violated_rule() const noexcept { return rule; }
};

class UserService {
    UserRepository repo;
public:
    void sendWelcomeEmail(const std::string& userId) {
        try {
            std::string email = repo.findEmailById(userId);
            if (email.empty()) {
                throw BusinessRuleViolation("Email cannot be empty", "EMAIL_NOT_EMPTY");
            }
            std::cout << "[EMAIL] Welcome sent to " << email << std::endl;
        } catch (const BusinessRuleViolation&) {
            throw;
        } catch (const DataAccessException& e) {
            throw BusinessRuleViolation(
                std::string("Cannot send welcome email: ") + e.what(),
                "USER_EXISTS");
        }
    }
};

// === Layer 3: Application/Service Layer ===
class ApplicationError : public std::runtime_error {
public:
    explicit ApplicationError(const std::string& msg)
        : std::runtime_error(msg) {}
};

class RegistrationService {
    UserService userService;
public:
    void processRegistration(const std::string& userId) {
        try {
            userService.sendWelcomeEmail(userId);
            std::cout << "[APP] Registration completed for: " << userId << std::endl;
        } catch (const BusinessRuleViolation& e) {
            throw ApplicationError(
                std::string("Registration failed - rule '") +
                e.violated_rule() + "': " + e.what());
        } catch (const std::exception& e) {
            throw ApplicationError(std::string("Registration failed: ") + e.what());
        }
    }
};

// === Layer 4: Presentation/UI Layer ===
class ApplicationUI {
    RegistrationService regService;
public:
    void handleRegistration(const std::string& userId) {
        try {
            regService.processRegistration(userId);
        } catch (const ApplicationError& e) {
            displayUserFriendlyError(e.what());
        } catch (const std::exception& e) {
            displayGenericError();
            logException(e);
        } catch (...) {
            displayGenericError();
            logUnknownException();
        }
    }

private:
    void displayUserFriendlyError(const std::string& msg) {
        if (msg.find("USER_EXISTS") != std::string::npos) {
            std::cout << "[UI] User account does not exist. Please check the ID."
                      << std::endl;
        } else {
            std::cout << "[UI] We couldn't complete your registration. "
                      << "Please try again later." << std::endl;
        }
    }

    void displayGenericError() {
        std::cout << "[UI] An unexpected error occurred. Please contact support."
                  << std::endl;
    }

    void logException(const std::exception& e) {
        std::cerr << "[LOG] Exception: " << e.what() << std::endl;
    }

    void logUnknownException() {
        std::cerr << "[LOG] Unknown exception caught" << std::endl;
    }
};

// === Full Stack Usage ===
int main() {
    ApplicationUI ui;

    std::cout << "=== Valid User ===" << std::endl;
    ui.handleRegistration("USER001");

    std::cout << "\n=== Invalid User ===" << std::endl;
    ui.handleRegistration("INVALID");
}
```

### 13.5 Key Patterns Across Layers

| Layer | Exception Strategy | Example |
|-------|-------------------|---------|
| **UI/Presentation** | Catch all, display user-friendly messages, log details | Show "Please try again" instead of stack traces |
| **Application/Service** | Coordinate between business modules, translate to application errors | Wrap business errors with operation context |
| **Business Logic** | Throw domain-specific exceptions, maintain invariants | `InsufficientFundsError`, `BusinessRuleViolation` |
| **Data Access** | Throw low-level data exceptions, include query/connection details | `DataAccessException`, `QueryException` |

**Golden Rules for Layered Exception Handling:**
1. **Never let implementation details leak to the UI** — translate technical errors to user-friendly messages
2. **Add context at each layer** — use `std::throw_with_nested` or wrap exceptions
3. **Catch at the right level** — UI catches everything; business layer catches data-layer exceptions
4. **Log at the catch point** — always log the original exception for debugging
5. **Use RAII for resources** — smart pointers and RAII wrappers ensure cleanup across all layers