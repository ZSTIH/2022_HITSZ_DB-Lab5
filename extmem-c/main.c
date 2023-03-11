#include "utils.h"

void linear_select(Buffer *buf, int c_value);
void tpmms(Buffer *buf);
void index_select(Buffer *buf, int c_value);
void sort_merge_join(Buffer *buf);
void intersect(Buffer *buf);
void union_set(Buffer *buf);
void difference(Buffer *buf);

int main()
{
    system("color 07");

    Buffer buf;
    if (!initBuffer(520, 64, &buf))
    {
        perror("Buffer Initialization Failed!\n");
        return -1;
    }

    linear_select(&buf, 128);
    tpmms(&buf);
    index_select(&buf, 128);
    sort_merge_join(&buf);
    intersect(&buf);
    union_set(&buf);
    difference(&buf);

    return 0;
}

// 任务一: 基于线性搜索的关系选择算法(选出 S.C 的值为 c_value 的元组)
void linear_select(Buffer *buf, int c_value)
{
    int output_disk_num = LINEAR_SELECT_OUTPUT_DISK_NUM;
    printf("\n---------------------------------------------------------------------------------\n");
    printf(RED "基于线性搜索的选择算法 S.C=%d\n" COLOR_NONE, c_value);
    printf("---------------------------------------------------------------------------------\n");

    // blk_read 是从磁盘中读取的数据块, blk_write 则是要写入磁盘的数据块
    unsigned char *blk_read, *blk_write;
    blk_write = getNewBlockInBuffer(buf);
    memset(blk_write, 0, buf->blkSize);
    int record_count = 0, index = 17;

    while (index <= 48)
    {
        if ((blk_read = readBlockFromDisk(index, buf)) == NULL)
        {
            perror("Reading Block Failed!\n");
            return;
        }
        printf("读入数据块%d\n", index);
        for (int i = 0; i < 7; i++)
        {
            int x = getIntFromBlock(blk_read + i * 8);
            int y = getIntFromBlock(blk_read + 4 + i * 8);
            if (x == c_value)
            {
                if ((record_count != 0) && (record_count % 7 == 0))
                {
                    // record_count 不为 0 且为 7 的倍数时, 说明已经写满了一个缓冲区数据块
                    writeTupleToDisk(blk_write, buf, output_disk_num, PRINT);
                    output_disk_num++;
                }
                writeTupleToBlock(blk_write + 8 * (record_count % 7), x, y);
                printf("(X=%d, Y=%d)\n", x, y);
                record_count++;
            }
        }
        index++;
        freeBlockInBuffer(blk_read, buf);
    }

    // 将剩余记录(不超过 7 个的那部分)写入磁盘
    if (writeBlockToDisk(blk_write, output_disk_num, buf) != 0)
    {
        perror("Writing Block Failed!\n");
        return;
    }
    printf("注：结果写入磁盘：%d\n", output_disk_num);

    printf(RED "满足选择条件的元组一共有%d个。\n" COLOR_NONE, record_count);
    printf(RED "IO读写一共有%lu次。\n" COLOR_NONE, buf->numIO);
    freeBuffer(buf);
}

