#include <cstdio>
#include <cassert>
#include <cstdlib>

#define MAX_DISK_NUM (10 + 1)  //硬盘最多10个，10+1是为了让你计数方便不从0开始
#define MAX_DISK_SIZE (16384 + 1)  //每个硬盘的存储单元数，+1是为了让你计数方便不从0开始 
#define MAX_REQUEST_NUM (30000000 + 1)  //读取次数小于等于30000000次，也是从1开始计数
#define MAX_OBJECT_NUM (100000 + 1)  //最多100000种对象，对象的id从1-100000
#define MAX_TAG_NUM (17)  // 最多16种标签
#define MAX_SLOT_NUM (49)  // 最多86400/1800个长时隙
#define REP_NUM (3)  //副本的个数
#define FRE_PER_SLICING (1800)  //一个长时隙占1800个时间片
#define EXTRA_TIME (105)  //判题器额外给出的105个时隙供处理

typedef struct Request_ {
    int object_id;  // 对象id
    int prev_id;  // 该对象的前一个读取请求的req_id
    int* unread_block;  // 该请求中，未被读取的块的索引（1-size）,对应索引位置为0就代表没有读
    bool is_done;
    bool is_abort;  // 该请求是否被取消
} Request;

typedef struct Object_ {
    int replica[REP_NUM + 1];  // 副本
    int* unit[REP_NUM + 1];
    int size;
    int tag;
    // int main_disk_num;
    int last_request_point;
    bool is_delete;
} Object;

typedef struct Disk_Head_ {
    int pos;  // 磁头位置
    int token;  // 当前时间片的可消耗令牌数
    int last_status;  // 磁头的上个动作消耗的令牌数 -1：上个动作是跳，1：上个动作是pass，64-16，上个动作是读
}Disk_Head;

Request request[MAX_REQUEST_NUM];  // 用一个数组保存所有请求
Object object[MAX_OBJECT_NUM];  // 对象的id就是用object数组的index表示
Disk_Head disk_head[MAX_DISK_NUM];  //磁头数组

int T, M, N, V, G;
int disk[MAX_DISK_NUM][MAX_DISK_SIZE];  //每块硬盘用二维数组表示
int fre_del[MAX_TAG_NUM][MAX_SLOT_NUM];  // 第i行第j个元素表示在j时隙内，所有删除操作中对象标签为i的对象大小之和
int fre_write[MAX_TAG_NUM][MAX_SLOT_NUM];  // 第i行第j个元素表示在j时隙内，所有写入操作中对象标签为i的对象大小之和
int fre_read[MAX_TAG_NUM][MAX_SLOT_NUM];  // 第i行第j个元素表示在j时隙内，所有读取操作中对象标签为i的对象大小之和，同一个对象的多次读取会重复计算
int tag_block_address[MAX_TAG_NUM];

void timestamp_action()  // 时间片对齐事件
{
    int timestamp;
    scanf("%*s%d", &timestamp);
    printf("TIMESTAMP %d\n", timestamp);

    fflush(stdout);
}

void do_object_delete(const int* object_unit, int* disk_unit, int size)
{
    for (int i = 1; i <= size; i++) {
        disk_unit[object_unit[i]] = 0;
    }
}

void delete_action()  // 对象删除事件
{
    int n_delete;  // 这一时间片被删除的对象个数
    int abort_num = 0;  // 被取消的读取请求的数量
    static int _id[MAX_OBJECT_NUM];  // 删除对象的对象编号

    scanf("%d", &n_delete);
    static int req_id_print[MAX_OBJECT_NUM];
    for (int i = 1; i <= n_delete; i++) {
        scanf("%d", &_id[i]);
    }

    // 两个循环可以合并：空间换时间，用数组保存被删除的对象id
    // 输出n_abort
    for (int i = 1; i <= n_delete; i++) {
        int id = _id[i];
        int current_id = object[id].last_request_point;  // 编号为id的对象最后一次被请求的req_id
        while (current_id != 0) {  // 是否需要一直回溯？？？此处可以优化
            if (request[current_id].is_done == false) {  // 取消该对象当前所有还没完成的读取请求
                abort_num++;
                *(req_id_print + abort_num) = current_id;
                request[current_id].is_abort = true;
            }
            current_id = request[current_id].prev_id;  // 该对象在编号为current_id的请求的前一次请求id
        }
        for (int j = 1; j <= REP_NUM; j++) {
            do_object_delete(object[id].unit[j], disk[object[id].replica[j]], object[id].size);
        }
        object[id].is_delete = true;
    }

    printf("%d\n", abort_num);
    for (int i = 1; i <= abort_num; i++) {  // 输出删除的请求编号req_id
        printf("%d\n", req_id_print[i]);
    }

    // for (int i = 1; i <= n_delete; i++) {  // 输出删除的请求编号req_id
    //     int id = _id[i];
    //     int current_id = object[id].last_request_point;
    //     while (current_id != 0) {
    //         if (request[current_id].is_done == false) {
    //             printf("%d\n", current_id);
    //         }
    //         current_id = request[current_id].prev_id;
    //     }
    //     for (int j = 1; j <= REP_NUM; j++) {
    //         do_object_delete(object[id].unit[j], disk[object[id].replica[j]], object[id].size);
    //     }
    //     object[id].is_delete = true;
    // }

    fflush(stdout);
}

