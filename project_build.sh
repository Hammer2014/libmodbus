#!/bin/bash

# ============================
# 参数解析
# ============================
CLEAN_BUILD=false
JOBS=""
PLATFORM=""

ARGS=()

for arg in "$@"; do
    case $arg in
    --clean|-c)
        CLEAN_BUILD=true
        ;;
    -j*)
        JOBS="${arg#-j}"
        ;;
    --platform=*)
        PLATFORM="${arg#*=}"
        ;;
    rv1126b|rk3568|t113i|t113)
        PLATFORM="$arg"
        ;;
    *)
        ARGS+=("$arg")
        ;;
    esac
done

# ============================
# 平台配置
# ============================
if [ -z "${PLATFORM}" ]; then
    echo "Usage:"
    echo "  $0 [--clean|-c] <platform> [-j<N>]"
    echo "  platform: rv1126b | rk3568 | t113i | t113"
    echo ""
    echo "  Examples:"
    echo "    $0 rv1126b"
    echo "    $0 --clean rk3568 -j8"
    exit 1
fi

# 工具链路径配置
declare -A TOOLCHAINS=(
    [rv1126b]="/opt/rv1126b-p-toolchain-qt5.15.2-gcc13.3.0"
    [rk3568]="/opt/rk3568-toolchain-qt5.15.2-gcc12.3.0"
    [t113i]="/opt/atk-dlt113is-toolchain-qt5.15"
    [t113]="/opt/Allwinner-T113-S3-toolchain-qt5.15"
)

# 各平台的交叉编译前缀
declare -A CROSS_PREFIXES=(
    [rv1126b]="aarch64-buildroot-linux-gnu"
    [rk3568]="aarch64-buildroot-linux-gnu"
    [t113i]="arm-linux-gnueabi"
    [t113]="arm-buildroot-linux-gnu"
)

# 各平台的 sysroot 相对路径
declare -A SYSROOT_PATHS=(
    [rv1126b]="aarch64-buildroot-linux-gnu/sysroot"
    [rk3568]="aarch64-buildroot-linux-gnu/sysroot"
    [t113i]="arm-buildroot-linux-gnueabi/sysroot"
    [t113]="arm-buildroot-linux-gnu/sysroot"
)

# 检查平台是否有效
if [ -z "${TOOLCHAINS[$PLATFORM]}" ]; then
    echo "❌ Unknown platform: ${PLATFORM}"
    echo "   Supported platforms: rv1126b | rk3568 | t113i | t113"
    exit 1
fi

# 设置平台相关变量
TOOLCHAIN_ROOT="${TOOLCHAINS[$PLATFORM]}"
CROSS_COMPILE="${TOOLCHAIN_ROOT}/bin/${CROSS_PREFIXES[$PLATFORM]}"
SYSROOT="${TOOLCHAIN_ROOT}/${SYSROOT_PATHS[$PLATFORM]}"

# ============================
# 路径配置
# ============================
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
INSTALL_PREFIX="/opt/libmodbus-${PLATFORM}"
SOURCE_DIR="${SCRIPT_DIR}"  # libmodbus 源码目录（当前目录）
BUILD_DIR="${SCRIPT_DIR}/build-libmodbus-${PLATFORM}"

# ============================
# 工具链检查
# ============================
echo "🔍 Checking toolchain for platform: ${PLATFORM}"

if [ ! -x "${CROSS_COMPILE}-gcc" ]; then
    echo "❌ Cross compiler not found:"
    echo "   ${CROSS_COMPILE}-gcc"
    echo ""
    echo "   Toolchain root: ${TOOLCHAIN_ROOT}"
    echo "   Please check if the toolchain is installed correctly."
    exit 1
fi

if [ ! -d "${SYSROOT}" ]; then
    echo "❌ Sysroot not found:"
    echo "   ${SYSROOT}"
    exit 1
fi

if [ ! -f "${SOURCE_DIR}/configure" ]; then
    echo "❌ configure script not found in:"
    echo "   ${SOURCE_DIR}"
    exit 1
fi

# ============================
# 清理源码目录中的配置（如果需要）
# ============================
if [ "${CLEAN_BUILD}" = true ]; then
    echo "🧹 Cleaning source directory..."
    cd "${SOURCE_DIR}"
    make distclean 2>/dev/null || true
    rm -rf "${BUILD_DIR}"
    echo "✅ Source directory cleaned"
fi

# ============================
# 构建参数
# ============================
CONFIGURE_ARGS=(
    "--host=${CROSS_PREFIXES[$PLATFORM]}"
    "--prefix=${INSTALL_PREFIX}"
    "--without-documentation"
    "CC=${CROSS_COMPILE}-gcc"
    "CFLAGS=--sysroot=${SYSROOT}"
    "LDFLAGS=--sysroot=${SYSROOT}"
)

# ============================
# 清理/构建目录
# ============================
if [ "${CLEAN_BUILD}" = true ]; then
    echo "🧹 Cleaning build directory..."
    rm -rf "${BUILD_DIR}"
fi

mkdir -p "${BUILD_DIR}"

# ============================
# 构建信息
# ============================
echo ""
echo "=========================================="
echo "📌 Platform    : ${PLATFORM}"
echo "📌 Toolchain   : ${TOOLCHAIN_ROOT}"
echo "📌 Cross prefix: ${CROSS_PREFIXES[$PLATFORM]}"
echo "📌 Sysroot     : ${SYSROOT}"
echo "📌 Source dir  : ${SOURCE_DIR}"
echo "📌 Build dir   : ${BUILD_DIR}"
echo "📌 Install to  : ${INSTALL_PREFIX}"
echo "📌 Mode        : $( [ "${CLEAN_BUILD}" = true ] && echo 'CLEAN BUILD' || echo 'INCREMENTAL' )"
echo "=========================================="

# ============================
# 执行构建（在构建目录中）
# ============================
cd "${BUILD_DIR}"

echo ""
echo "🔧 Running configure from build directory..."
"${SOURCE_DIR}/configure" "${CONFIGURE_ARGS[@]}"

if [ $? -ne 0 ]; then
    echo "❌ Configure failed!"
    exit 1
fi

echo ""
echo "🔨 Building..."
if [ -n "${JOBS}" ]; then
    make -j"${JOBS}"
else
    make -j$(nproc)
fi

if [ $? -ne 0 ]; then
    echo "❌ Build failed!"
    exit 1
fi

echo ""
echo "📦 Installing to ${INSTALL_PREFIX}..."
make install

if [ $? -ne 0 ]; then
    echo "❌ Install failed!"
    exit 1
fi

echo ""
echo "✅ Build and install completed successfully!"
echo "   Platform    : ${PLATFORM}"
echo "   Installed to: ${INSTALL_PREFIX}"
echo ""
echo "   Library files:"
find "${INSTALL_PREFIX}" -name "*.so*" -o -name "*.a" | while read -r f; do
    echo "     - ${f}"
done