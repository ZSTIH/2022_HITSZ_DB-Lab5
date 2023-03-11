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

// ����һ: �������������Ĺ�ϵѡ���㷨(ѡ�� S.C ��ֵΪ c_value ��Ԫ��)
void linear_select(Buffer *buf, int c_value)
{
    int output_disk_num = LINEAR_SELECT_OUTPUT_DISK_NUM;
    printf("\n---------------------------------------------------------------------------------\n");
    printf(RED "��������������ѡ���㷨 S.C=%d\n" COLOR_NONE, c_value);
    printf("---------------------------------------------------------------------------------\n");

    // blk_read �ǴӴ����ж�ȡ�����ݿ�, blk_write ����Ҫд����̵����ݿ�
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
        printf("�������ݿ�%d\n", index);
        for (int i = 0; i < 7; i++)
        {
            int x = getIntFromBlock(blk_read + i * 8);
            int y = getIntFromBlock(blk_read + 4 + i * 8);
            if (x == c_value)
            {
                if ((record_count != 0) && (record_count % 7 == 0))
                {
                    // record_count ��Ϊ 0 ��Ϊ 7 �ı���ʱ, ˵���Ѿ�д����һ�����������ݿ�
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

    // ��ʣ���¼(������ 7 �����ǲ���)д�����
    if (writeBlockToDisk(blk_write, output_disk_num, buf) != 0)
    {
        perror("Writing Block Failed!\n");
        return;
    }
    printf("ע�����д����̣�%d\n", output_disk_num);

    printf(RED "����ѡ��������Ԫ��һ����%d����\n" COLOR_NONE, record_count);
    printf(RED "IO��дһ����%lu�Ρ�\n" COLOR_NONE, buf->numIO);
    freeBuffer(buf);
}

// �����: ���׶ζ�·�鲢�����㷨
void tpmms(Buffer *buf)
{
    printf("\n---------------------------------------------------------------------------------\n");
    printf(RED "���׶ζ�·�鲢�����㷨\n" COLOR_NONE);
    printf("---------------------------------------------------------------------------------\n");
    if (!initBuffer(520, 64, buf))
    {
        perror("Buffer Initialization Failed!\n");
        return;
    }

    printf("\n---------------------------------------------------------------------------------\n");
    printf(RED "�Թ�ϵ R ���е�һ������\n" COLOR_NONE);
    printf("---------------------------------------------------------------------------------\n");
    tpmms_first_stage(buf, 1, 16, TPMMS_FIRST_OUTPUT_DISK_NUM);

    printf("\n---------------------------------------------------------------------------------\n");
    printf(RED "�Թ�ϵ R ���еڶ�������\n" COLOR_NONE);
    printf("---------------------------------------------------------------------------------\n");
    tpmms_second_stage(buf, TPMMS_FIRST_OUTPUT_DISK_NUM, TPMMS_FIRST_OUTPUT_DISK_NUM + 15, TPMMS_SECOND_OUTPUT_DISK_NUM);

    printf("��ϵ R ����" RED "���׶ζ�·�鲢����" COLOR_NONE "������ս���Ѿ���д�뵽���̿�%d�����̿�%d\n", TPMMS_SECOND_OUTPUT_DISK_NUM, TPMMS_SECOND_OUTPUT_DISK_NUM + 15);

    printf("\n---------------------------------------------------------------------------------\n");
    printf(RED "�Թ�ϵ S ���е�һ������\n" COLOR_NONE);
    printf("---------------------------------------------------------------------------------\n");
    tpmms_first_stage(buf, 17, 48, TPMMS_FIRST_OUTPUT_DISK_NUM + 16);

    printf("\n---------------------------------------------------------------------------------\n");
    printf(RED "�Թ�ϵ S ���еڶ�������\n" COLOR_NONE);
    printf("---------------------------------------------------------------------------------\n");
    tpmms_second_stage(buf, TPMMS_FIRST_OUTPUT_DISK_NUM + 16, TPMMS_FIRST_OUTPUT_DISK_NUM + 47, TPMMS_SECOND_OUTPUT_DISK_NUM + 16);

    printf("��ϵ S ����" RED "���׶ζ�·�鲢����" COLOR_NONE "������ս���Ѿ���д�뵽���̿�%d�����̿�%d\n" COLOR_NONE, TPMMS_SECOND_OUTPUT_DISK_NUM + 16, TPMMS_SECOND_OUTPUT_DISK_NUM + 47);

    freeBuffer(buf);
}

// ������: ���������Ĺ�ϵѡ���㷨(ѡ�� S.C ��ֵΪ c_value ��Ԫ��)
void index_select(Buffer *buf, int c_value)
{
    printf("\n---------------------------------------------------------------------------------\n");
    printf(RED "����������ѡ���㷨 S.C=%d\n" COLOR_NONE, c_value);
    printf("---------------------------------------------------------------------------------\n");
    if (!initBuffer(520, 64, buf))
    {
        perror("Buffer Initialization Failed!\n");
        return;
    }

    ///////////////////////////////// ������ //////////////////////////////////////

    int output_disk_num = INDEX_OUTPUT_DISK_NUM;
    unsigned char *blk_read, *blk_write;
    blk_write = getNewBlockInBuffer(buf);
    memset(blk_write, 0, buf->blkSize);
    int tuple_count = 0;                           // �Ѿ�������������¼��Ŀ
    int blkno = TPMMS_SECOND_OUTPUT_DISK_NUM + 16; // ֱ��ʹ��������Ѿ�����õ�����
    int index_value = -1;                          // Ϊ��һ�� S.C ��ֵΪ index_value �ļ�¼��������
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
                // ������ֵ, ��Ҫ��������
                if ((tuple_count != 0) && (tuple_count % 7 == 0))
                {
                    writeTupleToDisk(blk_write, buf, output_disk_num, NOT_PRINT);
                    output_disk_num++;
                }
                // ����ǰ��¼�Ŀ�ű�������, ����֮������������м���
                writeTupleToBlock(blk_write + 8 * (tuple_count % 7), x, blkno);
                tuple_count++;
                index_value = x;
            }
        }
        blkno++;
        freeBlockInBuffer(blk_read, buf);
    }

    // ��ʣ���¼(������ 7 �����ǲ���)д�����
    if (writeBlockToDisk(blk_write, output_disk_num, buf) != 0)
    {
        perror("Writing Block Failed!\n");
        return;
    }

    ///////////////////////////////// �����������й�ϵѡ�� ///////////////////////////////

    freeBuffer(buf);
    if (!initBuffer(520, 64, buf))
    {
        perror("Buffer Initialization Failed!\n");
        return;
    }
    // ��¼��һ�� output_disk_num ��ֵ, �Ӷ�ȷ����������������ݿ���
    int last_output_disk_num = output_disk_num;
    output_disk_num = INDEX_SELECT_OUTPUT_DISK_NUM;
    blk_write = getNewBlockInBuffer(buf);
    memset(blk_write, 0, buf->blkSize);
    tuple_count = 0;
    blkno = INDEX_OUTPUT_DISK_NUM;
    int blkno_find = 0;
    // ���ҵ���ǰ S.C ֵ�µ�����
    for (;;)
    {
        if ((blk_read = readBlockFromDisk(blkno, buf)) == NULL)
        {
            perror("Reading Block Failed!\n");
            return;
        }
        printf("����������%d\n", blkno);
        for (int i = 0; i < 7; i++)
        {
            int x = getIntFromBlock(blk_read + i * 8);
            int y = getIntFromBlock(blk_read + i * 8 + 4);
            if (x > c_value) // ���ڰ���������, ��ʱ����ǰ��ֹѭ��
                break;
            if (x == c_value) // ��ʱ���ҵ���Ҫ������, ͬ������ǰ��ֹ
            {
                blkno_find = y; // y ֵ���ǵ�һ�� S.C ֵΪ c_value ��Ԫ�����ڵĿ��
                break;
            }
        }
        blkno++;
        freeBlockInBuffer(blk_read, buf);
        if (blkno_find || (blkno > last_output_disk_num)) // ���ҵ����Ѿ�����ȫ��������, ֱ���˳�
            break;
        printf("û������������Ԫ�顣\n"); // ��ǰ������û���ҵ�����ļ�¼
    }
    if (blkno_find == 0)
    {
        printf("�Ѳ�����ȫ�������顣û������������Ԫ�顣\n");
        return;
    }
    // �ٸ��ݸ������ҵ�����Ҫ������ݿ�
    blkno = blkno_find;
    int no_more_flag = 0;
    for (;;)
    {
        if ((blk_read = readBlockFromDisk(blkno, buf)) == NULL)
        {
            perror("Reading Block Failed!\n");
            return;
        }
        printf("�������ݿ�%d\n", blkno);
        for (int i = 0; i < 7; i++)
        {
            int x = getIntFromBlock(blk_read + i * 8);
            int y = getIntFromBlock(blk_read + i * 8 + 4);
            if (x == c_value)
            {
                if ((tuple_count != 0) && (tuple_count % 7 == 0))
                {
                    // tuple_count ��Ϊ 0 ��Ϊ 7 �ı���ʱ, ˵���Ѿ�д����һ�����������ݿ�
                    writeTupleToDisk(blk_write, buf, output_disk_num, PRINT);
                    output_disk_num++;
                }
                writeTupleToBlock(blk_write + 8 * (tuple_count % 7), x, y);
                printf("(X=%d, Y=%d)\n", x, y);
                tuple_count++;
            }
            if (x > c_value)
            {
                // ���ڰ���������, �� x ���� c_value ʱ, ֮���Ѿ����������ҵ�
                no_more_flag = 1;
                break;
            }
        }
        if (no_more_flag)
            break;
        blkno++;
        freeBlockInBuffer(blk_read, buf);
    }

    // ��ʣ���¼(������ 7 �����ǲ���)д�����
    if (writeBlockToDisk(blk_write, output_disk_num, buf) != 0)
    {
        perror("Writing Block Failed!\n");
        return;
    }
    printf("ע�����д����̣�%d\n", output_disk_num);

    printf(RED "����ѡ��������Ԫ��һ����%d����\n" COLOR_NONE, tuple_count);
    printf(RED "IO��дһ����%lu�Ρ�\n" COLOR_NONE, buf->numIO);
    freeBuffer(buf);
}