void do_object_write(int* object_unit, int* disk_unit, int size, int object_id)
{
    int current_write_point = 0;
    int start_address = tag_block_address[object[object_id].tag];  // 每个TAG分区在各个盘中的首地址（相同）
    for (int i = start_address; i < start_address + V / M; i++) {
        if (disk_unit[i] == 0) {  // 如果该存储单元是空的则存，不是空的去下一个单元，所以不是连续存储
            disk_unit[i] = object_id;  // disk存的是对象id
            object_unit[++current_write_point] = i;  // object_unit即unit[j]是数组名（数组首地址），存的是存储单元编号
            if (current_write_point == size) {
                break;  // 存完了这个对象
            }
        }
    }

    // 如果遍历完当前TAG分区，该对象还没存完，就找到空闲存储空间存下。注意，实际上，每个disk都有若干空闲的TAG分区，如有必要，后续可以you'hua。
    if (current_write_point < size) {
        for (int i = 1; i <= V; i++) {
            if (disk_unit[i] == 0) {  // 如果该存储单元是空的则存，不是空的去下一个单元，所以不是连续存储
                disk_unit[i] = object_id;  // disk存的是对象id
                object_unit[++current_write_point] = i;  // object_unit即unit[j]是数组名（数组首地址），存的是存储单元编号
                if (current_write_point == size) {
                    break;  // 存完了这个对象
                }
            }
        }
    }
    assert(current_write_point == size);
}

void write_action()  // 对象写入事件
{
    int n_write;  // 这一时间片写入对象的个数
    scanf("%d", &n_write);
    for (int i = 1; i <= n_write; i++) {
        int id, size, tag_id;
        scanf("%d%d%d", &id, &size, &tag_id);
        object[id].last_request_point = 0;
        object[id].size = size;
        object[id].tag = tag_id;
        // object[id].main_disk_num = (tag_id - 1) % N + 1;
        object[id].is_delete = false;
        for (int j = 1; j <= REP_NUM; j++) {
            object[id].replica[j] = (id + j) % N + 1;  // 得到主盘与两个副本盘编号
            // object[id].replica[j] = (object[id].main_disk_num + j - 2) % N + 1;
            object[id].unit[j] = static_cast<int*>(malloc(sizeof(int) * (size + 1)));  // unit中的元素对应相应副本的首地址
            do_object_write(object[id].unit[j], disk[object[id].replica[j]], size, id);
        }

        printf("%d\n", id);  // 打印对象id
        for (int j = 1; j <= REP_NUM; j++) {
            printf("%d", object[id].replica[j]);  // 打印第j个副本写入的硬盘编号
            for (int k = 1; k <= size; k++) {
                printf(" %d", object[id].unit[j][k]);  // 打印第j个副本的第k个块的存储单元编号
            }
            printf("\n");
        }
    }

    fflush(stdout);
}