// 任务二: 两阶段多路归并排序算法
void tpmms(Buffer *buf)
{
    printf("\n---------------------------------------------------------------------------------\n");
    printf(RED "两阶段多路归并排序算法\n" COLOR_NONE);
    printf("---------------------------------------------------------------------------------\n");
    if (!initBuffer(520, 64, buf))
    {
        perror("Buffer Initialization Failed!\n");
        return;
    }

    printf("\n---------------------------------------------------------------------------------\n");
    printf(RED "对关系 R 进行第一趟排序\n" COLOR_NONE);
    printf("---------------------------------------------------------------------------------\n");
    tpmms_first_stage(buf, 1, 16, TPMMS_FIRST_OUTPUT_DISK_NUM);

    printf("\n---------------------------------------------------------------------------------\n");
    printf(RED "对关系 R 进行第二趟排序\n" COLOR_NONE);
    printf("---------------------------------------------------------------------------------\n");
    tpmms_second_stage(buf, TPMMS_FIRST_OUTPUT_DISK_NUM, TPMMS_FIRST_OUTPUT_DISK_NUM + 15, TPMMS_SECOND_OUTPUT_DISK_NUM);

    printf("关系 R 进行" RED "两阶段多路归并排序" COLOR_NONE "后的最终结果已经被写入到磁盘块%d至磁盘块%d\n", TPMMS_SECOND_OUTPUT_DISK_NUM, TPMMS_SECOND_OUTPUT_DISK_NUM + 15);

    printf("\n---------------------------------------------------------------------------------\n");
    printf(RED "对关系 S 进行第一趟排序\n" COLOR_NONE);
    printf("---------------------------------------------------------------------------------\n");
    tpmms_first_stage(buf, 17, 48, TPMMS_FIRST_OUTPUT_DISK_NUM + 16);

    printf("\n---------------------------------------------------------------------------------\n");
    printf(RED "对关系 S 进行第二趟排序\n" COLOR_NONE);
    printf("---------------------------------------------------------------------------------\n");
    tpmms_second_stage(buf, TPMMS_FIRST_OUTPUT_DISK_NUM + 16, TPMMS_FIRST_OUTPUT_DISK_NUM + 47, TPMMS_SECOND_OUTPUT_DISK_NUM + 16);

    printf("关系 S 进行" RED "两阶段多路归并排序" COLOR_NONE "后的最终结果已经被写入到磁盘块%d至磁盘块%d\n" COLOR_NONE, TPMMS_SECOND_OUTPUT_DISK_NUM + 16, TPMMS_SECOND_OUTPUT_DISK_NUM + 47);

    freeBuffer(buf);
}