// ������: ������������Ӳ����㷨
void sort_merge_join(Buffer *buf)
{
    printf("\n---------------------------------------------------------------------------------\n");
    printf(RED "��������������㷨\n" COLOR_NONE);
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
    int blkno_r = TPMMS_SECOND_OUTPUT_DISK_NUM;      // ��ϵ R ��ǰ���ʵ������ݿ�
    int blkno_s = TPMMS_SECOND_OUTPUT_DISK_NUM + 16; // ��ϵ S ��ǰ���ʵ������ݿ�
    int blkno_r_upper = blkno_r + 16;                // ��ϵ R ���ݿ�ŵ��Ͻ�
    int blkno_s_upper = blkno_s + 32;                // ��ϵ S ���ݿ�ŵ��Ͻ�
    int offset_r = 0;                                // ��ϵ R �����ݿ���ƫ��
    int offset_s = 0;                                // ��ϵ S �����ݿ���ƫ��
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
            // һ�γɹ���������Ҫд�� S �� R �е�����Ԫ��
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

            // ��ϵ R ����, ��ϵ S ������������
            if (offset_s < 48)
            {
                offset_s += 8;
            }
            else
            {
                // ��ʱ�Ѿ�������һ�����ݿ�� 7 ��Ԫ��, ��Ҫ������һ�����ݿ�
                blkno_s++;
                freeBlockInBuffer(blk_S, buf);
                if (blkno_s >= blkno_s_upper) // ��ϵ S �Ѿ�����������
                    break;
                if ((blk_S = readBlockFromDisk(blkno_s, buf)) == NULL)
                {
                    perror("Reading Block Failed!\n");
                    return;
                }
                offset_s = 0;
            }
        }
        // ���˹�ϵ S
        // ԭ��ֱ��ʹ�� "strcpy(blk_S, blk_backup);" , ���� blk_backup ָ����ָ������ݿ��п��ܱ��ͷ���
        blkno_s = blkno_backup;
        freeBlockInBuffer(blk_S, buf);
        if ((blk_S = readBlockFromDisk(blkno_s, buf)) == NULL)
        {
            perror("Reading Block Failed!\n");
            return;
        }
        offset_s = offset_backup;

        // ������ R �� S ���и�С��һ�����·���
        if (getIntFromBlock(blk_R + offset_r) <= getIntFromBlock(blk_S + offset_s))
        {
            // R ������������
            if (offset_r < 48)
            {
                offset_r += 8;
            }
            else
            {
                // ��ʱ�Ѿ�������һ�����ݿ�� 7 ��Ԫ��, ��Ҫ������һ�����ݿ�
                blkno_r++;
                freeBlockInBuffer(blk_R, buf);
                if (blkno_r >= blkno_r_upper) // ��ϵ R �Ѿ�����������
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
            // S ������������
            if (offset_s < 48)
            {
                offset_s += 8;
            }
            else
            {
                // ��ʱ�Ѿ�������һ�����ݿ�� 7 ��Ԫ��, ��Ҫ������һ�����ݿ�
                blkno_s++;
                freeBlockInBuffer(blk_S, buf);
                if (blkno_s >= blkno_s_upper) // ��ϵ S �Ѿ�����������
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
    // ��ʣ���¼(������ 7 �����ǲ���)д�����
    if (writeBlockToDisk(blk_write, output_disk_num, buf) != 0)
    {
        perror("Writing Block Failed!\n");
        return;
    }
    printf("ע�����д����̣�%d\n", output_disk_num);

    printf("ȫ�����ӽ����д���˴���%d������%d��\n", SORT_MERGE_JOIN_OUTPUT_DISK_NUM, output_disk_num);
    printf(RED "�ܹ�������%d�Ρ�\n" COLOR_NONE, tuple_count / 2);
    freeBuffer(buf);
}

// ������: ��������ļ��ϵĽ�����
void intersect(Buffer *buf)
{
    printf("\n---------------------------------------------------------------------------------\n");
    printf(RED "��������ļ��ϵĽ�����\n" COLOR_NONE);
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
    int blkno_r = TPMMS_SECOND_OUTPUT_DISK_NUM;      // ��ϵ R ��ǰ���ʵ������ݿ�
    int blkno_s = TPMMS_SECOND_OUTPUT_DISK_NUM + 16; // ��ϵ S ��ǰ���ʵ������ݿ�
    int blkno_r_upper = blkno_r + 16;                // ��ϵ R ���ݿ�ŵ��Ͻ�
    int blkno_s_upper = blkno_s + 32;                // ��ϵ S ���ݿ�ŵ��Ͻ�
    int offset_r = 0;                                // ��ϵ R �����ݿ���ƫ��
    int offset_s = 0;                                // ��ϵ S �����ݿ���ƫ��
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
            // �� R �� S ��ָ��ļ�¼��ȫ��ͬʱ, ����ͬʱ������һ����¼
            if ((tuple_count != 0) && (tuple_count % 7 == 0))
            {
                writeTupleToDisk(blk_write, buf, output_disk_num, PRINT);
                output_disk_num++;
            }
            writeTupleToBlock(blk_write + 8 * (tuple_count % 7), xr, yr);
            tuple_count++;
            printf("(X=%d, Y=%d)\n", xr, yr);
            if (offset_r < 48) // �ƶ� R
            {
                offset_r += 8;
            }
            else
            {
                // ��ʱ�Ѿ�������һ�����ݿ�� 7 ��Ԫ��, ��Ҫ������һ�����ݿ�
                blkno_r++;
                freeBlockInBuffer(blk_R, buf);
                if (blkno_r >= blkno_r_upper) // ��ϵ R �Ѿ�����������
                    break;
                if ((blk_R = readBlockFromDisk(blkno_r, buf)) == NULL)
                {
                    perror("Reading Block Failed!\n");
                    return;
                }
                offset_r = 0;
            }
            if (offset_s < 48) // �ƶ� S
            {
                offset_s += 8;
            }
            else
            {
                // ��ʱ�Ѿ�������һ�����ݿ�� 7 ��Ԫ��, ��Ҫ������һ�����ݿ�
                blkno_s++;
                freeBlockInBuffer(blk_S, buf);
                if (blkno_s >= blkno_s_upper) // ��ϵ S �Ѿ�����������
                    break;
                if ((blk_S = readBlockFromDisk(blkno_s, buf)) == NULL)
                {
                    perror("Reading Block Failed!\n");
                    return;
                }
                offset_s = 0;
            }
        }
        // �� R �� S ��ָ��ļ�¼�����ʱ, ֵ��С��һ��������
        else if ((xr < xs) || ((xr == xs) && (yr < ys)))
        {
            if (offset_r < 48) // �ƶ� R
            {
                offset_r += 8;
            }
            else
            {
                // ��ʱ�Ѿ�������һ�����ݿ�� 7 ��Ԫ��, ��Ҫ������һ�����ݿ�
                blkno_r++;
                freeBlockInBuffer(blk_R, buf);
                if (blkno_r >= blkno_r_upper) // ��ϵ R �Ѿ�����������
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
            if (offset_s < 48) // �ƶ� S
            {
                offset_s += 8;
            }
            else
            {
                // ��ʱ�Ѿ�������һ�����ݿ�� 7 ��Ԫ��, ��Ҫ������һ�����ݿ�
                blkno_s++;
                freeBlockInBuffer(blk_S, buf);
                if (blkno_s >= blkno_s_upper) // ��ϵ S �Ѿ�����������
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
    // ��ʣ���¼(������ 7 �����ǲ���)д�����
    if (writeBlockToDisk(blk_write, output_disk_num, buf) != 0)
    {
        perror("Writing Block Failed!\n");
        return;
    }
    printf("ע�����д����̣�%d\n", output_disk_num);

    printf(RED "R��S�Ľ�����%d��Ԫ�顣\n" COLOR_NONE, tuple_count);
    freeBuffer(buf);
}

// ������: ��������ļ��ϵĲ�����
void union_set(Buffer *buf)
{
    printf("\n---------------------------------------------------------------------------------\n");
    printf(RED "��������ļ��ϵĲ�����\n" COLOR_NONE);
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
    int blkno_r = TPMMS_SECOND_OUTPUT_DISK_NUM;      // ��ϵ R ��ǰ���ʵ������ݿ�
    int blkno_s = TPMMS_SECOND_OUTPUT_DISK_NUM + 16; // ��ϵ S ��ǰ���ʵ������ݿ�
    int blkno_r_upper = blkno_r + 16;                // ��ϵ R ���ݿ�ŵ��Ͻ�
    int blkno_s_upper = blkno_s + 32;                // ��ϵ S ���ݿ�ŵ��Ͻ�
    int offset_r = 0;                                // ��ϵ R �����ݿ���ƫ��
    int offset_s = 0;                                // ��ϵ S �����ݿ���ƫ��
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
            // �� R �� S ��ָ��ļ�¼��ȫ��ͬʱ, ֻ��Ҫ�����д��һ�ν��
            if ((tuple_count != 0) && (tuple_count % 7 == 0))
            {
                writeTupleToDisk(blk_write, buf, output_disk_num, PRINT);
                output_disk_num++;
            }
            writeTupleToBlock(blk_write + 8 * (tuple_count % 7), getIntFromBlock(blk_R + offset_r), getIntFromBlock(blk_R + offset_r + 4));
            tuple_count++;
            // Ȼ��, �� R �� S ������һ����¼
            if (offset_s < 48) // �ƶ� S
            {
                offset_s += 8;
            }
            else
            {
                // ��ʱ�Ѿ�������һ�����ݿ�� 7 ��Ԫ��, ��Ҫ������һ�����ݿ�
                blkno_s++;
                freeBlockInBuffer(blk_S, buf);
                if (blkno_s >= blkno_s_upper) // ��ϵ S �Ѿ�����������
                    continue;                 // �˴������� break , ��Ҫ��continue. ��Ϊ S �������˻������� R Ҫ����, break����������ѭ��.
                if ((blk_S = readBlockFromDisk(blkno_s, buf)) == NULL)
                {
                    perror("Reading Block Failed!\n");
                    return;
                }
                offset_s = 0;
            }
            if (offset_r < 48) // �ƶ� R
            {
                offset_r += 8;
            }
            else
            {
                // ��ʱ�Ѿ�������һ�����ݿ�� 7 ��Ԫ��, ��Ҫ������һ�����ݿ�
                blkno_r++;
                freeBlockInBuffer(blk_R, buf);
                if (blkno_r >= blkno_r_upper) // ��ϵ R �Ѿ�����������
                    continue;                 // �˴������� break , ��Ҫ��continue. ��Ϊ R �������˻������� S Ҫ����, break����������ѭ��.
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
            // ��ϵ S Ϊ�ջ��ϵ R ��ֵС�ڹ�ϵ S ʱ, �����ù�ϵ R �����������
            if ((tuple_count != 0) && (tuple_count % 7 == 0))
            {
                writeTupleToDisk(blk_write, buf, output_disk_num, PRINT);
                output_disk_num++;
            }
            writeTupleToBlock(blk_write + 8 * (tuple_count % 7), getIntFromBlock(blk_R + offset_r), getIntFromBlock(blk_R + offset_r + 4));
            tuple_count++;
            if (offset_r < 48) // �ƶ� R
            {
                offset_r += 8;
            }
            else
            {
                // ��ʱ�Ѿ�������һ�����ݿ�� 7 ��Ԫ��, ��Ҫ������һ�����ݿ�
                blkno_r++;
                freeBlockInBuffer(blk_R, buf);
                if (blkno_r >= blkno_r_upper) // ��ϵ R �Ѿ�����������
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
            // ���������, ���ǹ�ϵ S �����������
            if ((tuple_count != 0) && (tuple_count % 7 == 0))
            {
                writeTupleToDisk(blk_write, buf, output_disk_num, PRINT);
                output_disk_num++;
            }
            writeTupleToBlock(blk_write + 8 * (tuple_count % 7), getIntFromBlock(blk_S + offset_s), getIntFromBlock(blk_S + offset_s + 4));
            tuple_count++;
            if (offset_s < 48) // �ƶ� S
            {
                offset_s += 8;
            }
            else
            {
                // ��ʱ�Ѿ�������һ�����ݿ�� 7 ��Ԫ��, ��Ҫ������һ�����ݿ�
                blkno_s++;
                freeBlockInBuffer(blk_S, buf);
                if (blkno_s >= blkno_s_upper) // ��ϵ S �Ѿ�����������
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

    // ��ʣ���¼(������ 7 �����ǲ���)д�����
    if (writeBlockToDisk(blk_write, output_disk_num, buf) != 0)
    {
        perror("Writing Block Failed!\n");
        return;
    }
    printf("ע�����д����̣�%d\n", output_disk_num);

    printf(RED "R��S�Ĳ�����%d��Ԫ�顣\n" COLOR_NONE, tuple_count);
    freeBuffer(buf);
}

// ������: ��������ļ��ϵĲ�����
void difference(Buffer *buf)
{
    printf("\n---------------------------------------------------------------------------------\n");
    printf(RED "��������ļ��ϵĲ�����\n" COLOR_NONE);
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
    int blkno_r = TPMMS_SECOND_OUTPUT_DISK_NUM;      // ��ϵ R ��ǰ���ʵ������ݿ�
    int blkno_s = TPMMS_SECOND_OUTPUT_DISK_NUM + 16; // ��ϵ S ��ǰ���ʵ������ݿ�
    int blkno_r_upper = blkno_r + 16;                // ��ϵ R ���ݿ�ŵ��Ͻ�
    int blkno_s_upper = blkno_s + 32;                // ��ϵ S ���ݿ�ŵ��Ͻ�
    int offset_r = 0;                                // ��ϵ R �����ݿ���ƫ��
    int offset_s = 0;                                // ��ϵ S �����ݿ���ƫ��
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
        // ���ڼ������ S - R , �������ʱ��������κ�Ԫ��, S �� R ��Ҫ����
        if ((blkno_r < blkno_r_upper) && (blkno_s < blkno_s_upper) && (is_equal(blk_R + offset_r, blk_S + offset_s)))
        {
            if (offset_s < 48) // �ƶ� S
            {
                offset_s += 8;
            }
            else
            {
                // ��ʱ�Ѿ�������һ�����ݿ�� 7 ��Ԫ��, ��Ҫ������һ�����ݿ�
                blkno_s++;
                freeBlockInBuffer(blk_S, buf);
                if (blkno_s >= blkno_s_upper) // ��ϵ S �Ѿ�����������
                    continue;
                if ((blk_S = readBlockFromDisk(blkno_s, buf)) == NULL)
                {
                    perror("Reading Block Failed!\n");
                    return;
                }
                offset_s = 0;
            }
            if (offset_r < 48) // �ƶ� R
            {
                offset_r += 8;
            }
            else
            {
                // ��ʱ�Ѿ�������һ�����ݿ�� 7 ��Ԫ��, ��Ҫ������һ�����ݿ�
                blkno_r++;
                freeBlockInBuffer(blk_R, buf);
                if (blkno_r >= blkno_r_upper) // ��ϵ R �Ѿ�����������
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
            // ���� R С�� S �����, ��Ҫ�� R ����������, �Ҵ�ʱ��������κ�Ԫ��
            if (offset_r < 48) // �ƶ� R
            {
                offset_r += 8;
            }
            else
            {
                // ��ʱ�Ѿ�������һ�����ݿ�� 7 ��Ԫ��, ��Ҫ������һ�����ݿ�
                blkno_r++;
                freeBlockInBuffer(blk_R, buf);
                if (blkno_r >= blkno_r_upper) // ��ϵ R �Ѿ�����������
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
            // ���������, ��Ҫ�� S ����������, �Ҵ�ʱҪ��� S �е�Ԫ��
            if ((tuple_count != 0) && (tuple_count % 7 == 0))
            {
                writeTupleToDisk(blk_write, buf, output_disk_num, PRINT);
                output_disk_num++;
            }
            writeTupleToBlock(blk_write + 8 * (tuple_count % 7), getIntFromBlock(blk_S + offset_s), getIntFromBlock(blk_S + offset_s + 4));
            tuple_count++;
            if (offset_s < 48) // �ƶ� S
            {
                offset_s += 8;
            }
            else
            {
                // ��ʱ�Ѿ�������һ�����ݿ�� 7 ��Ԫ��, ��Ҫ������һ�����ݿ�
                blkno_s++;
                freeBlockInBuffer(blk_S, buf);
                if (blkno_s >= blkno_s_upper) // ��ϵ S �Ѿ�����������
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

    // ��ʣ���¼(������ 7 �����ǲ���)д�����
    if (writeBlockToDisk(blk_write, output_disk_num, buf) != 0)
    {
        perror("Writing Block Failed!\n");
        return;
    }
    printf("ע�����д����̣�%d\n", output_disk_num);

    printf(RED "S��R�Ĳ(S-R)��%d��Ԫ�顣\n" COLOR_NONE, tuple_count);
    freeBuffer(buf);
}