#include "md5.h"

void md5_init(MD5_CTX *ctx) {
    ctx->bit_len = 0;
    memcpy(ctx->state, initial_state, sizeof(initial_state));
}

void md5_transform(MD5_CTX *ctx, const uint8_t block[64]) {
    uint32_t a = ctx->state[0], b = ctx->state[1], c = ctx->state[2], d = ctx->state[3];
    uint32_t m[16], f, g, temp;
    int i;

    for (i = 0; i < 16; i++) {
        m[i] = (uint32_t)block[i*4] | ((uint32_t)block[i*4+1] << 8) |
               ((uint32_t)block[i*4+2] << 16) | ((uint32_t)block[i*4+3] << 24);
    }

    for (i = 0; i < 64; i++) {
        if (i < 16) {
            f=F(b,c,d);
            g=i;
        }
        else if (i <32) {
            f=G(b,c,d);
            g=(5*i+1)%16;
        }
        else if (i <48) {
            f=H(b,c,d);
            g=(3*i+5)%16;
        }
        else {
            f=I(b,c,d);
            g=(7*i)%16;
        }

        temp = d;
        d = c;
        c = b;
        b = b + LEFT_ROTATE((a + f + k[i] + m[g]), s[i]);
        a = temp;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
}

void md5_update(MD5_CTX *ctx, const uint8_t *data, size_t len) {
    size_t i, j = ctx->bit_len / 8 % 64;
    ctx->bit_len += len * 8;
    for (i = 0; i < len; i++) {
        ctx->data[j++] = data[i];
        if (j == 64) {
            md5_transform(ctx, ctx->data);
            j = 0;
        }
    }
}

void md5_final(MD5_CTX *ctx, uint8_t digest[16]) {
    size_t i = ctx->bit_len / 8 % 64;

    ctx->data[i++] = 0x80;

    if (i > 56) {
        memset(ctx->data + i, 0, 64 - i);
        md5_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    } else {
        memset(ctx->data + i, 0, 56 - i);
    }

    uint64_t bit_len = ctx->bit_len;
    for (i = 0; i < 8; i++) {
        ctx->data[56+i] = bit_len >> (8*i);
    }

    md5_transform(ctx, ctx->data);

    for (i = 0; i < 4; i++) {
        digest[i*4]   = ctx->state[i] & 0xff;
        digest[i*4+1] = (ctx->state[i] >> 8) & 0xff;
        digest[i*4+2] = (ctx->state[i] >> 16) & 0xff;
        digest[i*4+3] = (ctx->state[i] >> 24) & 0xff;
    }
}

void md5_to_string(const uint8_t digest[16], char *str) {
    const char *hex = "0123456789abcdef";
    for (int i = 0; i < 16; i++) {
        str[i*2] = hex[digest[i] >> 4];
        str[i*2+1] = hex[digest[i] & 0x0f];
    }
    str[32] = '\0';
}

int file_md5(const char *filename, char *md5_str) {
    MD5_CTX ctx;
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("fopen failed");
        return -1;
    }
    md5_init(&ctx);
    uint8_t buf[4096];
    size_t read_len;
    while ((read_len = fread(buf, 1, sizeof(buf), fp)) > 0) {
        md5_update(&ctx, buf, read_len);
    }
    if (ferror(fp)) {
        perror("fread failed");
        fclose(fp);
        return -1;
    }
    fclose(fp);
    uint8_t digest[16];
    md5_final(&ctx, digest);
    md5_to_string(digest, md5_str);
    return 0;
}

/**
 * 读取并解析校验文件内容
 *
 * 功能说明：
 * 1. 打开校验文件并读取第一行内容
 * 2. 解析出 MD5 哈希值和对应的文件名（如果存在）
 * 3. 验证 MD5 格式是否正确（32位十六进制字符）
 * 4. 将 MD5 统一转换为小写
 * 5. 提取并清理文件名（去除前导空格、转义符和换行符）
 * @param checksum_path  校验文件的路径
 * @param expected_md5   输出缓冲区，用于存储解析出的 32 位 MD5 字符串
 * @param recorded_name  输出缓冲区，用于存储校验文件中记录的文件名（可为空）
 * @return 0 成功，-1 失败
 */
