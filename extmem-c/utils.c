#include "utils.h"

char s[5];

// 将缓存区数据块中的字符转换为整型数
int getIntFromBlock(unsigned char *blk)
{
    // R.A, R.B, S.C, S.D 均是三位数
    for (int i = 0; i < 3; i++)
    {
        s[i] = blk[i];
    }
    s[3] = '\0';
    return atoi(s);
}

// 将关系中的元组写回缓存区数据块
void writeTupleToBlock(unsigned char *blk, int x, int y)
{
    for (int i = 0; i < 3; i++)
    {
        blk[2 - i] = x % 10 + '0';
        x /= 10;
    }
    blk[3] = '\0';

    if (y == TUPLE_END_FLAG)
    {
        // y 的值为 TUPLE_END_FLAG 时, 说明已经写满当前块, 故全部填充为空字符
        for (int i = 0; i < 4; i++)
        {
            blk[4 + i] = '\0';
        }
    }
    else
    {
        for (int i = 0; i < 3; i++)
        {
            blk[6 - i] = y % 10 + '0';
            y /= 10;
        }
        blk[7] = '\0';
    }
}

// 当缓存区数据块中的元组写满以后, 将其写回到磁盘中
void writeTupleToDisk(unsigned char *blk, Buffer *buf, int output_disk_num, int print_flag)
{
    writeTupleToBlock(blk + 56, output_disk_num + 1, TUPLE_END_FLAG);
    if (writeBlockToDisk(blk, output_disk_num, buf) != 0)
    {
        perror("Writing Block Failed!\n");
        return;
    }
    if (print_flag == PRINT)
    {
        printf("注：结果写入磁盘：%d\n", output_disk_num);
    }

    // 清空当前的缓存区数据块, 便于之后继续使用它
    memset(blk, 0, buf->blkSize);
    *(blk - 1) = BLOCK_UNAVAILABLE;
}

// 元组比较函数: 若 ta 更大则返回非零值(在第一个属性相等时会比较第二个属性)
int compare_tuple(unsigned char *ta, unsigned char *tb)
{
    int ta_x = getIntFromBlock(ta);
    int ta_y = getIntFromBlock(ta + 4);
    int tb_x = getIntFromBlock(tb);
    int tb_y = getIntFromBlock(tb + 4);
    if (ta_x > tb_x)
        return 1;
    else if ((ta_x == tb_x) && (ta_y > tb_y))
        return 1;
    else
        return 0;
}

// 元组交换函数: 交换两个元组在数据块中的位置
void swap_tuple(unsigned char *ta, unsigned char *tb)
{
    int ta_x = getIntFromBlock(ta);
    int ta_y = getIntFromBlock(ta + 4);
    int tb_x = getIntFromBlock(tb);
    int tb_y = getIntFromBlock(tb + 4);
    writeTupleToBlock(tb, ta_x, ta_y);
    writeTupleToBlock(ta, tb_x, tb_y);
}

// 两阶段多路归并排序的第一阶段: 划分子集并排序子集
void tpmms_first_stage(Buffer *buf, int disk_num_start, int disk_num_end, int output_disk_num)
{
    // 常量 TPMMS_MAX_GROUP_SIZE 的值为 6 , 即单个分组最多包含 6 个数据块
    unsigned char *blk_read[TPMMS_MAX_GROUP_SIZE];

    int index_blk = disk_num_start;
    int group_num = (disk_num_end - disk_num_start + 1 + TPMMS_MAX_GROUP_SIZE - 1) / TPMMS_MAX_GROUP_SIZE; // 分组总数向上取整
    for (int group = 0; group < group_num; group++)
    {
        int last_index_blk = index_blk;
        int count = 0;
        while ((count < TPMMS_MAX_GROUP_SIZE) && (index_blk <= disk_num_end))
        {
            // 循环结束后, count 的值应当为当前分组的实际数据块数
            if ((blk_read[count] = readBlockFromDisk(index_blk, buf)) == NULL)
            {
                perror("Reading Block Failed!\n");
                return;
            }
            // printf("读入数据块%d\n", index_blk);
            index_blk++;
            count++;
        }

        printf("现在开始对数据块%d至数据块%d进行内排序\n", last_index_blk, index_blk - 1);

        int max_tuple_num = TPMMS_MAX_GROUP_SIZE * 7;
        // 对当前分组中的所有元组进行冒泡排序, 所需的外层循环数不会超过单个分组包含的最大元组数
        for (int i = 0; i < max_tuple_num; i++)
        {
            for (int j = 0; j < count; j++)
            {
                // 偏移量必须小于 56, 因为后 8 位要存放后继数据块地址
                for (int offset = 0; offset < 56; offset += 8)
                {
                    unsigned char *ta, *tb;
                    ta = blk_read[j] + offset;
                    // 偏移量为 48 时, 说明 ta 是当前数据块的最后一个元组, 因此要和下一个数据块(如果有的话)的第一个元组进行比较
                    if (offset == 48)
                    {
                        if (j == count - 1)
                        {
                            continue;
                        }
                        tb = blk_read[j + 1];
                    }
                    // 否则, ta 直接和当前数据块中的下一个元组进行比较
                    else
                    {
                        tb = ta + 8;
                    }
                    if (compare_tuple(ta, tb))
                    {
                        swap_tuple(ta, tb);
                    }
                }
            }
        }

        printf("内排序完成, 现在将" RED "第一趟排序" COLOR_NONE "的结果写入磁盘%d至磁盘%d\n", output_disk_num, output_disk_num + count - 1);

        for (int i = 0; i < count; i++)
        {
            // 补充后继地址
            writeTupleToBlock(blk_read[i] + 56, output_disk_num + i + 1, TUPLE_END_FLAG);
            if (writeBlockToDisk(blk_read[i], output_disk_num + i, buf) != 0)
            {
                perror("Writing Block Failed!\n");
                return;
            }
            // printf("注：结果写入磁盘：%d\n", output_disk_num + i);
        }
        output_disk_num += count;
    }
}

