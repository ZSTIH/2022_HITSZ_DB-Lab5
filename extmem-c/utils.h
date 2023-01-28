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

// 任务一基于线性搜索的选择结果从块号为 100 的磁盘块开始存储
#define LINEAR_SELECT_OUTPUT_DISK_NUM                       100
// 任务二第一趟排序的结果从块号为 201 的磁盘块开始存储
#define TPMMS_FIRST_OUTPUT_DISK_NUM                         201
// 任务二第二趟排序的结果从块号为 301 的磁盘块开始存储
#define TPMMS_SECOND_OUTPUT_DISK_NUM                        301
// 任务三建立的索引从块号为 350 的磁盘块开始存储
#define INDEX_OUTPUT_DISK_NUM                               350
// 任务三基于索引的选择结果从块号为 400 的磁盘块开始存储
#define INDEX_SELECT_OUTPUT_DISK_NUM                        400
// 任务四基于排序的连接操作结果从块号为 500 的磁盘块开始存储
#define SORT_MERGE_JOIN_OUTPUT_DISK_NUM                     500
// 任务五基于排序的集合的交运算结果从块号为 700 的磁盘块开始存储
#define SET_INTERSECT_OUTPUT_DISK_NUM                       700
// 附加题基于排序的集合的并运算结果从块号为 800 的磁盘块开始存储
#define SET_UNION_OUTPUT_DISK_NUM                           800
// 附加题基于排序的集合的差运算结果从块号为 900 的磁盘块开始存储
#define SET_DIFFERENCE_OUTPUT_DISK_NUM                      900

int getIntFromBlock(unsigned char *blk);
void writeTupleToBlock(unsigned char *blk, int x, int y);
void writeTupleToDisk(unsigned char *blk, Buffer *buf, int output_disk_num, int print_flag);
void tpmms_first_stage(Buffer *buf, int disk_num_start, int disk_num_end, int output_disk_num);
void tpmms_second_stage(Buffer *buf, int disk_num_start, int disk_num_end, int output_disk_num);
int is_equal(unsigned char *ta, unsigned char *tb);
int is_smaller(unsigned char *ta, unsigned char *tb);