void read_action()  // 对象读取事件
{
    static int req_count=0;
    int n_read;  // 这一时间片读取对象的个数
    int request_id, object_id;
    scanf("%d", &n_read);
    for (int i = 1; i <= n_read; i++) {
        scanf("%d%d", &request_id, &object_id);
        request[request_id].object_id = object_id;
        request[request_id].prev_id = object[object_id].last_request_point;
        object[object_id].last_request_point = request_id;
        request[request_id].is_done = false;
        request[request_id].is_abort = false;
        request[request_id].unread_block = static_cast<int*>(calloc(object[object_id].size + 1, sizeof(int)));  //记得在clean里面free
    }
    req_count+=n_read;

    static int current_request = 0;
    // static int current_phase = 0;
    if (!current_request && n_read > 0) {
        current_request = 1;  // 本时间片的最后一个请求req_id[n_read] obj_id[n_read]
    }
    while(request[current_request].is_abort){current_request++;}
    if(!request[current_request].is_abort&&current_request<=req_count)
    {
        object_id = request[current_request].object_id;
        for (int i = 1; i <= N; i++) {
            if (i == object[object_id].replica[1]) {  // 当前逻辑是只找第一个副本读
                if(disk_head[i].pos==object[object_id].unit[i][request[current_request].unread_block])  // 如果磁头刚好在对应位置，就直接读，然后unread_block++,磁头位置往后一个
                {
                    printf("r#\n");  // 实际上要判断上个动作的令牌消耗
                    request[current_request].unread_block++;
                    disk_head[i].pos=disk_head[i].pos%V+1;
                }
                else if((disk_head[i].pos<=object[object_id].unit[i][request[current_request].unread_block]-G+64)
                    ||(V-disk_head[i].pos+object[object_id].unit[i][request[current_request].unread_block]-1<=G-64))  
                    // 如果磁头可以通过pass的方式到对应位置而且令牌足够读取这个块，就直接读了，磁头位置和unread_block都改变
                {
                    for(int j=1;j<=object[object_id].unit[i][request[current_request].unread_block]-disk_head[i].pos;j++)
                    {
                        printf("p");
                    }
                    printf("r#\n");
                    request[current_request].unread_block++;
                    disk_head[i].pos=(disk_head[i].pos+object[object_id].unit[i][request[current_request].unread_block]-disk_head[i].pos)%V+1;
                }
                else{  // 如果磁头太远了，就直接jump
                    printf("j %d\n", object[object_id].unit[i][request[current_request].unread_block]);
                    disk_head[i].pos=object[object_id].unit[i][request[current_request].unread_block];
                }
                if(request[current_request].unread_block>object[object_id].size)
                {
                    request[current_request].is_done=true;
                }
            }
        }
    }
    // if (!current_request) {
    //     for (int i = 1; i <= N; i++) {  // 如果n_read = 0，即当前时间片没有请求内容，则所有硬盘的磁头都不动，输出“0”表示没有读操作
    //         printf("#\n");
    //     }
    //     printf("0\n");  // 若当前时间片读取了前面时间片请求的内容，该如何输出？？？
    // } else {
    //     current_phase++;
    //     object_id = request[current_request].object_id;
        

    //     if (current_phase == object[object_id].size * 2) {  // 该对象读取完毕
    //         if (object[object_id].is_delete) {
    //             printf("0\n");
    //         } else {
    //             printf("1\n%d\n", current_request);
    //             request[current_request].is_done = true;
    //         }
    //         current_request++;
    //         current_phase = 0;
    //     } else {
    //         printf("0\n");
    //     }
    // }

    fflush(stdout);
}

void clean()
{
    for (auto& obj : object) {
        for (int i = 1; i <= REP_NUM; i++) {
            if (obj.unit[i] == nullptr)
                continue;
            free(obj.unit[i]);
            obj.unit[i] = nullptr;
        }
    }
}

int main()
{
    scanf("%d%d%d%d%d", &T, &M, &N, &V, &G);
    // T+105为本次数据的时间片数，其中最后105个时间片判题器不会再输入写入、删除和读取请求
    // M为对象标签数
    // N为硬盘实际个数，V为每个硬盘有的存储单元的个数，G为每个磁头每个时间片最多消耗的令牌数

    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%d", &fre_del[i][j]);
        }
    }

    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%d", &fre_write[i][j]);
        }
    }

    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%d", &fre_read[i][j]);
        }
    }

    for (int i = 1; i <= N; i++) {
        disk_head[i].pos = 1;
        disk_head[i].token = G;
    }
    // 对每个盘进行分区操作
    for (int tag_id = 1; tag_id <= M; tag_id++) {
        tag_block_address[tag_id] = (tag_id - 1) * V / M + 1;  // 每个盘的TAG分区一样
    }

    printf("OK\n");
    fflush(stdout);

    for (int t = 1; t <= T + EXTRA_TIME; t++) {
        timestamp_action();
        delete_action();
        write_action();
        read_action();
    }
    clean();

    return 0;
}