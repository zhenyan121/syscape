[English](foundation.md) | [简体中文](foundation.zh-CN.md)

# Foundation API Reference

The Foundation headers provide compile-target identification, capability vocabulary, portable error codes, and the core `syscape::result<T>` container.

---

## 1. Architecture Identification (`<syscape/architecture.hpp>`)

- **Profile**: Freestanding Minimal
- **Language Standard**: Strict C++11 or later
- **Namespace**: `namespace syscape`

### Types and Enums

#### `enum class architecture`
Identifies the CPU architecture targeted by the current translation unit:
- `unknown`, `x86`, `x86_64`, `arm`, `arm64`, `riscv32`, `riscv64`, `mips`, `mips64`, `powerpc`, `powerpc64`, `sparc`, `sparc64`, `s390`, `s390x`, `loongarch32`, `loongarch64`, `ia64`, `alpha`, `m68k`, `avr`, `msp430`, `xtensa`, `arc`, `csky`, `microblaze`, `nios2`, `superh`, `v850`, `rl78`, `rx`, `h8300`, `blackfin`, `cris`, `pa_risc`, `pdp11`, `vax`, `m32c`, `m32r`, `fr30`, `frv`, `pru`, `mmix`, `stormy16`, `visium`, `epiphany`, `iq2000`, `lm32`, `mep`, `mcore`, `mn10300`, `moxie`, `nds32`.

#### `enum class byte_order`
- `unknown`: Target byte order cannot be determined.
- `little_endian`: Least significant byte stored at lowest address.
- `big_endian`: Most significant byte stored at lowest address.
- `mixed_endian`: PDP-11 style middle-endian.

#### `enum class data_model`
Identifies fundamental type widths:
- `unknown`, `lp32`, `ilp32`, `lp64`, `llp64`, `ilp64`, `other`.

#### `struct data_model_info`
```cpp
struct data_model_info {
    unsigned int short_bits;
    unsigned int int_bits;
    unsigned int long_bits;
    unsigned int long_long_bits;
    unsigned int pointer_bits;
};
```

### Functions

```cpp
constexpr architecture target_architecture() noexcept;
constexpr byte_order target_byte_order() noexcept;
constexpr data_model_info target_data_model_info() noexcept;
data_model target_data_model() noexcept;

const char* architecture_name(architecture value) noexcept;
const char* byte_order_name(byte_order value) noexcept;
const char* data_model_name(data_model value) noexcept;
```

---

## 2. Toolchain Identification (`<syscape/toolchain.hpp>`)

- **Profile**: Freestanding Minimal
- **Language Standard**: Strict C++11 or later
- **Namespace**: `namespace syscape`

### Types and Enums

#### `enum class compiler`
Identifies compiler frontend:
- `unknown`, `gcc`, `clang`, `apple_clang`, `msvc`, `intel_classic`, `intel_llvm`, `ibm_xl`, `ibm_open_xl`, `oracle_developer_studio`, `hp_acc`, `iar`, `arm_compiler`, `green_hills`, `texas_instruments`, `renesas`, `microchip_xc`, `open_watcom`, `emscripten`.

#### `enum class standard_library`
- `unknown`, `libstdcxx`, `libcxx`, `msvc_stl`, `dinkumware`.

#### `struct toolchain_version`
```cpp
struct toolchain_version {
    unsigned int major;
    unsigned int minor;
    unsigned int patch;
};
```

### Functions

```cpp
constexpr compiler target_compiler() noexcept;
constexpr toolchain_version target_compiler_version() noexcept;
constexpr long target_cpp_version() noexcept;
constexpr standard_library target_standard_library() noexcept;

const char* compiler_name(compiler value) noexcept;
const char* standard_library_name(standard_library value) noexcept;
```

---

## 3. Execution Environment (`<syscape/execution_environment.hpp>`)

- **Profile**: Freestanding Minimal
- **Language Standard**: Strict C++11 or later
- **Namespace**: `namespace syscape`

### Types and Enums

#### `enum class operating_system`
Compile-target OS detection:
- `unknown`, `windows`, `linux_os`, `macos`, `android`, `ios`, `ipados`, `watchos`, `tvos`, `visionos`, `freebsd`, `openbsd`, `netbsd`, `dragonfly_bsd`, `illumos`, `solaris`, `aix`, `hpux`, `haiku`, `serenityos`, `redox`, `hurd`, `qnx`, `vxworks`, `rtems`, `zephyr`, `nuttx`, `wasi`, `emscripten`, `openharmony`.

