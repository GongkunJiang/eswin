#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "mbedtls/ecdh.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/platform.h"

// __LINE__：在源代码中插入当前源代码行号；
#define assert_exit(cond, ret) \
    do { if (!(cond)) { \
        printf("  !. assert: failed [line: %d, error: -0x%04X]\n", __LINE__, -ret); \
        goto cleanup; \
    } } while (0)

static void dump_buf(char *info, uint8_t *buf, uint32_t len)
{
    mbedtls_printf("%s", info);// config.h 2870
    for (int i = 0; i < len; i++) {
        mbedtls_printf("%s%02X%s", i % 16 == 0 ? "\n     ":" ", 
                        buf[i], i == len - 1 ? "\n":"");
    }
}

int main(void)
{
    int ret = 0;
    /*
    size_t主要用于计数，如sizeof函数返回值类型即为size_t。
    在不同位的机器中所占的位数也不同，size_t是无符号数，ssize_t是有符号数。
    在32位机器中定义为：typedef  unsigned int size_t; （4个字节）
    在64位机器中定义为：typedef  unsigned long size_t;（8个字节）
    */
    size_t olen;
    char buf[65];
    mbedtls_ecp_group grp;//  ECP group structure. ecp.h 148
    mbedtls_mpi cli_secret, srv_secret;// MPI (Multi-precision integer) structure bignum.h 203
    mbedtls_mpi cli_pri, srv_pri;
    mbedtls_ecp_point cli_pub, srv_pub;//ECP point structure ecp.h 131
    mbedtls_entropy_context entropy;//Entropy context structure  entropy.h 146
    // PRNG from a block-cipher in counter mode operation Using Deterministic Random Bit Generators
    mbedtls_ctr_drbg_context ctr_drbg;//The CTR_DRBG context structure. ctr_drbg.h 168
    uint8_t *pers = "simple_ecdh";// typedef unsigned char           uint8_t;

    mbedtls_mpi_init(&cli_pri); //Initialize cli_pri MPI 
    mbedtls_mpi_init(&srv_pri);
    mbedtls_mpi_init(&cli_secret); 
    mbedtls_mpi_init(&srv_secret);
    mbedtls_ecp_group_init(&grp); //初始化椭圆曲线群结构体
    mbedtls_ecp_point_init(&cli_pub); //初始化椭圆曲线点结构体 cli
    mbedtls_ecp_point_init(&srv_pub);//初始化椭圆曲线点结构体 srv
    mbedtls_entropy_init(&entropy); //初始化熵结构体
    mbedtls_ctr_drbg_init(&ctr_drbg);//初始化随机数结构体

    // This function seeds and sets up the CTR_DRBG entropy source for future reseeds.
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, 
                                (const uint8_t *) pers, strlen(pers));
    mbedtls_printf("\n  . setup rng ... ok\n");

    //加载椭圆曲线，选择SECP256R1
    ret = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
    mbedtls_printf("\n  . select ecp group SECP256R1 ... ok\n");
    //cli生成公开参数
    ret = mbedtls_ecdh_gen_public(&grp,    //椭圆曲线结构体
    							  &cli_pri,//输出cli私密参数d
    							  &cli_pub,//输出cli公开参数Q
                                  mbedtls_ctr_drbg_random, &ctr_drbg);
    assert_exit(ret == 1, ret);
    mbedtls_ecp_point_write_binary(&grp, &cli_pub, //把cli的公开参数到处到buf中
                            MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, buf, sizeof(buf));
    dump_buf("  1. ecdh client generate public parameter:", buf, olen);

	//srv生成公开参数
    ret = mbedtls_ecdh_gen_public(&grp,    //椭圆曲线结构体
    							  &srv_pri,//输出srv私密参数d
    							  &srv_pub,//输出srv公开参数Q
                                  mbedtls_ctr_drbg_random, &ctr_drbg);
    assert_exit(ret == 0, ret);
    mbedtls_ecp_point_write_binary(&grp, &srv_pub, //把srv的公开参数导出到buf中
                            MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, buf, sizeof(buf));
    dump_buf("  2. ecdh server generate public parameter:", buf, olen);
    //cli计算共享密钥
    ret = mbedtls_ecdh_compute_shared(&grp,    //椭圆曲线结构体
                                      &cli_secret, //cli计算出的共享密钥
                                      &srv_pub, //输入srv公开参数Q
                                      &cli_pri, //输入cli本身的私密参数d
                                      mbedtls_ctr_drbg_random, &ctr_drbg);
    assert_exit(ret == 0, ret);
	//把cli计算出的共享密钥导出buf中
    mbedtls_mpi_write_binary(&cli_secret, buf, mbedtls_mpi_size(&cli_secret));//Return the total size in bytes
    dump_buf("  3. ecdh client generate secret:", buf, mbedtls_mpi_size(&cli_secret));

	//srv计算共享密钥
    ret = mbedtls_ecdh_compute_shared(&grp,   //椭圆曲线结构体
                                      &srv_secret, //srv计算出的共享密钥
                                      &cli_pub, //输入cli公开参数Q
                                      &srv_pri, //输入srv本身的私密参数d
                                      mbedtls_ctr_drbg_random, &ctr_drbg);
    assert_exit(ret == 0, ret);
	//把srv计算出的共享密钥导出buf中
    mbedtls_mpi_write_binary(&srv_secret, buf, mbedtls_mpi_size(&srv_secret));
    dump_buf("  4. ecdh server generate secret:", buf, mbedtls_mpi_size(&srv_secret));

    //比较2个大数是否相等
    ret = mbedtls_mpi_cmp_mpi(&cli_secret, &srv_secret);
    assert_exit(ret == 0, ret);
    mbedtls_printf("  5. ecdh checking secrets ... ok\n");

cleanup:
    mbedtls_mpi_free(&cli_pri); 
    mbedtls_mpi_free(&srv_pri);
    mbedtls_mpi_free(&cli_secret); 
    mbedtls_mpi_free(&srv_secret);
    mbedtls_ecp_group_free(&grp);
    mbedtls_ecp_point_free(&cli_pub); 
    mbedtls_ecp_point_free(&srv_pub);
    mbedtls_entropy_free(&entropy); 
    mbedtls_ctr_drbg_free(&ctr_drbg);

    return 0;
}
