#include <pcap.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int custom_tcpdump_capture(const char* iface, const char* custom_filter, void* buffer, size_t buffer_size) {
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle;
    struct bpf_program bpf;
    bpf_u_int32 net = 0, mask = 0;
    size_t offset = 0;
    int snaplen = 65535;
    int promisc = 1;
    int timeout_ms = 1000;

    // 1. 打开网络接口进行抓包
    if ((handle = pcap_open_live(iface, snaplen, promisc, timeout_ms, errbuf)) == NULL) {
        fprintf(stderr, "Error: cannot open device %s - %s\n", iface, errbuf);
        return -1;
    }

    // 可选：获取网络号和子网掩码（用于编译过滤器时）
    if (pcap_lookupnet(iface, &net, &mask, errbuf) == -1) {
        fprintf(stderr, "Warning: could not get netmask for device %s, %s\n", iface, errbuf);
        net = 0;
        mask = 0;
    }

    // 2. 编译并设置过滤规则（如果提供了 custom_filter）
    if (custom_filter != NULL && strlen(custom_filter) > 0) {
        if (pcap_compile(handle, &bpf, custom_filter, 1, net) == -1) {
            fprintf(stderr, "Error: bad filter \"%s\" - %s\n", custom_filter, pcap_geterr(handle));
            pcap_close(handle);
            return -2;
        }
        if (pcap_setfilter(handle, &bpf) == -1) {
            fprintf(stderr, "Error: cannot set filter \"%s\" - %s\n", custom_filter, pcap_geterr(handle));
            pcap_freecode(&bpf);
            pcap_close(handle);
            return -3;
        }
        pcap_freecode(&bpf);  // 释放过滤程序资源
    }

    // 3. 抓包循环，将数据拷贝到用户缓冲区
    const u_char *packet;
    struct pcap_pkthdr header;
    while ((packet = pcap_next(handle, &header)) != NULL) {
        // 如果抓到的数据包长度超过剩余缓冲区，则停止抓取
        if (header.caplen > buffer_size - offset) {
            fprintf(stderr, "Buffer is full, stopping capture.\n");
            break;
        }
        // 将数据包拷贝到缓冲区
        memcpy((u_char*)buffer + offset, packet, header.caplen);
        offset += header.caplen;
    }

    // 4. 关闭抓包句柄
    pcap_close(handle);
    return 0;
}