// 任务三: 基于索引的关系选择算法(选出 S.C 的值为 c_value 的元组)
void index_select(Buffer *buf, int c_value)
{
    printf("\n---------------------------------------------------------------------------------\n");
    printf(RED "基于索引的选择算法 S.C=%d\n" COLOR_NONE, c_value);
    printf("---------------------------------------------------------------------------------\n");
    if (!initBuffer(520, 64, buf))
    {
        perror("Buffer Initialization Failed!\n");
        return;
    }

    ///////////////////////////////// 建索引 //////////////////////////////////////

    int output_disk_num = INDEX_OUTPUT_DISK_NUM;
    unsigned char *blk_read, *blk_write;
    blk_write = getNewBlockInBuffer(buf);
    memset(blk_write, 0, buf->blkSize);
    int tuple_count = 0;                           // 已经建立的索引记录数目
    int blkno = TPMMS_SECOND_OUTPUT_DISK_NUM + 16; // 直接使用任务二已经排序好的数据
    int index_value = -1;                          // 为第一个 S.C 的值为 index_value 的记录建立索引
    while (blkno < TPMMS_SECOND_OUTPUT_DISK_NUM + 48)
    {
        if ((blk_read = readBlockFromDisk(blkno, buf)) == NULL)
        {
            perror("Reading Block Failed!\n");
            return;
        }
        for (int i = 0; i < 7; i++)
        {
            int x = getIntFromBlock(blk_read + i * 8);
            if (x != index_value)
            {
                // 发现新值, 需要建立索引
                if ((tuple_count != 0) && (tuple_count % 7 == 0))
                {
                    writeTupleToDisk(blk_write, buf, output_disk_num, NOT_PRINT);
                    output_disk_num++;
                }
                // 将当前记录的块号保存下来, 便于之后根据索引进行检索
                writeTupleToBlock(blk_write + 8 * (tuple_count % 7), x, blkno);
                tuple_count++;
                index_value = x;
            }
        }
        blkno++;
        freeBlockInBuffer(blk_read, buf);
    }

    // 将剩余记录(不超过 7 个的那部分)写入磁盘
    if (writeBlockToDisk(blk_write, output_disk_num, buf) != 0)
    {
        perror("Writing Block Failed!\n");
        return;
    }

    ///////////////////////////////// 基于索引进行关系选择 ///////////////////////////////

    freeBuffer(buf);
    if (!initBuffer(520, 64, buf))
    {
        perror("Buffer Initialization Failed!\n");
        return;
    }
    // 记录上一个 output_disk_num 的值, 从而确定存放了索引的数据块数
    int last_output_disk_num = output_disk_num;
    output_disk_num = INDEX_SELECT_OUTPUT_DISK_NUM;
    blk_write = getNewBlockInBuffer(buf);
    memset(blk_write, 0, buf->blkSize);
    tuple_count = 0;
    blkno = INDEX_OUTPUT_DISK_NUM;
    int blkno_find = 0;
    // 先找到当前 S.C 值下的索引
    for (;;)
    {
        if ((blk_read = readBlockFromDisk(blkno, buf)) == NULL)
        {
            perror("Reading Block Failed!\n");
            return;
        }
        printf("读入索引块%d\n", blkno);
        for (int i = 0; i < 7; i++)
        {
            int x = getIntFromBlock(blk_read + i * 8);
            int y = getIntFromBlock(blk_read + i * 8 + 4);
            if (x > c_value) // 由于按升序排列, 此时可提前终止循环
                break;
            if (x == c_value) // 此时已找到需要的索引, 同样可提前终止
            {
                blkno_find = y; // y 值就是第一个 S.C 值为 c_value 的元组所在的块号
                break;
            }
        }
        blkno++;
        freeBlockInBuffer(blk_read, buf);
        if (blkno_find || (blkno > last_output_disk_num)) // 若找到或已经找完全部索引块, 直接退出
            break;
        printf("没有满足条件的元组。\n"); // 当前索引块没有找到所需的记录
    }
    if (blkno_find == 0)
    {
        printf("已查找完全部索引块。没有满足条件的元组。\n");
        return;
    }
    // 再根据该索引找到满足要求的数据块
    blkno = blkno_find;
    int no_more_flag = 0;
    for (;;)
    {
        if ((blk_read = readBlockFromDisk(blkno, buf)) == NULL)
        {
            perror("Reading Block Failed!\n");
            return;
        }
        printf("读入数据块%d\n", blkno);
        for (int i = 0; i < 7; i++)
        {
            int x = getIntFromBlock(blk_read + i * 8);
            int y = getIntFromBlock(blk_read + i * 8 + 4);
            if (x == c_value)
            {
                if ((tuple_count != 0) && (tuple_count % 7 == 0))
                {
                    // tuple_count 不为 0 且为 7 的倍数时, 说明已经写满了一个缓冲区数据块
                    writeTupleToDisk(blk_write, buf, output_disk_num, PRINT);
                    output_disk_num++;
                }
                writeTupleToBlock(blk_write + 8 * (tuple_count % 7), x, y);
                printf("(X=%d, Y=%d)\n", x, y);
                tuple_count++;
            }
            if (x > c_value)
            {
                // 由于按升序排列, 当 x 大于 c_value 时, 之后已经不可能再找到
                no_more_flag = 1;
                break;
            }
        }
        if (no_more_flag)
            break;
        blkno++;
        freeBlockInBuffer(blk_read, buf);
    }

    // 将剩余记录(不超过 7 个的那部分)写入磁盘
    if (writeBlockToDisk(blk_write, output_disk_num, buf) != 0)
    {
        perror("Writing Block Failed!\n");
        return;
    }
    printf("注：结果写入磁盘：%d\n", output_disk_num);

    printf(RED "满足选择条件的元组一共有%d个。\n" COLOR_NONE, tuple_count);
    printf(RED "IO读写一共有%lu次。\n" COLOR_NONE, buf->numIO);
    freeBuffer(buf);
}

