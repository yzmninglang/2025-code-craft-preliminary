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
    bool is_done;
} Request;

typedef struct Object_ {
    int replica[REP_NUM + 1];  // 副本
    int* unit[REP_NUM + 1];
    int size;
    int last_request_point;
    bool is_delete;
} Object;

Request request[MAX_REQUEST_NUM];  // 用一个数组保存所有请求
Object object[MAX_OBJECT_NUM];  // 对象的id就是用object数组的index表示

int T, M, N, V, G;
int disk[MAX_DISK_NUM][MAX_DISK_SIZE];  //每块硬盘用二维数组表示
int disk_point[MAX_DISK_NUM];  //磁头数组？
int fre_del[MAX_TAG_NUM][MAX_SLOT_NUM];  // 第i行第j个元素表示在j时隙内，所有删除操作中对象标签为i的对象大小之和
int fre_write[MAX_TAG_NUM][MAX_SLOT_NUM];  // 第i行第j个元素表示在j时隙内，所有写入操作中对象标签为i的对象大小之和
int fre_read[MAX_TAG_NUM][MAX_SLOT_NUM];  // 第i行第j个元素表示在j时隙内，所有读取操作中对象标签为i的对象大小之和，同一个对象的多次读取会重复计算

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
            }
            current_id = request[current_id].prev_id;  // 该对象在编号为current_id的请求的前一次请求id
        }
    }

    printf("%d\n", abort_num);
    for (int i = 1; i <= n_delete; i++) {  // 输出删除的请求编号req_id
        int id = _id[i];
        int current_id = object[id].last_request_point;
        while (current_id != 0) {
            if (request[current_id].is_done == false) {
                printf("%d\n", current_id);
            }
            current_id = request[current_id].prev_id;
        }
        for (int j = 1; j <= REP_NUM; j++) {
            do_object_delete(object[id].unit[j], disk[object[id].replica[j]], object[id].size);
        }
        object[id].is_delete = true;
    }

    fflush(stdout);
}

void do_object_write(int* object_unit, int* disk_unit, int size, int object_id)
{
    int current_write_point = 0;
    for (int i = 1; i <= V; i++) {
        if (disk_unit[i] == 0) {  // 如果该存储单元是空的则存，不是空的去下一个单元，所以不是连续存储
            disk_unit[i] = object_id;
            object_unit[++current_write_point] = i;  // object_unit即unit[j]是数组名（数组首地址），存的是存储单元编号
            if (current_write_point == size) {
                break;  // 存完了这个对象
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
        int id, size;
        scanf("%d%d%*d", &id, &size);
        object[id].last_request_point = 0;
        for (int j = 1; j <= REP_NUM; j++) {
            object[id].replica[j] = (id + j) % N + 1;
            object[id].unit[j] = static_cast<int*>(malloc(sizeof(int) * (size + 1)));  // unit中的元素对应相应副本的首地址
            object[id].size = size;
            object[id].is_delete = false;
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
    int n_read;  // 这一时间片读取对象的个数
    int request_id, object_id;
    scanf("%d", &n_read);
    for (int i = 1; i <= n_read; i++) {
        scanf("%d%d", &request_id, &object_id);
        request[request_id].object_id = object_id;
        request[request_id].prev_id = object[object_id].last_request_point;
        object[object_id].last_request_point = request_id;
        request[request_id].is_done = false;
    }

    static int current_request = 0;
    static int current_phase = 0;
    if (!current_request && n_read > 0) {
        current_request = request_id;  // 本时间片的最后一个请求req_id[n_read] obj_id[n_read]
    }
    if (!current_request) {
        for (int i = 1; i <= N; i++) {  // 如果n_read = 0，即当前时间片没有请求内容，则所有硬盘的磁头都不动，输出“0”表示没有读操作
            printf("#\n");
        }
        printf("0\n");  // 若当前时间片读取了前面时间片请求的内容，该如何输出？？？
    } else {
        current_phase++;
        object_id = request[current_request].object_id;
        for (int i = 1; i <= N; i++) {
            if (i == object[object_id].replica[1]) {
                if (current_phase % 2 == 1) {
                    printf("j %d\n", object[object_id].unit[1][current_phase / 2 + 1]);  // TODO: 优化读取逻辑；将令牌消耗考虑
                } else {
                    printf("r#\n");
                }
            } else {
                printf("#\n");
            }
        }

        if (current_phase == object[object_id].size * 2) {  // 该对象读取完毕
            if (object[object_id].is_delete) {
                printf("0\n");
            } else {
                printf("1\n%d\n", current_request);
                request[current_request].is_done = true;
            }
            current_request = 0;
            current_phase = 0;
        } else {
            printf("0\n");
        }
    }

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

    printf("OK\n");
    fflush(stdout);

    for (int i = 1; i <= N; i++) {
        disk_point[i] = 1;
    }

    for (int t = 1; t <= T + EXTRA_TIME; t++) {
        timestamp_action();
        delete_action();
        write_action();
        read_action();
    }
    clean();

    return 0;
}