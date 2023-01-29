#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "extmem.h"

#define COLOR_NONE                                          "\033[m"
#define RED                                                 "\033[0;32;31m"

#define TUPLE_END_FLAG                                      0
#define PRINT                                               1
#define NOT_PRINT                                           0
#define TPMMS_MAX_GROUP_SIZE                                6

// ����һ��������������ѡ�����ӿ��Ϊ 100 �Ĵ��̿鿪ʼ�洢
#define LINEAR_SELECT_OUTPUT_DISK_NUM                       100
// �������һ������Ľ���ӿ��Ϊ 201 �Ĵ��̿鿪ʼ�洢
#define TPMMS_FIRST_OUTPUT_DISK_NUM                         201
// ������ڶ�������Ľ���ӿ��Ϊ 301 �Ĵ��̿鿪ʼ�洢
#define TPMMS_SECOND_OUTPUT_DISK_NUM                        301
// �����������������ӿ��Ϊ 350 �Ĵ��̿鿪ʼ�洢
#define INDEX_OUTPUT_DISK_NUM                               350
// ����������������ѡ�����ӿ��Ϊ 400 �Ĵ��̿鿪ʼ�洢
#define INDEX_SELECT_OUTPUT_DISK_NUM                        400
// �����Ļ�����������Ӳ�������ӿ��Ϊ 500 �Ĵ��̿鿪ʼ�洢
#define SORT_MERGE_JOIN_OUTPUT_DISK_NUM                     500
// �������������ļ��ϵĽ��������ӿ��Ϊ 700 �Ĵ��̿鿪ʼ�洢
#define SET_INTERSECT_OUTPUT_DISK_NUM                       700
// �������������ļ��ϵĲ��������ӿ��Ϊ 800 �Ĵ��̿鿪ʼ�洢
#define SET_UNION_OUTPUT_DISK_NUM                           800
// �������������ļ��ϵĲ��������ӿ��Ϊ 900 �Ĵ��̿鿪ʼ�洢
#define SET_DIFFERENCE_OUTPUT_DISK_NUM                      900

int getIntFromBlock(unsigned char *blk);
void writeTupleToBlock(unsigned char *blk, int x, int y);
void writeTupleToDisk(unsigned char *blk, Buffer *buf, int output_disk_num, int print_flag);
void tpmms_first_stage(Buffer *buf, int disk_num_start, int disk_num_end, int output_disk_num);
void tpmms_second_stage(Buffer *buf, int disk_num_start, int disk_num_end, int output_disk_num);
int is_equal(unsigned char *ta, unsigned char *tb);
int is_smaller(unsigned char *ta, unsigned char *tb);