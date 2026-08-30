[English](foundation.md) | [简体中文](foundation.zh-CN.md)

# 基础核心 API 参考 (Foundation)

基础核心头文件提供编译目标识别、能力状态词汇表、可移植错误码体系以及核心的 `syscape::result<T>` 容器。

---

## 1. 架构识别 (`<syscape/architecture.hpp>`)

- **兼容性配置**: 独立环境最小化 (Freestanding Minimal)
- **语言标准**: 严格 C++11 或更高版本
- **所属命名空间**: `namespace syscape`

### 类型与枚举

#### `enum class architecture`
标识当前编译单元所针对的 CPU 目标架构：
- `unknown`, `x86`, `x86_64`, `arm`, `arm64`, `riscv32`, `riscv64`, `mips`, `mips64`, `powerpc`, `powerpc64`, `sparc`, `sparc64`, `s390`, `s390x`, `loongarch32`, `loongarch64`, `ia64`, `alpha`, `m68k`, `avr`, `msp430`, `xtensa`, `arc`, `csky`, `microblaze`, `nios2`, `superh`, `v850`, `rl78`, `rx`, `h8300`, `blackfin`, `cris`, `pa_risc`, `pdp11`, `vax`, `m32c`, `m32r`, `fr30`, `frv`, `pru`, `mmix`, `stormy16`, `visium`, `epiphany`, `iq2000`, `lm32`, `mep`, `mcore`, `mn10300`, `moxie`, `nds32`。

#### `enum class byte_order`
目标字节序：
- `unknown`: 无法确定的未知字节序。
- `little_endian`: 小端序（低位字节存于低地址）。
- `big_endian`: 大端序（高位字节存于低地址）。
- `mixed_endian`: PDP-11 混合端序。

#### `enum class data_model`
基本数据类型位宽模型：
- `unknown`, `lp32`, `ilp32`, `lp64`, `llp64`, `ilp64`, `other`。

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

### 函数接口

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

## 2. 工具链识别 (`<syscape/toolchain.hpp>`)

- **兼容性配置**: 独立环境最小化 (Freestanding Minimal)
- **语言标准**: 严格 C++11 或更高版本
- **所属命名空间**: `namespace syscape`

### 类型与枚举

#### `enum class compiler`
编译器前端标识：
- `unknown`, `gcc`, `clang`, `apple_clang`, `msvc`, `intel_classic`, `intel_llvm`, `ibm_xl`, `ibm_open_xl`, `oracle_developer_studio`, `hp_acc`, `iar`, `arm_compiler`, `green_hills`, `texas_instruments`, `renesas`, `microchip_xc`, `open_watcom`, `emscripten`。

#### `enum class standard_library`
标准库实现标识：
- `unknown`, `libstdcxx`, `libcxx`, `msvc_stl`, `dinkumware`。

#### `struct toolchain_version`
```cpp
struct toolchain_version {
    unsigned int major;
    unsigned int minor;
    unsigned int patch;
};
```

### 函数接口

```cpp
constexpr compiler target_compiler() noexcept;
constexpr toolchain_version target_compiler_version() noexcept;
constexpr long target_cpp_version() noexcept;
constexpr standard_library target_standard_library() noexcept;

const char* compiler_name(compiler value) noexcept;
const char* standard_library_name(standard_library value) noexcept;
```

---

## 3. 执行环境识别 (`<syscape/execution_environment.hpp>`)

- **兼容性配置**: 独立环境最小化 (Freestanding Minimal)
- **语言标准**: 严格 C++11 或更高版本
- **所属命名空间**: `namespace syscape`

### 类型与枚举

#### `enum class operating_system`
编译目标操作系统：
- `unknown`, `windows`, `linux_os`, `macos`, `android`, `ios`, `ipados`, `watchos`, `tvos`, `visionos`, `freebsd`, `openbsd`, `netbsd`, `dragonfly_bsd`, `illumos`, `solaris`, `aix`, `hpux`, `haiku`, `serenityos`, `redox`, `hurd`, `qnx`, `vxworks`, `rtems`, `zephyr`, `nuttx`, `wasi`, `emscripten`。

#### `enum class execution_environment`
执行运行时大类划分：
- `unknown`: 无法确定的环境。
- `hosted`: 具备完整 POSIX 或 Win32 运行时的常规托管操作系统。
- `sandboxed`: 权限严格受限的沙盒环境（如 WASI、iOS 沙盒）。
- `compatibility`: 兼容性仿真层（如 Cygwin、MinGW）。
- `rtos`: 实时操作系统 (RTOS) 环境。
- `bare_metal`: 无宿主标准库支持的纯裸机环境。

### 函数接口

```cpp
constexpr operating_system target_operating_system() noexcept;
constexpr execution_environment target_execution_environment() noexcept;

const char* operating_system_name(operating_system value) noexcept;
const char* execution_environment_name(execution_environment value) noexcept;
```

---

## 4. 能力状态 (`<syscape/capability.hpp>`)

- **兼容性配置**: 独立环境最小化 (Freestanding Minimal)
- **语言标准**: 严格 C++11 或更高版本
- **所属命名空间**: `namespace syscape`

### 类型与枚举

#### `enum class capability_state`
- `unknown`: 平台未识别或未声明该项能力。
- `unsupported`: 目标平台已知且明确不支持此能力。
- `available`: 能力完全可用且当前可正常访问。
- `permission_required`: 能力存在，但需要提权或更高权限方可访问。
- `temporarily_unavailable`: 能力存在，但当前暂时不可用。

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

## 5. 可移植错误体系 (`<syscape/error.hpp>`)

- **兼容性配置**: 托管环境 C++11 / 托管完整版 C++17
- **语言标准**: C++11 或更高版本（需要 `<system_error>` 支持）
- **所属命名空间**: `namespace syscape`

### 类型与枚举

#### `enum class errc`
库级可移植错误码枚举：
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

### 函数接口

```cpp
const std::error_category& error_category() noexcept;
std::error_code make_error_code(errc value) noexcept;
```

---

## 6. 值或错误容器 (`<syscape/result.hpp>`)

- **兼容性配置**: 托管完整版 (Hosted Full)
- **语言标准**: 严格 C++17 或更高版本
- **所属命名空间**: `namespace syscape`

### 核心类型

#### `class unexpected`
显式错误包装类：
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
当对包含错误的 `result` 实例调用 `.value()` 时抛出的异常：
```cpp
class bad_result_access : public std::logic_error {
public:
    explicit bad_result_access(std::error_code error);
    const std::error_code& error() const noexcept;
};
```

#### `template <typename T> class result`
封装有效数据 `T` 或 `std::error_code` 的核心结果容器：
```cpp
template <typename T>
class result {
public:
    using value_type = T;
    using error_type = std::error_code;

    // 构造函数
    result(const T& value);
    result(T&& value) noexcept;
    result(const unexpected& unexp) noexcept;
    result(unexpected&& unexp) noexcept;

    // 状态与观测
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
针对成功时不返回数据的操作特化：
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
