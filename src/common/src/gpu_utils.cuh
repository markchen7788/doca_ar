/*
 * Copyright (c) 2022 NVIDIA CORPORATION & AFFILIATES, ALL RIGHTS RESERVED.
 *
 * This software product is a proprietary product of NVIDIA CORPORATION &
 * AFFILIATES (the "Company") and all right, title, and interest in and to the
 * software product, including all associated intellectual property rights, are
 * and shall remain exclusively with the Company.
 *
 * This software product is governed by the End User License Agreement
 * provided with the software product.
 *
 */
#ifndef GPU_UTILS_H
#define GPU_UTILS_H

#include <stdint.h>
#include <rte_ethdev.h>

#if __cplusplus
extern "C" {
#endif

/* Bytes swap in a 16-bit value */
#define __bswap_constant_8(x) \
     ((unsigned short int) (((x) >> 8) | ((x) << 8)))

/*
 * Calculate the IPV4 header length
 *
 * @ipv4_hdr [in]: IPV4 header
 * @return: IPV4 header length
 */
__device__ __forceinline__ uint8_t
gpu_ipv4_hdr_len(const struct rte_ipv4_hdr *ipv4_hdr)
{
	return (uint8_t)((ipv4_hdr->version_ihl & RTE_IPV4_HDR_IHL_MASK) * RTE_IPV4_IHL_MULTIPLIER);
}

/*
 * Swap between two bytes
 *
 * @x [in/out]: pointer to first byte
 * @y [in/out]: pointer to second byte
 */
__device__ __forceinline__ void
gpu_swap(char *x, char *y)
{
    char t = *x;

    *x = *y;
    *y = t;
}

/*
 * A utility function to reverse a string
 *
 * @length [in]: string length
 * @str [in/out]: string to reverse
 */
__device__ __forceinline__ void
gpu_reverse(int length, char *str)
{
    int start = 0;
    int end = length -1;
    while (start < end)
    {
        gpu_swap((str+start), (str+end));
        start++;
        end--;
    }
}

/*
 * Convert integer value to string
 *
 * num [in]: integer value
 * base [in]: which base should convert the integer value
 * str [out]: converted integer value to string
 */
__device__ __forceinline__ void
gpu_itoa(uint8_t num, int base, char* str)
{
	int i = 0;

	/* Handle 0 explicitly, otherwise empty string is printed for 0 */
	if (num == 0)
	{
		str[i++] = '0';
		str[i++] = '0';
		str[i] = '\0';
		return;
	}

	/* Process individual digits */
	while (num != 0)
	{
		int rem = num % base;
		str[i++] = (rem > 9)? (rem-10) + 'a' : rem + '0';
		num = num/base;
	}

	/* Insert zero at the begining if the number is individual digit */
	if (i == 1) {
		str[i] = '0';
		i++;
	}

	/* Append string terminator */
	str[i] = '\0';

	/* Reverse the string */
	gpu_reverse(i, str);

	return;
}

/*
 * Conver MAC address to string
 *
 * @ether_addr [in]: array of MAC address parts
 * @output_string [out]: MAC address, string format
 */
__device__ __forceinline__ void
gpu_mac_to_string(const uint8_t ether_addr[6], char *output_string)
{
	int i;
	char buffer[10];

	for (i = 0; i < RTE_ETHER_ADDR_LEN; i++) {
		gpu_itoa(ether_addr[i], 16, buffer);
		memcpy(output_string + i * 3, buffer, 2);
		*(output_string + 2 + i * 3) = ':';
	}
	*(output_string + 2 + (i - 1) * 3) = '\0';
}

/*
 * Calculate string length
 *
 * @str [in]: pointer to string
 * @return: string length
 */
__device__ __forceinline__ int
gpu_strlen(char *str) {
	int len = 0;

	if (str == NULL)
		return len;

	while (str[len] != '\0')
		len++;
	return len;
}

/*
 * Convert IPV6 address to string
 *
 * @ip_addr [in]: array of IP address parts
 * @output_string [out]: IP address, string format
 */
__device__ __forceinline__ void
gpu_ipv6_to_string(const uint8_t ip_addr[8], char *output_string)
{
	int i, offset = 0;
	char buffer[10];

	for (i = 0; i < 8; i++) {
		gpu_itoa(__bswap_constant_8(ip_addr[i]), 16, buffer);
		if (i > 0 && i % 2 != 0)
			offset++;
		memcpy(output_string + i * 2 + offset, buffer, 2);
		*(output_string + 4 + i * 5) = ':';
	}
	*(output_string + 4 + (i - 1) * 5) = '\0';
}

/*
 * Convert IPV4 address to string
 *
 * @ip_addr [in]: IP address, integer format
 * @output_string [out]: IP address, string format
 */
__device__ __forceinline__ void
gpu_ipv4_to_string(const uint32_t ip_addr, char *output_string)
{
	int i;
	char buffer[10] = {0};
	uint32_t ip = ip_addr;
	uint32_t field;
	int part_size = 0, end_buffer = 0;

	for (i = 0; i < 4; i++) {
		field = ip & (0xff << (8 * (3 - i)));
		field = (field >> (8 * (3 - i)));
		gpu_itoa(field, 10, buffer);
		if (buffer[0] == '0') {
			gpu_swap(buffer, buffer + 1);
			buffer[1] = '\0';
		}
		part_size = gpu_strlen(buffer);
		memcpy((output_string + end_buffer), buffer, part_size);
		*(output_string + part_size + end_buffer) = '.';
		end_buffer += (part_size + 1);
	}
	*(output_string + end_buffer + 1) = '\0';
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GPU_UTILS_H */
