#!/usr/bin/env bash

set -u

usage() {
	printf '用法:\n'
	printf '  %s generate <文件> [校验文件]\n' "$0"
	printf '  %s check <文件> [校验文件]\n' "$0"
}

if [[ $# -lt 2 || $# -gt 3 ]]; then
	usage >&2
	exit 2
fi

command=$1
file=$2
checksum_file=${3:-"${file}.md5"}

if [[ ! -f $file ]]; then
	printf '错误：文件不存在：%s\n' "$file" >&2
	exit 1
fi

case $command in
	# generate)   //生成校验文件
    g)
		if md5sum -- "$file" > "$checksum_file"; then
			printf '已生成校验文件：%s\n' "$checksum_file"
		else
			printf '错误：无法写入校验文件：%s\n' "$checksum_file" >&2
			exit 1
		fi
		;;
	# check)    //校验文件
    c)
		if [[ ! -f $checksum_file ]]; then
			printf '错误：校验文件不存在：%s\n' "$checksum_file" >&2
			exit 1
		fi

		if md5sum --check --status "$checksum_file"; then
			printf '校验通过：%s\n' "$file"
		else
			printf '校验失败：%s\n' "$file" >&2
			exit 1
		fi
		;;
	*)
		printf '错误：未知操作：%s\n' "$command" >&2
		usage >&2
		exit 2
		;;
esac
