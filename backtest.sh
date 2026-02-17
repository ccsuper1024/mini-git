#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

echo "[mini-git] 回测开始..."

if [ ! -d "${BUILD_DIR}" ]; then
  echo "[mini-git] 未发现 build 目录，执行 CMake 配置..."
  cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}"
fi

echo "[mini-git] 编译项目..."
cmake --build "${BUILD_DIR}"

echo "[mini-git] 运行单元测试 (ctest)..."
(
  cd "${BUILD_DIR}"
  ctest --output-on-failure
)

echo "[mini-git] 运行端到端回测场景..."

TMP_WORKDIR="$(mktemp -d /tmp/mini-git-work-XXXXXX)"
TMP_REPO="$(mktemp -d /tmp/mini-git-repo-XXXXXX)"

echo "[mini-git] 工作目录: ${TMP_WORKDIR}"
echo "[mini-git] 仓库目录: ${TMP_REPO}"

BIN="${BUILD_DIR}/minigit_cli"

if [ ! -x "${BIN}" ]; then
  echo "[mini-git] 错误: 未找到可执行文件 ${BIN}" >&2
  exit 1
fi

cd "${TMP_WORKDIR}"

echo "hello mini-git" > file1.txt
mkdir -p dir/sub
echo "sub file" > dir/sub/file2.txt

echo "[mini-git] 使用 hash-object 存储单个文件..."
HASH1="$("${BIN}" hash-object file1.txt)"
echo "[mini-git] blob 哈希: ${HASH1}"

echo "[mini-git] 使用 write-tree 构建目录快照..."
TREE_HASH="$("${BIN}" write-tree)"
echo "[mini-git] tree 哈希: ${TREE_HASH}"

if [ -z "${TREE_HASH}" ]; then
  echo "[mini-git] 错误: write-tree 返回空哈希" >&2
  exit 1
fi

echo "[mini-git] 回测成功完成。"