// 任务四: 基于排序的连接操作算法
void sort_merge_join(Buffer *buf)
{
    printf("\n---------------------------------------------------------------------------------\n");
    printf(RED "基于排序的连接算法\n" COLOR_NONE);
    printf("---------------------------------------------------------------------------------\n");
    if (!initBuffer(520, 64, buf))
    {
        perror("Buffer Initialization Failed!\n");
        return;
    }
    int output_disk_num = SORT_MERGE_JOIN_OUTPUT_DISK_NUM;
    int tuple_count = 0;
    unsigned char *blk_R, *blk_S, *blk_write /*, *blk_backup */;
    blk_write = getNewBlockInBuffer(buf);
    memset(blk_write, 0, buf->blkSize);
    // blk_backup = getNewBlockInBuffer(buf);
    int blkno_r = TPMMS_SECOND_OUTPUT_DISK_NUM;      // 关系 R 当前访问到的数据块
    int blkno_s = TPMMS_SECOND_OUTPUT_DISK_NUM + 16; // 关系 S 当前访问到的数据块
    int blkno_r_upper = blkno_r + 16;                // 关系 R 数据块号的上界
    int blkno_s_upper = blkno_s + 32;                // 关系 S 数据块号的上界
    int offset_r = 0;                                // 关系 R 的数据块内偏移
    int offset_s = 0;                                // 关系 S 的数据块内偏移
    int blkno_backup, offset_backup;
    if ((blk_R = readBlockFromDisk(blkno_r, buf)) == NULL)
    {
        perror("Reading Block Failed!\n");
        return;
    }
    if ((blk_S = readBlockFromDisk(blkno_s, buf)) == NULL)
    {
        perror("Reading Block Failed!\n");
        return;
    }

    // strcpy(blk_backup, blk_S);
    while ((blkno_r < blkno_r_upper) && (blkno_s < blkno_s_upper))
    {
        blkno_backup = blkno_s;
        offset_backup = offset_s;
        while (getIntFromBlock(blk_R + offset_r) == getIntFromBlock(blk_S + offset_s))
        {
            // 一次成功的连接需要写入 S 和 R 中的两个元组
            if ((tuple_count != 0) && (tuple_count % 7 == 0))
            {
                writeTupleToDisk(blk_write, buf, output_disk_num, PRINT);
                output_disk_num++;
            }
            writeTupleToBlock(blk_write + 8 * (tuple_count % 7), getIntFromBlock(blk_S + offset_s), getIntFromBlock(blk_S + offset_s + 4));
            tuple_count++;
            if ((tuple_count != 0) && (tuple_count % 7 == 0))
            {
                writeTupleToDisk(blk_write, buf, output_disk_num, PRINT);
                output_disk_num++;
            }
            writeTupleToBlock(blk_write + 8 * (tuple_count % 7), getIntFromBlock(blk_R + offset_r), getIntFromBlock(blk_R + offset_r + 4));
            tuple_count++;

            // 关系 R 不动, 关系 S 继续向下搜索
            if (offset_s < 48)
            {
                offset_s += 8;
            }
            else
            {
                // 此时已经访问完一个数据块的 7 个元组, 需要访问下一个数据块
                blkno_s++;
                freeBlockInBuffer(blk_S, buf);
                if (blkno_s >= blkno_s_upper) // 关系 S 已经被访问完了
                    break;
                if ((blk_S = readBlockFromDisk(blkno_s, buf)) == NULL)
                {
                    perror("Reading Block Failed!\n");
                    return;
                }
                offset_s = 0;
            }
        }
        // 回退关系 S
        // 原本直接使用 "strcpy(blk_S, blk_backup);" , 但是 blk_backup 指针所指向的数据块有可能被释放了
        blkno_s = blkno_backup;
        freeBlockInBuffer(blk_S, buf);
        if ((blk_S = readBlockFromDisk(blkno_s, buf)) == NULL)
        {
            perror("Reading Block Failed!\n");
            return;
        }
        offset_s = offset_backup;

        // 接下来 R 和 S 当中更小的一者往下访问
        if (getIntFromBlock(blk_R + offset_r) <= getIntFromBlock(blk_S + offset_s))
        {
            // R 继续往下搜索
            if (offset_r < 48)
            {
                offset_r += 8;
            }
            else
            {
                // 此时已经访问完一个数据块的 7 个元组, 需要访问下一个数据块
                blkno_r++;
                freeBlockInBuffer(blk_R, buf);
                if (blkno_r >= blkno_r_upper) // 关系 R 已经被访问完了
                    break;
                if ((blk_R = readBlockFromDisk(blkno_r, buf)) == NULL)
                {
                    perror("Reading Block Failed!\n");
                    return;
                }
                offset_r = 0;
            }
        }
        else
        {
            // S 继续往下搜索
            if (offset_s < 48)
            {
                offset_s += 8;
            }
            else
            {
                // 此时已经访问完一个数据块的 7 个元组, 需要访问下一个数据块
                blkno_s++;
                freeBlockInBuffer(blk_S, buf);
                if (blkno_s >= blkno_s_upper) // 关系 S 已经被访问完了
                    break;
                if ((blk_S = readBlockFromDisk(blkno_s, buf)) == NULL)
                {
                    perror("Reading Block Failed!\n");
                    return;
                }
                // strcpy(blk_backup, blk_S);
                offset_s = 0;
            }
        }
    }
    // 将剩余记录(不超过 7 个的那部分)写入磁盘
    if (writeBlockToDisk(blk_write, output_disk_num, buf) != 0)
    {
        perror("Writing Block Failed!\n");
        return;
    }
    printf("注：结果写入磁盘：%d\n", output_disk_num);

    printf("全部连接结果被写入了磁盘%d至磁盘%d。\n", SORT_MERGE_JOIN_OUTPUT_DISK_NUM, output_disk_num);
    printf(RED "总共连接了%d次。\n" COLOR_NONE, tuple_count / 2);
    freeBuffer(buf);
}

