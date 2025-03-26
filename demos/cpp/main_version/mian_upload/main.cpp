#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <deque>
#include<map>
#include<algorithm>
#include <cmath>

#define MAX_DISK_NUM (10 + 1)  // 硬盘最多10个，10+1是为了让你计数方便不从0开始
#define MAX_DISK_SIZE (16384 + 1)  // 每个硬盘的存储单元数，+1是为了让你计数方便不从0开始 
#define MAX_REQUEST_NUM (30000000 + 1)  // 读取次数小于等于30000000次，也是从1开始计数
#define MAX_OBJECT_NUM (100000 + 1)  // 最多100000种对象，对象的id从1-100000
#define MAX_TAG_NUM (17)  // 最多16种标签
#define MAX_SLOT_NUM (49)  // 最多86400/1800个长时隙
#define REP_NUM (3)  // 副本的个数
#define FRE_PER_SLICING (1800)  // 一个长时隙占1800个时间片
#define EXTRA_TIME (105)  // 判题器额外给出的105个时隙供处理

#define ROLL_TAG_TS_FRE (65)
#define ABORT_LOW_SCORE_REQ_TS_FRE (75)

// 增加了存储排序的内容：20250324yzm
#include <numeric>
std::vector<int> Peak(MAX_TAG_NUM-1,0);   //存储了Tag对应的最大所需存储空间
std::vector<int> CDF_type(MAX_TAG_NUM,0);
std::vector<int> indices_Sort(MAX_TAG_NUM-1,0);
std::vector<int> Disk_size_Tag;



typedef struct Request_ {
    int object_id;  // 对象id
    int object_size;  // 对象的size
    int remain_size; // 还剩下多少个块没有读，为0时就是读完了，需要及时done
    int prev_id;  // 该对象的前一个读取请求的req_id
    int* unread_block;  // 该请求中，未被读取的块的索引（1~size）,对应索引位置为0就代表没有读
    bool is_done;  //请求被完成
    bool is_abort;  // 该请求是否被取消
} Request;

typedef struct Request_id_{
    int requestid;  //请求的号
    int ts_create;  //请求创建的时间
    double score;
} Request_Id;


typedef struct Object_ {
    int replica[REP_NUM + 1];  // 副本
    int* unit[REP_NUM + 1];
    int size;
    int tag;
    int true_tag_area[REP_NUM + 1];  // 每个副本真正所在的TAG分区
    int last_request_point; // 该对象上一次被请求的req_id（如果需要读上上个被请求的req_id就得到request数组读）
    bool is_delete;
} Object;

typedef struct Disk_Head_ {
    int pos;  // 磁头位置
    int last_status;  // 磁头的上个动作消耗的令牌数 -1：上个动作是跳，1：上个动作是pass，64-16，上个动作是读；初始化为0，表示第一个时间片刚开始，磁头之前没有任何动作。
}Disk_Head;

Request request[MAX_REQUEST_NUM];  // 用一个数组保存所有请求
Object object[MAX_OBJECT_NUM];  // 对象的id就是用object数组的index表示
Disk_Head disk_head[MAX_DISK_NUM];  //磁头数组：[MAX_DISK_NUM]

int T, M, N, V, G;
int TS; //timestamp
int disk[MAX_DISK_NUM][MAX_DISK_SIZE][2];  //每块硬盘用三维数组表示，[xx][yy][0]代表object_id，[xx][yy][1]代表block_id（不会超过其size）
int fre_del[MAX_TAG_NUM][MAX_SLOT_NUM];  // 第i行第j个元素表示在j时隙内，所有删除操作中对象标签为i的对象大小之和
int fre_write[MAX_TAG_NUM][MAX_SLOT_NUM];  // 第i行第j个元素表示在j时隙内，所有写入操作中对象标签为i的对象大小之和
int fre_read[MAX_TAG_NUM][MAX_SLOT_NUM];  // 第i行第j个元素表示在j时隙内，所有读取操作中对象标签为i的对象大小之和，同一个对象的多次读取会重复计算
int tag_block_address[MAX_TAG_NUM];
int free_block_num[MAX_DISK_NUM][MAX_TAG_NUM];  // 各磁盘每个分区空闲块数量,初始化为V/M
std::deque<Request_Id> no_need_to_abort; //在范围内的数组
void write_to_file(int num1, int num2, int num3,int num4,int num5);

