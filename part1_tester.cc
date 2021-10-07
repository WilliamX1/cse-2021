/*
 *  You need not go through or edit this file to complete this lab.
 * 
 * */

/* 
 * By Yutao Liu
 * 
 * 	for CSP-2013-Fall
 * ---------------
 * 
 * By Zhe Qiu
 * 	
 * 	for CSE-2014-Undergraduate
 * 
 * -----------------
 * 
 * By Fuqian Huang
 *
 *	for CSE-2018-Autumn
 * 
 * -----------------
 * 
 * By Jiahuan Shen
 * 
 *  for CSE-2021-Fall
 * 
 * */


/* part1 tester.  
 * Test whether extent_client -> extent_server -> inode_manager behave correctly
 */

#include "extent_client.h"
#include <stdio.h>

#define FILE_NUM 50
#define LARGE_FILE_SIZE_MIN 512*10
#define LARGE_FILE_SIZE_MAX 512*200

#define iprint(msg) \
    printf("[TEST_ERROR]: %s\n", msg);
extent_client *ec;
int total_score = 0;

int test_create_and_getattr()
{
    int i, rnum;
    extent_protocol::extentid_t id;
    extent_protocol::attr a;

    printf("========== begin test create and getattr ==========\n");

    srand((unsigned)time(NULL));

    for (i = 0; i < FILE_NUM; i++) {
        rnum = rand() % 10;
        memset(&a, 0, sizeof(a));
        if (rnum < 3) {
            ec->create(extent_protocol::T_DIR, id);
            if ((int)id == 0) {
                iprint("error creating dir\n");
                return 1;
            }
            if (ec->getattr(id, a) != extent_protocol::OK) {
                iprint("error getting attr, return not OK\n");
                return 2;
            }
            if (a.type != extent_protocol::T_DIR) {
                iprint("error getting attr, type is wrong");
                return 3;
            }
        } else {
            ec->create(extent_protocol::T_FILE, id);
            if ((int)id == 0) {
                iprint("error creating dir\n");
                return 1;
            }
            if (ec->getattr(id, a) != extent_protocol::OK) {
                iprint("error getting attr, return not OK\n");
                return 2;
            }
            if (a.type != extent_protocol::T_FILE) {
                iprint("error getting attr, type is wrong");
                return 3;
            }
        }
    } 
    total_score += 40; 
    printf("========== pass test create and getattr ==========\n");
    return 0;
}

int test_indirect()
{
    int i, j, k, rnum, size;
    char *temp = (char *)malloc(LARGE_FILE_SIZE_MAX);
    extent_protocol::extentid_t id_list[FILE_NUM];
    std::string content[FILE_NUM];
    printf("begin test indirect\n");
    srand((unsigned)time(NULL));

    for (i = 0; i < FILE_NUM; i++) {
        if (ec->create(extent_protocol::T_FILE, id_list[i]) != extent_protocol::OK) {
            printf("error create, return not OK\n");
            return 1;
        }
    }

    for (j = 0; j < 10; j++) {
        for (i = 0; i < FILE_NUM; i++) {
            memset(temp, 0, LARGE_FILE_SIZE_MAX);
            size = (rand() % (LARGE_FILE_SIZE_MAX - LARGE_FILE_SIZE_MIN)) + LARGE_FILE_SIZE_MIN;
            for (k = 0; k < size; k++) {
                rnum = rand() % 26;
                temp[k] = 97 + rnum;
            }
            content[i] = std::string(temp);
            if (ec->put(id_list[i], content[i]) != extent_protocol::OK) {
                printf("error put, return not OK\n");
                return 1;
            }
        }

        for (i = 0; i < FILE_NUM; i++) {
            std::string buf;
            if (ec->get(id_list[i], buf) != extent_protocol::OK) {
                printf("error get, return not OK\n");
                return 2;
            }
            if (buf.compare(content[i]) != 0) {
                std::cout << "error get large file, not consistent with put large file : " << 
                    buf << " <-> " << content[i] << "\n";
                // return 3;
            }
        }
    }

    for (i = 0; i < FILE_NUM; i++) {
        if (ec->remove(id_list[i]) != extent_protocol::OK) {
            printf("error remove, return not OK\n");
            return 4;
        }
    }

    total_score += 10;
    printf("end test indirect\n");
    return 0;
}

int test_put_and_get()
{
    int i, rnum;
    extent_protocol::extentid_t id;
    extent_protocol::attr a;
    int contents[FILE_NUM];
    char *temp = (char *)malloc(10);

    printf("========== begin test put and get ==========\n");
    srand((unsigned)time(NULL));
    for (i = 0; i < FILE_NUM; i++) {
        memset(&a, 0, sizeof(a));
        id = (extent_protocol::extentid_t)(i+2);
        if (ec->getattr(id, a) != extent_protocol::OK) {
            iprint("error getting attr, return not OK\n");
            return 1;
        }
        if (a.type == extent_protocol::T_FILE) {
            rnum = rand() % 10000;
            memset(temp, 0, 10);
            sprintf(temp, "%d", rnum);
            std::string buf(temp);
            if (ec->put(id, buf) != extent_protocol::OK) {
                iprint("error put, return not OK\n");
                return 2;
            }
            contents[i] = rnum;
        }
    }
    for (i = 0; i < FILE_NUM; i++) {
        memset(&a, 0, sizeof(a));
        id = (extent_protocol::extentid_t)(i+2);
        if (ec->getattr(id, a) != extent_protocol::OK) {
            iprint("error getting attr, return not OK\n");
            return 3;
        }
        if (a.type == extent_protocol::T_FILE) {
            std::string buf;
            if (ec->get(id, buf) != extent_protocol::OK) {
                iprint("error get, return not OK\n");
                return 4;
            }
            memset(temp, 0, 10);
            sprintf(temp, "%d", contents[i]);
            std::string buf2(temp);
            if (buf.compare(buf2) != 0) {
                std::cout << "[TEST_ERROR] : error get, not consistent with put " << 
                    buf << " <-> " << buf2 << "\n\n";
                return 5;
            }
        }
    } 

    total_score += 30;
    printf("========== pass test put and get ==========\n");
    return 0;
}

int test_remove()
{
    int i;
    extent_protocol::extentid_t id;
    extent_protocol::attr a;
    
    printf("========== begin test remove ==========\n");
    for (i = 0; i < FILE_NUM; i++) {
        memset(&a, 0, sizeof(a));
        id = (extent_protocol::extentid_t)(i+2);
        if (ec->remove(id) != extent_protocol::OK) {
            iprint("error removing, return not OK\n");
            return 1;
        }
        ec->getattr(id, a);
        if (a.type != 0) {
            iprint("error removing, type is still positive\n");
            return 2;
        }
    }
    total_score += 20; 
    printf("========== pass test remove ==========\n");
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 1) {
        printf("Usage: ./part1_tester\n");
        return 1;
    }
  
    ec = new extent_client();

    if (test_create_and_getattr() != 0)
        goto test_finish;
    if (test_put_and_get() != 0)
        goto test_finish;
    if (test_remove() != 0)
        goto test_finish;
    if (test_indirect() != 0)
        goto test_finish;

test_finish:
    printf("---------------------------------\n");
    printf("Part1 score is : %d/100\n", total_score);
    return 0;
}