#### `enum class execution_environment`
Runtime environment classification:
- `unknown`: Target environment cannot be identified.
- `hosted`: Standard operating system with complete POSIX or Win32 runtime.
- `sandboxed`: Constrained runtime with restricted OS permissions (e.g., WASI, iOS sandbox).
- `compatibility`: Emulation or compatibility layers (e.g., Cygwin, MinGW).
- `rtos`: Real-Time Operating System environment.
- `bare_metal`: Non-hosted environment without standard library runtime.

### Functions

```cpp
constexpr operating_system target_operating_system() noexcept;
constexpr execution_environment target_execution_environment() noexcept;

const char* operating_system_name(operating_system value) noexcept;
const char* execution_environment_name(execution_environment value) noexcept;
```

---

## 4. Capability Status (`<syscape/capability.hpp>`)

- **Profile**: Freestanding Minimal
- **Language Standard**: Strict C++11 or later
- **Namespace**: `namespace syscape`

### Types and Enums

#### `enum class capability_state`
- `unknown`: Platform does not recognize or declare this capability.
- `unsupported`: Capability is known but not supported on this target.
- `available`: Capability is fully available and accessible.
- `permission_required`: Capability exists but requires elevated privileges or permissions.
- `temporarily_unavailable`: Capability exists but is temporarily inaccessible.

#### `class capability`
```cpp
class capability {
public:
    constexpr capability() noexcept = default;
    constexpr explicit capability(capability_state state) noexcept;

    constexpr capability_state state() const noexcept;
    constexpr bool available() const noexcept;
    constexpr bool recognized() const noexcept;
    constexpr explicit operator bool() const noexcept;
};

const char* capability_state_name(capability_state value) noexcept;
```

---

## 5. Portable Errors (`<syscape/error.hpp>`)

- **Profile**: Hosted C++11 / Hosted Full C++17
- **Language Standard**: C++11 or later (Requires `<system_error>`)
- **Namespace**: `namespace syscape`

### Types and Enums

#### `enum class errc`
Portable library error codes:
```cpp
enum class errc {
    success = 0,
    unknown = 1,
    not_supported,
    permission_denied,
    not_found,
    temporarily_unavailable,
    malformed_data,
    io_error,
    invalid_encoding,
    value_too_large,
    resource_exhausted,
    invalid_argument
};
```

### Functions

```cpp
const std::error_category& error_category() noexcept;
std::error_code make_error_code(errc value) noexcept;
```

---

## 6. Value-or-Error Container (`<syscape/result.hpp>`)

- **Profile**: Hosted Full
- **Language Standard**: Strict C++17 or later
- **Namespace**: `namespace syscape`

### Types

#### `class unexpected`
Represents an explicit error state:
```cpp
class unexpected {
public:
    explicit unexpected(std::error_code error) noexcept;
    const std::error_code& error() const noexcept;
};

unexpected fail(std::error_code error) noexcept;
unexpected fail(errc error) noexcept;
```

#### `class bad_result_access`
Exception thrown when attempting to access the value of an error result via `.value()`.
```cpp
class bad_result_access : public std::logic_error {
public:
    explicit bad_result_access(std::error_code error);
    const std::error_code& error() const noexcept;
};
```

#### `template <typename T> class result`
Primary container holding either a value of `T` or a `std::error_code`:
```cpp
template <typename T>
class result {
public:
    using value_type = T;
    using error_type = std::error_code;

    // Constructors
    result(const T& value);
    result(T&& value) noexcept;
    result(const unexpected& unexp) noexcept;
    result(unexpected&& unexp) noexcept;

    // Observers
    constexpr bool has_value() const noexcept;
    constexpr explicit operator bool() const noexcept;

    const T& value() const&;
    T& value() &;
    const T&& value() const&&;
    T&& value() &&;

    const std::error_code& error() const noexcept;

    const T& operator*() const& noexcept;
    T& operator*() & noexcept;
    const T* operator->() const noexcept;
    T* operator->() noexcept;

    template <typename U>
    T value_or(U&& default_value) const&;
};
```

#### `template <> class result<void>`
Specialization for operations that return no data on success:
```cpp
template <>
class result<void> {
public:
    using value_type = void;
    using error_type = std::error_code;

    result() noexcept;
    result(const unexpected& unexp) noexcept;

    constexpr bool has_value() const noexcept;
    constexpr explicit operator bool() const noexcept;

    void value() const;
    const std::error_code& error() const noexcept;
};
```