// 任务五: 基于排序的集合的交运算
void intersect(Buffer *buf)
{
    printf("\n---------------------------------------------------------------------------------\n");
    printf(RED "基于排序的集合的交运算\n" COLOR_NONE);
    printf("---------------------------------------------------------------------------------\n");
    if (!initBuffer(520, 64, buf))
    {
        perror("Buffer Initialization Failed!\n");
        return;
    }
    int output_disk_num = SET_INTERSECT_OUTPUT_DISK_NUM;
    int tuple_count = 0;
    unsigned char *blk_R, *blk_S, *blk_write;
    blk_write = getNewBlockInBuffer(buf);
    memset(blk_write, 0, buf->blkSize);
    int blkno_r = TPMMS_SECOND_OUTPUT_DISK_NUM;      // 关系 R 当前访问到的数据块
    int blkno_s = TPMMS_SECOND_OUTPUT_DISK_NUM + 16; // 关系 S 当前访问到的数据块
    int blkno_r_upper = blkno_r + 16;                // 关系 R 数据块号的上界
    int blkno_s_upper = blkno_s + 32;                // 关系 S 数据块号的上界
    int offset_r = 0;                                // 关系 R 的数据块内偏移
    int offset_s = 0;                                // 关系 S 的数据块内偏移
    if ((blk_R = readBlockFromDisk(blkno_r, buf)) == NULL)
    {
        perror("Reading Block Failed!\n");
        return;
    }
    if ((blk_S = readBlockFromDisk(blkno_s, buf)) == NULL)
    {
        perror("Reading Block Failed!\n");
        return;
    }

    while ((blkno_r < blkno_r_upper) && (blkno_s < blkno_s_upper))
    {
        int xr = getIntFromBlock(blk_R + offset_r);
        int yr = getIntFromBlock(blk_R + offset_r + 4);
        int xs = getIntFromBlock(blk_S + offset_s);
        int ys = getIntFromBlock(blk_S + offset_s + 4);
        if ((xr == xs) && (yr == ys))
        {
            // 当 R 和 S 所指向的记录完全相同时, 两者同时向后访问一个记录
            if ((tuple_count != 0) && (tuple_count % 7 == 0))
            {
                writeTupleToDisk(blk_write, buf, output_disk_num, PRINT);
                output_disk_num++;
            }
            writeTupleToBlock(blk_write + 8 * (tuple_count % 7), xr, yr);
            tuple_count++;
            printf("(X=%d, Y=%d)\n", xr, yr);
            if (offset_r < 48) // 移动 R
            {
                offset_r += 8;
            }
            else
            {
                // 此时已经访问完一个数据块的 7 个元组, 需要访问下一个数据块
                blkno_r++;
                freeBlockInBuffer(blk_R, buf);
                if (blkno_r >= blkno_r_upper) // 关系 R 已经被访问完了
                    break;
                if ((blk_R = readBlockFromDisk(blkno_r, buf)) == NULL)
                {
                    perror("Reading Block Failed!\n");
                    return;
                }
                offset_r = 0;
            }
            if (offset_s < 48) // 移动 S
            {
                offset_s += 8;
            }
            else
            {
                // 此时已经访问完一个数据块的 7 个元组, 需要访问下一个数据块
                blkno_s++;
                freeBlockInBuffer(blk_S, buf);
                if (blkno_s >= blkno_s_upper) // 关系 S 已经被访问完了
                    break;
                if ((blk_S = readBlockFromDisk(blkno_s, buf)) == NULL)
                {
                    perror("Reading Block Failed!\n");
                    return;
                }
                offset_s = 0;
            }
        }
        // 当 R 和 S 所指向的记录不相等时, 值更小的一者向后访问
        else if ((xr < xs) || ((xr == xs) && (yr < ys)))
        {
            if (offset_r < 48) // 移动 R
            {
                offset_r += 8;
            }
            else
            {
                // 此时已经访问完一个数据块的 7 个元组, 需要访问下一个数据块
                blkno_r++;
                freeBlockInBuffer(blk_R, buf);
                if (blkno_r >= blkno_r_upper) // 关系 R 已经被访问完了
                    break;
                if ((blk_R = readBlockFromDisk(blkno_r, buf)) == NULL)
                {
                    perror("Reading Block Failed!\n");
                    return;
                }
                offset_r = 0;
            }
        }
        else
        {
            if (offset_s < 48) // 移动 S
            {
                offset_s += 8;
            }
            else
            {
                // 此时已经访问完一个数据块的 7 个元组, 需要访问下一个数据块
                blkno_s++;
                freeBlockInBuffer(blk_S, buf);
                if (blkno_s >= blkno_s_upper) // 关系 S 已经被访问完了
                    break;
                if ((blk_S = readBlockFromDisk(blkno_s, buf)) == NULL)
                {
                    perror("Reading Block Failed!\n");
                    return;
                }
                offset_s = 0;
            }
        }
    }
    // 将剩余记录(不超过 7 个的那部分)写入磁盘
    if (writeBlockToDisk(blk_write, output_disk_num, buf) != 0)
    {
        perror("Writing Block Failed!\n");
        return;
    }
    printf("注：结果写入磁盘：%d\n", output_disk_num);

    printf(RED "R和S的交集有%d个元组。\n" COLOR_NONE, tuple_count);
    freeBuffer(buf);
}

