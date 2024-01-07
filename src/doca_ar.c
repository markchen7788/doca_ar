/**
 * @file doca_ar.c
 * @author Mark Chen (markchen77888@gmail.com)
 * @brief the main function of DOCA-AR
 * @version 1.0
 * @date 2024-01-07
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#include "doca_ar_env.h"
#include "doca_ar_pipe.h"
#include "doca_ar_core.h"

/**
 * @brief the main function of DOCA-AR
 * 
 * @param argc 
 * @param argv 
 * @return int 
 */
int main(int argc, char **argv)
{
    if (doca_ar_env_init(argc, argv) != DOCA_SUCCESS)
        goto EXIT;
    if (doca_ar_pipe_init())
        goto EXIT;

    doca_ar();

EXIT:
    doca_ar_env_destroy();
    return 0;
}