std::vector<int> allocateDisks(const std::vector<std::pair<int, int>>& fre_tag) {
    // 得到请求频率最高的前num个TAG，并计算其频次的区间和
    int num = std::min(N,M);
    std::vector<int> top_keys(num);
    std::vector<int> top_values(num);
    for (int i = 0; i < num; ++i) {
        top_keys[i]=fre_tag[i+1].first;
        if(i>0){
            top_values[i]=fre_tag[i+1].second+top_values[i-1];  // 区间和
        }else{
            top_values[i]=fre_tag[i+1].second;
        }
        
    }

    int total_value = top_values.back();
    std::vector<int> result(N+1);

    for (int i=1;i<=N;i++)  // 根据区间和，将TAG按请求频次比例分给disk
    {
        bool flag=false;
        for(int j=0;j<num;++j){
            if(i<std::round(static_cast<double>(top_values[j]) / total_value * N)+1){
                result[i]=top_keys[j];
                flag= true;
                break;
            }
        }
        if(!flag){result[i]=top_keys[0];}
    }
    // 实现打散，假设tag比例是6，3，1，那么原本可能就是前六个盘读tag1，7-9读tag2，10都tag3
    // 但是一个内容是连续三个盘存的，打散一点的话，应该可能读到更多的内容
    for(int i=1;i<=N/2;i=i+2){
        std::swap(result[i], result[N+1-i]);
    }

    return result;
}

void write_to_file(int num1, int num2, int num3,int num4,int num5) {
    // 固定文件名
    const char *filename = "output.txt";

    // 以追加模式打开文件
    std::ofstream file;
    file.open(filename, std::ios::app);  // 使用 std::ios::app 表示追加模式

    if (!file.is_open()) {
        // 如果文件打开失败，打印错误信息并退出
        std::cerr << "Error: Unable to open file " << filename << std::endl;
        return;
    }

    // 写入三个整数到文件，以空格分隔，并换行
    file << num1 << " " << num2 << " " << num3 <<" "<<num4 <<" "<<num5<< std::endl;

    // 关闭文件
    file.close();
}

void timestamp_action()  // 时间片对齐事件
{
    
    int timestamp;
    scanf("%*s%d", &timestamp);
    printf("TIMESTAMP %d\n", timestamp);
    // if(timestamp>=86504){write_to_file(timestamp, 1, 1, 1, 1);}

    fflush(stdout);
}

inline int max(int a, int b) {
    return (a > b) ? a : b;
}

void do_object_delete(const int* object_unit, int (*disk_unit)[2], int size)
{
    for (int i = 1; i <= size; i++) {
        disk_unit[object_unit[i]][0] = 0;
        disk_unit[object_unit[i]][1] = 0;
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
            free_block_num[object[id].replica[j]][object[id].true_tag_area[j]] += object[id].size;
        }
        object[id].is_delete = true;
    }

    printf("%d\n", abort_num);
    for (int i = 1; i <= abort_num; i++) {  // 输出删除的请求编号req_id
        printf("%d\n", req_id_print[i]);
    }

    fflush(stdout);
}

