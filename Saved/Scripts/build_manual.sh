#!/bin/bash
# 手动并行编译 ArcaneVanguard 模块（UBT 失效时的兜底）
# 用法: bash Saved/Scripts/build_manual.sh

export WindowsSdkDir='C:\Program Files (x86)\Windows Kits\10\'
export WindowsSdkVerBinPath='C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64'
export WindowsSDKVersion='10.0.26100.0\'
export PATH="/c/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64:$PATH"

CLBIN="C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.38.33130/bin/Hostx64/x64/cl.exe"
DIR="D:/UEProject/ArcaneVanguard/Intermediate/Build/Win64/x64/UnrealEditor/Development/ArcaneVanguard"
ENGINE_SRC="C:/Program Files/Epic Games/UE_5.4/Engine/Source"
LOGDIR="/tmp/cllogs"
mkdir -p "$LOGDIR"
rm -f "$LOGDIR"/*.txt

export CLBIN ENGINE_SRC LOGDIR

cd "$ENGINE_SRC" || exit 1

# 收集要编的 rsp：跳过 .lc（Live Coding）和陈旧残留
LIST=$(ls "$DIR"/*.cpp.obj.rsp 2>/dev/null | grep -v '\.lc\.' | grep -v 'WeaponNotifies')
COUNT=$(echo "$LIST" | grep -c .)
echo "=== 待编译 $COUNT 个文件 ==="

# [!] 先删掉同名 obj。
#     不删的话，如果某个文件编译失败或编译被中断，旧 obj 会留在原地，
#     后续 link 会报 "LNK2019 无法解析的外部符号"（看起来像代码没写，实际是 obj 过期）。
#     这个坑 2026-10-30 踩过一次，排查花了很久。
echo "=== 清理旧 obj ==="
echo "$LIST" | while read -r f; do
  [ -z "$f" ] && continue
  rm -f "$DIR/$(basename "$f" .obj.rsp).obj"
done

echo "$LIST" | xargs -P 8 -I{} bash -c '
  f="{}"
  base=$(basename "$f")
  cd "$ENGINE_SRC" || exit 1
  MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL="*" "$CLBIN" "@$f" > "$LOGDIR/$base.txt" 2>&1
  rc=$?
  if [ $rc -ne 0 ]; then
    echo "FAIL $base (rc=$rc)"
  fi
'

echo "=== 编译结束，检查失败项 ==="
FAILED=$(grep -l "error" "$LOGDIR"/*.txt 2>/dev/null | head -10)
if [ -n "$FAILED" ]; then
  for f in $FAILED; do
    echo "--- $(basename $f) ---"
    iconv -f GBK -t UTF-8 "$f" 2>/dev/null | grep -iE "error C|error LNK|fatal" | head -3
  done
else
  echo "无错误"
fi

echo "=== obj 数量 ==="
ls "$DIR"/*.obj 2>/dev/null | wc -l
