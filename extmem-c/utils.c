#include "utils.h"

char s[5];

// �����������ݿ��е��ַ�ת��Ϊ������
int getIntFromBlock(unsigned char *blk)
{
    // R.A, R.B, S.C, S.D ������λ��
    for (int i = 0; i < 3; i++)
    {
        s[i] = blk[i];
    }
    s[3] = '\0';
    return atoi(s);
}

// ����ϵ�е�Ԫ��д�ػ��������ݿ�
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
        // y ��ֵΪ TUPLE_END_FLAG ʱ, ˵���Ѿ�д����ǰ��, ��ȫ�����Ϊ���ַ�
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

// �����������ݿ��е�Ԫ��д���Ժ�, ����д�ص�������
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
        printf("ע�����д����̣�%d\n", output_disk_num);
    }

    // ��յ�ǰ�Ļ��������ݿ�, ����֮�����ʹ����
    memset(blk, 0, buf->blkSize);
    *(blk - 1) = BLOCK_UNAVAILABLE;
}

// Ԫ��ȽϺ���: �� ta �����򷵻ط���ֵ(�ڵ�һ���������ʱ��Ƚϵڶ�������)
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

// Ԫ�齻������: ��������Ԫ�������ݿ��е�λ��
void swap_tuple(unsigned char *ta, unsigned char *tb)
{
    int ta_x = getIntFromBlock(ta);
    int ta_y = getIntFromBlock(ta + 4);
    int tb_x = getIntFromBlock(tb);
    int tb_y = getIntFromBlock(tb + 4);
    writeTupleToBlock(tb, ta_x, ta_y);
    writeTupleToBlock(ta, tb_x, tb_y);
}