// 附加题: 基于排序的集合的并运算
void union_set(Buffer *buf)
{
    printf("\n---------------------------------------------------------------------------------\n");
    printf(RED "基于排序的集合的并运算\n" COLOR_NONE);
    printf("---------------------------------------------------------------------------------\n");
    if (!initBuffer(520, 64, buf))
    {
        perror("Buffer Initialization Failed!\n");
        return;
    }
    int output_disk_num = SET_UNION_OUTPUT_DISK_NUM;
    int tuple_count = 0;
    unsigned char *blk_R, *blk_S, *blk_write;
    blk_write = getNewBlockInBuffer(buf);
    memset(blk_write, 0, buf->blkSize);
    int blkno_r = TPMMS_SECOND_OUTPUT_DISK_NUM;      // 关系 R 当前访问到的数据块
    int blkno_s = TPMMS_SECOND_OUTPUT_DISK_NUM + 16; // 关系 S 当前访问到的数据块
    int blkno_r_upper = blkno_r + 16;                // 关系 R 数据块号的上界
    int blkno_s_upper = blkno_s + 32;                // 关系 S 数据块号的上界
    int offset_r = 0;                                // 关系 R 的数据块内偏移
    int offset_s = 0;                                // 关系 S 的数据块内偏移
    if ((blk_R = readBlockFromDisk(blkno_r, buf)) == NULL)
    {
        perror("Reading Block Failed!\n");
        return;
    }
    if ((blk_S = readBlockFromDisk(blkno_s, buf)) == NULL)
    {
        perror("Reading Block Failed!\n");
        return;
    }

    while ((blkno_r < blkno_r_upper) || (blkno_s < blkno_s_upper))
    {
        if ((blkno_r < blkno_r_upper) && (blkno_s < blkno_s_upper) && is_equal(blk_R + offset_r, blk_S + offset_s))
        {
            // 当 R 和 S 所指向的记录完全相同时, 只需要向磁盘写出一次结果
            if ((tuple_count != 0) && (tuple_count % 7 == 0))
            {
                writeTupleToDisk(blk_write, buf, output_disk_num, PRINT);
                output_disk_num++;
            }
            writeTupleToBlock(blk_write + 8 * (tuple_count % 7), getIntFromBlock(blk_R + offset_r), getIntFromBlock(blk_R + offset_r + 4));
            tuple_count++;
            // 然后, 让 R 和 S 都后移一个记录
            if (offset_s < 48) // 移动 S
            {
                offset_s += 8;
            }
            else
            {
                // 此时已经访问完一个数据块的 7 个元组, 需要访问下一个数据块
                blkno_s++;
                freeBlockInBuffer(blk_S, buf);
                if (blkno_s >= blkno_s_upper) // 关系 S 已经被访问完了
                    continue;                 // 此处不能用 break , 而要用continue. 因为 S 访问完了还可能有 R 要访问, break会跳出整个循环.
                if ((blk_S = readBlockFromDisk(blkno_s, buf)) == NULL)
                {
                    perror("Reading Block Failed!\n");
                    return;
                }
                offset_s = 0;
            }
            if (offset_r < 48) // 移动 R
            {
                offset_r += 8;
            }
            else
            {
                // 此时已经访问完一个数据块的 7 个元组, 需要访问下一个数据块
                blkno_r++;
                freeBlockInBuffer(blk_R, buf);
                if (blkno_r >= blkno_r_upper) // 关系 R 已经被访问完了
                    continue;                 // 此处不能用 break , 而要用continue. 因为 R 访问完了还可能有 S 要访问, break会跳出整个循环.
                if ((blk_R = readBlockFromDisk(blkno_r, buf)) == NULL)
                {
                    perror("Reading Block Failed!\n");
                    return;
                }
                offset_r = 0;
            }
        }
        else if ((blkno_s >= blkno_s_upper) || ((blkno_r < blkno_r_upper) && is_smaller(blk_R + offset_r, blk_S + offset_s)))
        {
            // 关系 S 为空或关系 R 的值小于关系 S 时, 均是让关系 R 输出并向后访问
            if ((tuple_count != 0) && (tuple_count % 7 == 0))
            {
                writeTupleToDisk(blk_write, buf, output_disk_num, PRINT);
                output_disk_num++;
            }
            writeTupleToBlock(blk_write + 8 * (tuple_count % 7), getIntFromBlock(blk_R + offset_r), getIntFromBlock(blk_R + offset_r + 4));
            tuple_count++;
            if (offset_r < 48) // 移动 R
            {
                offset_r += 8;
            }
            else
            {
                // 此时已经访问完一个数据块的 7 个元组, 需要访问下一个数据块
                blkno_r++;
                freeBlockInBuffer(blk_R, buf);
                if (blkno_r >= blkno_r_upper) // 关系 R 已经被访问完了
                    continue;
                if ((blk_R = readBlockFromDisk(blkno_r, buf)) == NULL)
                {
                    perror("Reading Block Failed!\n");
                    return;
                }
                offset_r = 0;
            }
        }
        else
        {
            // 其它情况下, 均是关系 S 输出并向后访问
            if ((tuple_count != 0) && (tuple_count % 7 == 0))
            {
                writeTupleToDisk(blk_write, buf, output_disk_num, PRINT);
                output_disk_num++;
            }
            writeTupleToBlock(blk_write + 8 * (tuple_count % 7), getIntFromBlock(blk_S + offset_s), getIntFromBlock(blk_S + offset_s + 4));
            tuple_count++;
            if (offset_s < 48) // 移动 S
            {
                offset_s += 8;
            }
            else
            {
                // 此时已经访问完一个数据块的 7 个元组, 需要访问下一个数据块
                blkno_s++;
                freeBlockInBuffer(blk_S, buf);
                if (blkno_s >= blkno_s_upper) // 关系 S 已经被访问完了
                    continue;
                if ((blk_S = readBlockFromDisk(blkno_s, buf)) == NULL)
                {
                    perror("Reading Block Failed!\n");
                    return;
                }
                offset_s = 0;
            }
        }
    }

    // 将剩余记录(不超过 7 个的那部分)写入磁盘
    if (writeBlockToDisk(blk_write, output_disk_num, buf) != 0)
    {
        perror("Writing Block Failed!\n");
        return;
    }
    printf("注：结果写入磁盘：%d\n", output_disk_num);

    printf(RED "R和S的并集有%d个元组。\n" COLOR_NONE, tuple_count);
    freeBuffer(buf);
}