// 两阶段多路归并排序的第二阶段: 各子集间归并排序
void tpmms_second_stage(Buffer *buf, int disk_num_start, int disk_num_end, int output_disk_num)
{
    unsigned char *blk_write;
    blk_write = getNewBlockInBuffer(buf);
    memset(blk_write, 0, buf->blkSize);
    unsigned char *blk_read[TPMMS_MAX_GROUP_SIZE];

    int group_num = (disk_num_end - disk_num_start + TPMMS_MAX_GROUP_SIZE) / TPMMS_MAX_GROUP_SIZE;
    // group_disk_index[group] 保存的是当前分组比较到的磁盘块编号
    // 由于 R 和 S 分别有 16, 32 个数据块, 故最多有 6 个分组
    int group_disk_index[6];
    for (int group = 0; group < group_num; group++)
    {
        group_disk_index[group] = disk_num_start + TPMMS_MAX_GROUP_SIZE * group;
        if ((blk_read[group] = readBlockFromDisk(group_disk_index[group], buf)) == NULL)
        {
            perror("Reading Block Failed!\n");
            return;
        }
        // printf("读入数据块%d\n", group_disk_index[group]);
    }

    int blk_num_sorted = 0;         // 当前处理完的块数
    int offset_within_blk[6] = {0}; // 每个数据块比较到的位置
    int tuple_num_sorted = 0;       // blk_write中的记录数(即处理完的记录数)
    while (blk_num_sorted < disk_num_end - disk_num_start + 1)
    {
        int min_group = -1, min_offset = 0; // 最小值所在组及其对应偏移
        for (int group = 0; group < group_num; group++)
        {
            // 判断当前组是否已经为空
            if ((group_disk_index[group] > disk_num_end) || (group_disk_index[group] >= disk_num_start + TPMMS_MAX_GROUP_SIZE * (group + 1)))
            {
                continue;
            }
            if ((min_group == -1) || compare_tuple(blk_read[min_group] + min_offset, blk_read[group] + offset_within_blk[group]))
            {
                min_group = group;
                min_offset = offset_within_blk[group];
            }
        }

        // 及时将归并结果写入磁盘
        if ((tuple_num_sorted != 0) && (tuple_num_sorted % 7 == 0))
        {
            writeTupleToDisk(blk_write, buf, output_disk_num, NOT_PRINT);
            output_disk_num++;
        }

        int x = getIntFromBlock(blk_read[min_group] + min_offset);
        int y = getIntFromBlock(blk_read[min_group] + min_offset + 4);
        writeTupleToBlock(blk_write + 8 * (tuple_num_sorted % 7), x, y);
        offset_within_blk[min_group] += 8;
        tuple_num_sorted++;

        // 当 min_offset 的值为 48 时, 说明当前块的元组已经被访问完, 对应分组需要读入一个新的块
        if (min_offset == 48)
        {
            freeBlockInBuffer(blk_read[min_group], buf);
            group_disk_index[min_group]++;
            blk_num_sorted++;
            if ((group_disk_index[min_group] > disk_num_end) || (group_disk_index[min_group] >= disk_num_start + TPMMS_MAX_GROUP_SIZE * (min_group + 1)))
            {
                // 若当前组已经没有块可读则跳过
                continue;
            }
            if ((blk_read[min_group] = readBlockFromDisk(group_disk_index[min_group], buf)) == NULL)
            {
                perror("Reading Block Failed!\n");
                return;
            }
            // printf("读入数据块%d\n", group_disk_index[min_group]);
            offset_within_blk[min_group] = 0;
        }
    }

    // 将剩余已排序记录(不超过 7 个的那部分)写入磁盘
    writeTupleToBlock(blk_write + 56, output_disk_num + 1, TUPLE_END_FLAG);
    if (writeBlockToDisk(blk_write, output_disk_num, buf) != 0)
    {
        perror("Writing Block Failed!\n");
        return;
    }
    // printf("注：结果写入磁盘：%d\n", output_disk_num);
}

// 用于判断两个元组是否相等: 若相等则返回非零值
int is_equal(unsigned char *ta, unsigned char *tb)
{
    int ta_x = getIntFromBlock(ta);
    int ta_y = getIntFromBlock(ta + 4);
    int tb_x = getIntFromBlock(tb);
    int tb_y = getIntFromBlock(tb + 4);
    if ((ta_x == tb_x) && (ta_y == tb_y))
        return 1;
    return 0;
}

// 用于判断两个元组谁更小: 若 ta 更小则返回非零值(在第一个属性相等时会比较第二个属性)
int is_smaller(unsigned char *ta, unsigned char *tb)
{
    int ta_x = getIntFromBlock(ta);
    int ta_y = getIntFromBlock(ta + 4);
    int tb_x = getIntFromBlock(tb);
    int tb_y = getIntFromBlock(tb + 4);
    if (ta_x < tb_x)
        return 1;
    else if ((ta_x == tb_x) && (ta_y < tb_y))
        return 1;
    else
        return 0;
}