void do_object_write(int* object_unit, int (*disk_unit)[2], int size, int object_id, bool flag, int j)
{
    int current_write_point = 0;
    int start_address = tag_block_address[object[object_id].tag];  // 每个TAG分区在各个盘中的首地址（相同）
    object[object_id].true_tag_area[j] = object[object_id].tag;
    // 该对象不能在当前分区完整存下
    if (flag) {
        // 找到可以完整存下对象的分区
        object[object_id].true_tag_area[j] = object[object_id].tag % M + 1;
        while (1) {
            if (free_block_num[object[object_id].replica[j]][object[object_id].true_tag_area[j]] - size >= 0) break; 
            object[object_id].true_tag_area[j] = object[object_id].true_tag_area[j] % M + 1;
        }
        start_address = tag_block_address[object[object_id].true_tag_area[j]];
    }
    for (int i = start_address; i < start_address+Disk_size_Tag[object[object_id].true_tag_area[j]-1]; i++) {
        if (disk_unit[i][0] == 0) {  // 如果该存储单元是空的则存，不是空的去下一个单元，所以不是连续存储
            disk_unit[i][0] = object_id;  // disk[0]存的是对象id
            disk_unit[i][1] = ++current_write_point; //disk[1]存的是对象块的编号
            object_unit[current_write_point] = i;  // object_unit即unit[j]是数组名（数组首地址），存的是存储单元编号
            if (current_write_point == size) {
                break;  // 存完了这个对象
            }
        }
    }
    // assert(current_write_point == size);
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
        object[id].is_delete = false;
        for (int j = 1; j <= REP_NUM; j++) {
            object[id].replica[j] = (id + j) % N + 1;  // 得到主盘与两个副本盘编号
            object[id].unit[j] = static_cast<int*>(malloc(sizeof(int) * (size + 1)));  // unit中的元素对应相应副本的首地址
            bool flag = false;
            if (free_block_num[object[id].replica[j]][object[id].tag] < object[id].size) flag = true;
            // 如果要存到别的分区去,也要全存在一个分区里
            do_object_write(object[id].unit[j], disk[object[id].replica[j]], size, id, flag, j);
            free_block_num[object[id].replica[j]][object[id].true_tag_area[j]] -= object[id].size;
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
    static int req_count = 0;
    static int completed_req_id[MAX_REQUEST_NUM];  // 保存的是当前时间片完成的请求的id
    int completed_req_count = 0;  // 这个时间片完成的请求数
    int n_read;  // 这一时间片读取对象的个数
    int request_id, object_id;
    scanf("%d", &n_read);

    for (int i = 1; i <= n_read; i++) {
        scanf("%d%d", &request_id, &object_id);
        request[request_id].object_id = object_id;
        request[request_id].object_size = object[object_id].size;
        request[request_id].remain_size = object[object_id].size;
        request[request_id].prev_id = object[object_id].last_request_point;
        object[object_id].last_request_point = request_id;
        request[request_id].is_done = false;
        request[request_id].is_abort = false;
        request[request_id].unread_block = static_cast<int*>(calloc(request[request_id].object_size + 1, sizeof(int)));  //记得在clean里面free
        Request_Id requestinfo;
        requestinfo.requestid=request_id;
        requestinfo.ts_create=TS;
        no_need_to_abort.push_back(requestinfo);

    }
    //如果ts_create小于TS-105,则出队
    // if(TS>=86504){write_to_file(TS, 12, no_need_to_abort.size(), n_read, 12);}
    if(!no_need_to_abort.empty())
    {
        while(TS-no_need_to_abort.front().ts_create > ABORT_LOW_SCORE_REQ_TS_FRE)
        {
            request[no_need_to_abort.front().requestid].is_abort = true;
            no_need_to_abort.pop_front();
            if(no_need_to_abort.empty()){break;}
        }
    }
    // if(TS>=86504){write_to_file(TS, 22, no_need_to_abort.size(), n_read, 22);}
    req_count += n_read;
    int req_completed = 0;  // 这个时间片完成了多少请求
    // 每个大时隙（1800）初，将流行的TAG按比例分给disk，各磁头跳到分配的TAG分区首地址
    static std::vector<int> most_fre_index(N + 1);
    if ((TS - 1) % 1800 == 0) {
        std::vector<std::pair<int, int>> fre_tag;
        for (int tag = 1; tag <= M; tag++) {
            fre_tag.push_back(std::make_pair(tag, fre_read[tag][TS / 1800 + 1]));
        }
        std::sort(fre_tag.begin(), fre_tag.end(),
            [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
            return a.second > b.second; // 降序排序
        });
        // for(std::pair<int, int> m : fre_tag)
        // {
        //     write_to_file(TS,fre_tag.size(),m.first,m.second,1);
        // }
        std::vector<int> result = allocateDisks(fre_tag);
        for(int i=1;i<=N;++i){
            most_fre_index[i] = result[i];
        }
    }
    static int mycount = 0;
    for (int i = 1; i <= N; i++) {  // 对每个磁头都进行操作
        int token = G;  // 时间片初始化  // 当前时间片的可消耗令牌数
        // 尝试无效备注:如果磁头不在指定的TAG分区内,则跳回
        while (token > 0) {
            // 尝试无效备注:复用后面的PASS or JUMP代码,性能反而降低,所以还是直接跳吧
            if ((TS - 1) % 1800 == 0) {
                mycount=0;
                disk_head[i].pos = tag_block_address[most_fre_index[i]];
                disk_head[i].last_status = -1;
                printf("j %d\n", disk_head[i].pos);
                break;
            }
            else if((TS - 1) % ROLL_TAG_TS_FRE == 0){
                ++mycount;
                // 如果回到了分配的TAG分区,则跳到该分区的中间
                // if ((i + mycount - 1) % N + 1 == i) disk_head[i].pos = tag_block_address[most_fre_index[i]] + V/M/2;
                disk_head[i].pos = tag_block_address[most_fre_index[(i+mycount-1)%N+1]];
                disk_head[i].last_status = -1;
                printf("j %d\n", disk_head[i].pos);
                break;
            }
            // if(TS>=86504){write_to_file(TS, i, token, n_read, 1);}
            int current_disk_head = disk_head[i].pos;
            int last_status = disk_head[i].last_status; //上一次动作，-1：j; 1：p; 其他数字表示上次的token消耗
            int current_point_objid = disk[i][current_disk_head][0];  // disk[i][disk_head[i]][0]表示当前硬盘当前磁头对应位置写入的object_id，未写入是0
            int current_point_objblock = disk[i][current_disk_head][1];  // 对象的块的编号
            int not_find =0; //表征是不是没有找到
            while (current_point_objid == 0 || request[object[current_point_objid].last_request_point].is_done ||
                request[object[current_point_objid].last_request_point].is_abort) {
                // 如果当前磁头指向空位置或者是所指向位置所对应的请求已经被删除或者是丢弃（感觉is_abort有可能没有用了）
                // 假设对同一个对象的请求中，后到的总是不早于先到的done，也就是说如果后到的请求都done，那么先到的肯定也done
                current_disk_head = current_disk_head % V + 1;
                current_point_objid = disk[i][current_disk_head][0];
                current_point_objblock = disk[i][current_disk_head][1];
                if (current_disk_head == disk_head[i].pos) //如果走了一圈还是没有找到就离开吧
                {
                    /* code */
                    not_find =1;
                    break;
                }
            }

            if (not_find == 0)
            {
                // if(TS>=86504){write_to_file(TS, i, token, n_read, 2);}
                /* code */
                // while循环之后，就能保证current_disk_head指向的是可以读的内容（该对象被请求了而且该请求没有被完成）
                int current_req_id = object[current_point_objid].last_request_point;
                if (current_req_id!=0) {
                    // if(TS>=86504){write_to_file(TS, i, token, n_read, 10);}
                    if (current_disk_head == disk_head[i].pos) {  // 当前磁头没有额外移动可以直接读
                        // 根据last_status计算这一次读要消耗的令牌数，如果剩余令牌>=要消耗的令牌，则读取成功，否则进入下一个时间片
                        int ceil = max(16,(last_status * 8 + 9) / 10);  // (last_status*8+9)/10就能保证是last_status*0.8还向上取整
                        if (last_status <= 1) {  // 上个动作是跳或者pass，或者是第一个时间片首次"Read"，令牌-64
                            // last_status == -1 || last_status == 1 || last_status == 0直接改成<=1
                            if (token >= 64) { last_status = 64; }
                            else {
                                // if(TS>=86504){write_to_file(TS, i, token, n_read, 8);}
                                printf("#\n");
                                break;
                            }
                        } else if (token >= ceil) { //如果token个数大于ceil，则表明这次读
                            last_status = ceil;
                        } else
                        {
                            // if(TS>=86504){write_to_file(TS, i, token, n_read, 9);}
                            printf("#\n");
                            break;
                        }

                        // 对应请求中，读过的块置1,remiansize减一
                        if (request[current_req_id].unread_block[current_point_objblock] ==
                            0 && last_status>=16) {  // 没读过才读,如果最后来的请求没有读过这个块，合理推测前面的请求也有可能没读过。
                            // 但是现在的请求读过这个块的话，暂时认为之前的请求也读过了这个块
                            // write_to_file(TS,i,token,last_status,ceil);
                            token -= last_status;  // Read动作消耗令牌

                            printf("r");
                            request[current_req_id].unread_block[current_point_objblock] = 1;
                            if (--request[current_req_id].remain_size == 0) {
                                completed_req_id[++req_completed] = current_req_id;  // 如果请求的对象的每个块都读完了就记录，然后这个req置为is_done
                                request[current_req_id].is_done = true;
                            }
                            disk_head[i].pos = disk_head[i].pos % V + 1;
                            disk_head[i].last_status = last_status;

                            // 当前磁头指向的block可以满足若干同一对象请求中的同一个block请求，如果上一个请求存在且未完成才进这个循环
                            while (request[current_req_id].prev_id != 0 &&
                                    !request[request[current_req_id].prev_id].is_done) {
                                // if(TS>=86504){write_to_file(TS, i, token, n_read, 3);}
                                current_req_id = request[current_req_id].prev_id;
                                // 找上一个对该对象的请求看看要不要读，要的话就顺便满足其需求：相应unread_block位置置1
                                if (request[current_req_id].unread_block[current_point_objblock] == 0) {
                                    request[current_req_id].unread_block[current_point_objblock] = 1;
                                    if (--request[current_req_id].remain_size == 0) {
                                        completed_req_id[++req_completed] = current_req_id;  // 如果请求的对象的每个块都读完了就记录
                                        request[current_req_id].is_done = true;
                                    }
                                }
                            }
                        } else {  // 读过了就Pass
                            printf("p");
                            --token;
                            disk_head[i].pos = disk_head[i].pos % V + 1;
                            disk_head[i].last_status = 1;
                        }
                    } else {
                        int pass_num;
                            //  这个地方求余数出现了泄露，会得到负数
                        if (current_disk_head >= disk_head[i].pos){
                            pass_num = current_disk_head - disk_head[i].pos ;  // 计算实际上磁头要pass多少次才能到下一个有效的读位

                        }else
                        {
                            pass_num = current_disk_head+V - disk_head[i].pos;
                        }
                        //分多种情况，1.token==G，跳还是不跳：此时判断G的令牌数能否支撑pass到目标位置且读取，能就不跳，不能就跳
                        // 2.token!=G，此时判断此时判断G的令牌数能否支撑pass到目标位置，能就pass，不能就结束磁头动作。读的动作让下个while循环去处理
                        if (token == G) {
                            if (pass_num + 64 >
                                token) //pass_num<token但是pass_num+64>token的情况，相当于是可以通过pass到对应位置之后啥也干不了，还不如就直接跳
                            {
                                printf("j %d\n", current_disk_head);
                                disk_head[i].pos = current_disk_head;
                                disk_head[i].last_status = -1;
                                break;
                            } else //pass_num+64<=token的情况，通过pass之后还有余力读
                            {
                                token = token - pass_num;
                                while (pass_num > 0) {
                                    --pass_num;
                                    printf("p");
                                }
                                disk_head[i].pos = current_disk_head;
                                disk_head[i].last_status = 1;
                            }
                        } else {
                            if (pass_num + 64 > token) {
                                printf("#\n");
                                break;
                            } else {
                                token = token - pass_num;
                                while (pass_num > 0) {
                                    --pass_num;
                                    printf("p");
                                }
                                disk_head[i].pos = current_disk_head;
                                disk_head[i].last_status = 1;
                            }
                        }
                    }
                } else
                {
                    // if(TS>=86504){write_to_file(TS, i, token, n_read, 11);}
                    printf("#\n");
                    // continue;
                    break;
                }

            }
            else{
                // if(TS>=86504){write_to_file(TS, i, token, n_read, 6);}
                printf("#\n");
                break;
            }
            // 为了防止刚好用完350个token，r之后不进入循环，导致少打了一个#号，针对11851时隙
            if(token==0)
            {
                // if(TS>=86504){write_to_file(TS, i, token, n_read, 7);}
                printf("#\n");
                break;
            }
        // if(TS>=86504){write_to_file(TS, i, token, n_read, 4);}
        }
    }
    // if(TS>=86504){write_to_file(TS, req_completed, G, n_read, 5);}
    // 上报本次完成读取的req_id，必须要在for循环的外面
    if (req_completed>0) {  // 该对象读取完毕
        printf("%d\n",req_completed);
        for(int i= 1;i<=req_completed;i++)
        {
            printf("%d\n",completed_req_id[i]);
        }
    }
    else{
        printf("0\n");
//        break;
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

void Calculate_Peak_store()
{

    for(int i=1;i<=M;i++)
    {

        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            CDF_type[i]=CDF_type[i]+fre_read[i][j]-fre_del[i][j];
            if(CDF_type[i]>=Peak[i-1])
            {
                Peak[i-1] = CDF_type[i];
            }
        }
        // write_to_file(i,1,1,1,Peak[i]);
    }
    
    for (int i =0;i<Peak.size();i++)
    {
        indices_Sort[i]=i;
    }
    std::sort(indices_Sort.begin(), indices_Sort.end(), [&](int a, int b) {
        return Peak[a] > Peak[b]; // 比较 data[a] 和 data[b]
    });
    std::vector<int> sorted_data;
    int sum = std::accumulate(Peak.begin(), Peak.end(), 0);
    for (int peak_i : Peak)
    {
        // write_to_file(1,1,1,peak_i,0.0);
        // 这个版本中没有将所有的disk看成一个整体，而是对每一个盘进行分区
        Disk_size_Tag.push_back(floor(static_cast<double>(peak_i)/double(sum)*V));
    }
    for (int index : indices_Sort) {
        sorted_data.push_back(Peak[index]);
    }
    for (int index : indices_Sort) {
        sorted_data.push_back(Peak[index]);
    }
    Peak = sorted_data;
    // for(int i=0;i<M;i++)
    // {
    //     write_to_file(Peak[i],Disk_size_Tag[i],indices_Sort[i],Disk_size_Tag[i], 0.0);
    // }

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
    Calculate_Peak_store();

    // 对每个盘进行分区操作
    int temp_disk_point=1;
    for (int tag_id = 1; tag_id <= M; tag_id++) {

        tag_block_address[indices_Sort[tag_id-1]+1] = temp_disk_point;  // 每个盘的TAG分区一样
        temp_disk_point = temp_disk_point + Disk_size_Tag[indices_Sort[tag_id-1]];
    }

    for (int i = 1; i <= N; i++) {
        disk_head[i].pos = 1;
        disk_head[i].last_status = 0;
        for (int tag_id = 1; tag_id <= M; tag_id++) {
            free_block_num[i][tag_id] = Disk_size_Tag[tag_id-1];
            // write_to_file(1,i,tag_id,1,free_block_num[i][tag_id]);

        }
    }
    for (int i=0;i<M;i++)
    {
        write_to_file(1,1,Disk_size_Tag[i],free_block_num[1][i+1],tag_block_address[i+1]);

    }


    printf("OK\n");
    fflush(stdout);

    for (int t = 1; t <= T + EXTRA_TIME; t++) {
        TS=t;
        timestamp_action();
        delete_action();
        // if(TS>=86505){write_to_file(TS, 2, 2, 2, 2);}
        write_action();
        // if(TS>=86505){write_to_file(TS, 3, 3, 3, 3);}
        read_action();
        // if(TS>=86505){write_to_file(TS, 5, 5, 5, 5);}
    }
    clean();

    return 0;
}