// 附加题: 基于排序的集合的差运算
void difference(Buffer *buf)
{
    printf("\n---------------------------------------------------------------------------------\n");
    printf(RED "基于排序的集合的差运算\n" COLOR_NONE);
    printf("---------------------------------------------------------------------------------\n");
    if (!initBuffer(520, 64, buf))
    {
        perror("Buffer Initialization Failed!\n");
        return;
    }
    int output_disk_num = SET_DIFFERENCE_OUTPUT_DISK_NUM;
    int tuple_count = 0;
    unsigned char *blk_R, *blk_S, *blk_write;
    blk_write = getNewBlockInBuffer(buf);
    memset(blk_write, 0, buf->blkSize);
    int blkno_r = TPMMS_SECOND_OUTPUT_DISK_NUM;      // 关系 R 当前访问到的数据块
    int blkno_s = TPMMS_SECOND_OUTPUT_DISK_NUM + 16; // 关系 S 当前访问到的数据块
    int blkno_r_upper = blkno_r + 16;                // 关系 R 数据块号的上界
    int blkno_s_upper = blkno_s + 32;                // 关系 S 数据块号的上界
    int offset_r = 0;                                // 关系 R 的数据块内偏移
    int offset_s = 0;                                // 关系 S 的数据块内偏移
    if ((blk_R = readBlockFromDisk(blkno_r, buf)) == NULL)
    {
        perror("Reading Block Failed!\n");
        return;
    }
    if ((blk_S = readBlockFromDisk(blkno_s, buf)) == NULL)
    {
        perror("Reading Block Failed!\n");
        return;
    }
    while (blkno_s < blkno_s_upper)
    {
        // 由于计算的是 S - R , 两者相等时不能输出任何元组, S 和 R 都要后移
        if ((blkno_r < blkno_r_upper) && (blkno_s < blkno_s_upper) && (is_equal(blk_R + offset_r, blk_S + offset_s)))
        {
            if (offset_s < 48) // 移动 S
            {
                offset_s += 8;
            }
            else
            {
                // 此时已经访问完一个数据块的 7 个元组, 需要访问下一个数据块
                blkno_s++;
                freeBlockInBuffer(blk_S, buf);
                if (blkno_s >= blkno_s_upper) // 关系 S 已经被访问完了
                    continue;
                if ((blk_S = readBlockFromDisk(blkno_s, buf)) == NULL)
                {
                    perror("Reading Block Failed!\n");
                    return;
                }
                offset_s = 0;
            }
            if (offset_r < 48) // 移动 R
            {
                offset_r += 8;
            }
            else
            {
                // 此时已经访问完一个数据块的 7 个元组, 需要访问下一个数据块
                blkno_r++;
                freeBlockInBuffer(blk_R, buf);
                if (blkno_r >= blkno_r_upper) // 关系 R 已经被访问完了
                    continue;
                if ((blk_R = readBlockFromDisk(blkno_r, buf)) == NULL)
                {
                    perror("Reading Block Failed!\n");
                    return;
                }
                offset_r = 0;
            }
        }
        else if ((blkno_r < blkno_r_upper) && is_smaller(blk_R + offset_r, blk_S + offset_s))
        {
            // 对于 R 小于 S 的情况, 需要让 R 继续向后访问, 且此时不能输出任何元组
            if (offset_r < 48) // 移动 R
            {
                offset_r += 8;
            }
            else
            {
                // 此时已经访问完一个数据块的 7 个元组, 需要访问下一个数据块
                blkno_r++;
                freeBlockInBuffer(blk_R, buf);
                if (blkno_r >= blkno_r_upper) // 关系 R 已经被访问完了
                    continue;
                if ((blk_R = readBlockFromDisk(blkno_r, buf)) == NULL)
                {
                    perror("Reading Block Failed!\n");
                    return;
                }
                offset_r = 0;
            }
        }
        else
        {
            // 其它情况下, 需要让 S 继续向后访问, 且此时要输出 S 中的元组
            if ((tuple_count != 0) && (tuple_count % 7 == 0))
            {
                writeTupleToDisk(blk_write, buf, output_disk_num, PRINT);
                output_disk_num++;
            }
            writeTupleToBlock(blk_write + 8 * (tuple_count % 7), getIntFromBlock(blk_S + offset_s), getIntFromBlock(blk_S + offset_s + 4));
            tuple_count++;
            if (offset_s < 48) // 移动 S
            {
                offset_s += 8;
            }
            else
            {
                // 此时已经访问完一个数据块的 7 个元组, 需要访问下一个数据块
                blkno_s++;
                freeBlockInBuffer(blk_S, buf);
                if (blkno_s >= blkno_s_upper) // 关系 S 已经被访问完了
                    continue;
                if ((blk_S = readBlockFromDisk(blkno_s, buf)) == NULL)
                {
                    perror("Reading Block Failed!\n");
                    return;
                }
                offset_s = 0;
            }
        }
    }

    // 将剩余记录(不超过 7 个的那部分)写入磁盘
    if (writeBlockToDisk(blk_write, output_disk_num, buf) != 0)
    {
        perror("Writing Block Failed!\n");
        return;
    }
    printf("注：结果写入磁盘：%d\n", output_disk_num);

    printf(RED "S和R的差集(S-R)有%d个元组。\n" COLOR_NONE, tuple_count);
    freeBuffer(buf);
}