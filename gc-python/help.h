/* 
 * test_cond("Check if 1 == 1", 1==1)
 * test_cond("Check if 5 > 10", 5 > 10)
 * test_report()
 */


#ifndef __TESTHELP_H
#define __TESTHELP_H

extern int __failed_tests;
extern int __test_num;

#define assert_s(descr,_c) do { \
    __test_num++; printf("%d - %s: ", __test_num, descr); \
    if(_c) printf("PASSED\n"); else {printf("FAILED\n"); __failed_tests++;} \
} while(0);
#define test_report() do { \
    printf("%d tests, %d passed, %d failed\n", __test_num, \
                    __test_num-__failed_tests, __failed_tests); \
    if (__failed_tests) { \
        printf("=== WARNING === We have failed tests here...\n"); \
        exit(1); \
    } \
} while(0);

#endif