static int read_verify_file(const char *checksum_path, char expected_md5[33])
{
    FILE *fp = fopen(checksum_path, "r");
    char line[8192];
    char recorded_field[4096] = "";

    if (fp == NULL) {
        fprintf(stderr, "无法打开校验文件：%s\n", checksum_path);
        return -1;
    }
    else if (fgets(line, sizeof(line), fp) == NULL) {
        fprintf(stderr, "无法从校验文件读取 MD5：%s\n", checksum_path);
        fclose(fp);
        return -1;
    }
    else if (sscanf(line, "%32s %4095[^\n]", expected_md5,
                recorded_field) < 1 || strlen(expected_md5) != 32) {
        fprintf(stderr, "校验文件中的 MD5 长度错误：%s\n", checksum_path);
        return -1;
    }
    else for (int i = 0; i < 32; i++) {
        if (!isxdigit((unsigned char)expected_md5[i])) {
            fprintf(stderr, "校验文件中的 MD5 格式错误：%s\n", checksum_path);
            return -1;
        }
        expected_md5[i] = (char)tolower((unsigned char)expected_md5[i]);
    }

    fclose(fp);
    return 0;
}
/**
 * 校验文件的 MD5 值是否与校验文件中存储的期望值一致
 *
 * 功能说明：
 * 1. 计算目标文件的实际 MD5 哈希值
 * 2. 从校验文件中读取期望的 MD5 值
 * 3. 对比两个值是否一致，输出校验结果
 * @param check_file_path  需要校验的目标文件路径
 * @param checksum_path    存储期望 MD5 值的校验文件路径
 * @return 0 校验通过，1 校验失败（计算失败、读取失败或值不匹配）
 */
int compare_flie(const char *check_file_path, const char *checksum_path)
{
    char md5_str[33];
    char expected_md5[33];

    if (file_md5(check_file_path, md5_str) != 0) {
        fprintf(stderr, "无法计算文件的 MD5：%s\n", check_file_path);
        return 1;
    }
    else if (read_verify_file(checksum_path, expected_md5) != 0) {
        return 1;
    }
    else if (strcmp(md5_str, expected_md5) == 0) {
        LOG_INFO(U_ICON_SUCCESS "校验通过：%s\n" RESET, md5_str );
        LOG_INFO("Verify file value：%s\n", expected_md5);
        LOG_INFO("Calculate values：%s\n", md5_str);
        return 0;
    }else {
        LOG_INFO(U_ICON_FAIL "校验失败：%s\n" RESET, check_file_path);
        LOG_INFO("期望值：%s\n", expected_md5);
        LOG_INFO("实际值：%s\n", md5_str);
        return 1;
    }
}

/* ==================== 独立 CLI 入口 ====================
 * 仅在定义 ENABLE_MD5_CLI 时编译，用于主机端独立计算/校验 MD5
 *
 * 用法:
 *   ./md5sum <文件路径>                # 计算文件 MD5
 *   ./md5sum -c <文件路径> <校验文件>   # 校验文件 MD5
 *
 * 编译:
 *   gcc -DENABLE_MD5_CLI md5.c -I.. -I../.. -o md5sum
 */
#ifdef ENABLE_MD5_CLI

static void print_usage(const char *program_name)
{
    printf("用法: %s [选项] <文件路径>\n\n", program_name);
    printf("选项:\n");
    printf("  -c <文件> <校验文件>   校验文件 MD5 是否与校验文件一致\n");
    printf("  -h                    显示此帮助信息\n");
    printf("\n示例:\n");
    printf("  %s firmware.bin                # 计算 firmware.bin 的 MD5\n", program_name);
    printf("  %s -c firmware.bin firmware.md5 # 校验 firmware.bin 的 MD5\n", program_name);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "-c") == 0) {
        if (argc < 4) {
            fprintf(stderr, "错误: -c 选项需要两个参数\n");
            print_usage(argv[0]);
            return 1;
        }
        return compare_flie(argv[2], argv[3]);
    }

    // 默认：计算文件 MD5
    {
        char md5_str[33];
        if (file_md5(argv[1], md5_str) != 0) {
            fprintf(stderr, "错误: 无法计算文件 MD5: %s\n", argv[1]);
            return 1;
        }
        printf("%s  %s\n", md5_str, argv[1]);
    }

    return 0;
}
#endif /* ENABLE_MD5_CLI */