// ���׶ζ�·�鲢����ĵ�һ�׶�: �����Ӽ��������Ӽ�
void tpmms_first_stage(Buffer *buf, int disk_num_start, int disk_num_end, int output_disk_num)
{
    // ���� TPMMS_MAX_GROUP_SIZE ��ֵΪ 6 , ���������������� 6 �����ݿ�
    unsigned char *blk_read[TPMMS_MAX_GROUP_SIZE];

    int index_blk = disk_num_start;
    int group_num = (disk_num_end - disk_num_start + 1 + TPMMS_MAX_GROUP_SIZE - 1) / TPMMS_MAX_GROUP_SIZE; // ������������ȡ��
    for (int group = 0; group < group_num; group++)
    {
        int last_index_blk = index_blk;
        int count = 0;
        while ((count < TPMMS_MAX_GROUP_SIZE) && (index_blk <= disk_num_end))
        {
            // ѭ��������, count ��ֵӦ��Ϊ��ǰ�����ʵ�����ݿ���
            if ((blk_read[count] = readBlockFromDisk(index_blk, buf)) == NULL)
            {
                perror("Reading Block Failed!\n");
                return;
            }
            // printf("�������ݿ�%d\n", index_blk);
            index_blk++;
            count++;
        }

        printf("���ڿ�ʼ�����ݿ�%d�����ݿ�%d����������\n", last_index_blk, index_blk - 1);

        int max_tuple_num = TPMMS_MAX_GROUP_SIZE * 7;
        // �Ե�ǰ�����е�����Ԫ�����ð������, ��������ѭ�������ᳬ������������������Ԫ����
        for (int i = 0; i < max_tuple_num; i++)
        {
            for (int j = 0; j < count; j++)
            {
                // ƫ��������С�� 56, ��Ϊ�� 8 λҪ��ź�����ݿ��ַ
                for (int offset = 0; offset < 56; offset += 8)
                {
                    unsigned char *ta, *tb;
                    ta = blk_read[j] + offset;
                    // ƫ����Ϊ 48 ʱ, ˵�� ta �ǵ�ǰ���ݿ�����һ��Ԫ��, ���Ҫ����һ�����ݿ�(����еĻ�)�ĵ�һ��Ԫ����бȽ�
                    if (offset == 48)
                    {
                        if (j == count - 1)
                        {
                            continue;
                        }
                        tb = blk_read[j + 1];
                    }
                    // ����, ta ֱ�Ӻ͵�ǰ���ݿ��е���һ��Ԫ����бȽ�
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

        printf("���������, ���ڽ�" RED "��һ������" COLOR_NONE "�Ľ��д�����%d������%d\n", output_disk_num, output_disk_num + count - 1);

        for (int i = 0; i < count; i++)
        {
            // �����̵�ַ
            writeTupleToBlock(blk_read[i] + 56, output_disk_num + i + 1, TUPLE_END_FLAG);
            if (writeBlockToDisk(blk_read[i], output_disk_num + i, buf) != 0)
            {
                perror("Writing Block Failed!\n");
                return;
            }
            // printf("ע�����д����̣�%d\n", output_disk_num + i);
        }
        output_disk_num += count;
    }
}

// ���׶ζ�·�鲢����ĵڶ��׶�: ���Ӽ���鲢����
void tpmms_second_stage(Buffer *buf, int disk_num_start, int disk_num_end, int output_disk_num)
{
    unsigned char *blk_write;
    blk_write = getNewBlockInBuffer(buf);
    memset(blk_write, 0, buf->blkSize);
    unsigned char *blk_read[TPMMS_MAX_GROUP_SIZE];

    int group_num = (disk_num_end - disk_num_start + TPMMS_MAX_GROUP_SIZE) / TPMMS_MAX_GROUP_SIZE;
    // group_disk_index[group] ������ǵ�ǰ����Ƚϵ��Ĵ��̿���
    // ���� R �� S �ֱ��� 16, 32 �����ݿ�, ������� 6 ������
    int group_disk_index[6];
    for (int group = 0; group < group_num; group++)
    {
        group_disk_index[group] = disk_num_start + TPMMS_MAX_GROUP_SIZE * group;
        if ((blk_read[group] = readBlockFromDisk(group_disk_index[group], buf)) == NULL)
        {
            perror("Reading Block Failed!\n");
            return;
        }
        // printf("�������ݿ�%d\n", group_disk_index[group]);
    }

    int blk_num_sorted = 0;         // ��ǰ������Ŀ���
    int offset_within_blk[6] = {0}; // ÿ�����ݿ�Ƚϵ���λ��
    int tuple_num_sorted = 0;       // blk_write�еļ�¼��(��������ļ�¼��)
    while (blk_num_sorted < disk_num_end - disk_num_start + 1)
    {
        int min_group = -1, min_offset = 0; // ��Сֵ�����鼰���Ӧƫ��
        for (int group = 0; group < group_num; group++)
        {
            // �жϵ�ǰ���Ƿ��Ѿ�Ϊ��
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

        // ��ʱ���鲢���д�����
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

        // �� min_offset ��ֵΪ 48 ʱ, ˵����ǰ���Ԫ���Ѿ���������, ��Ӧ������Ҫ����һ���µĿ�
        if (min_offset == 48)
        {
            freeBlockInBuffer(blk_read[min_group], buf);
            group_disk_index[min_group]++;
            blk_num_sorted++;
            if ((group_disk_index[min_group] > disk_num_end) || (group_disk_index[min_group] >= disk_num_start + TPMMS_MAX_GROUP_SIZE * (min_group + 1)))
            {
                // ����ǰ���Ѿ�û�п�ɶ�������
                continue;
            }
            if ((blk_read[min_group] = readBlockFromDisk(group_disk_index[min_group], buf)) == NULL)
            {
                perror("Reading Block Failed!\n");
                return;
            }
            // printf("�������ݿ�%d\n", group_disk_index[min_group]);
            offset_within_blk[min_group] = 0;
        }
    }

    // ��ʣ���������¼(������ 7 �����ǲ���)д�����
    writeTupleToBlock(blk_write + 56, output_disk_num + 1, TUPLE_END_FLAG);
    if (writeBlockToDisk(blk_write, output_disk_num, buf) != 0)
    {
        perror("Writing Block Failed!\n");
        return;
    }
    // printf("ע�����д����̣�%d\n", output_disk_num);
}

// �����ж�����Ԫ���Ƿ����: ������򷵻ط���ֵ
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

// �����ж�����Ԫ��˭��С: �� ta ��С�򷵻ط���ֵ(�ڵ�һ���������ʱ��Ƚϵڶ�������)
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