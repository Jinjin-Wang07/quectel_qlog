#!/bin/bash

# 检查输入参数的数量
if [ "$#" -ne 1 ]; then
    echo "Error, eg: "$0" <file_path> <file_name>"
    exit 1
fi

# 获取第一个参数
file_path="$1"

# 使用basename获取文件名
filename=$(basename "$file_path")

echo "File Name: $filename"

# 检查文件路径是否存在
if [ ! -e "$file_path" ]; then
    echo "错误：文件或目录 '$file_path' 不存在。"
    exit 1
fi

# 进入文件目录
cd "$(dirname "$file_path")" || exit
echo "已进入目录: $(pwd)"

# 检查文件名是否存在
if [ ! -f "$filename" ]; then
    echo "错误：文件 '$filename' 不存在。"
    exit 1
fi

# 检查文件是否存在
if [ ! -f "$filename" ]; then
    echo "ERROR: file: '$filename' do not exit."
    exit 1
fi

# 压缩文件
# gzip "$filename"
zip -9 -m "$filename.zip" "$filename"


# 检查压缩是否成功
if [ $? -eq 0 ]; then
    echo "file: '$filename' compress successful."
else
    echo "ERROR:  '$filename' compress fail."
    exit 1
fi
