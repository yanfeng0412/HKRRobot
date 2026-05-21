#ifndef DIFY_CHAT_H
#define DIFY_CHAT_H

#include <stdint.h>

// 如果需要像 Arduino 那样跳过 TLS 证书验证，可以在这里设为 1
// 推荐在生产环境使用证书 bundle（更安全）并将此保持为 0
#ifndef DIFY_TLS_FORCE_INSECURE
// set to 1 to force insecure TLS (skip server cert verification) — Arduino-like behaviour
// WARNING: insecure is unsafe for production, use only for development/testing
#define DIFY_TLS_FORCE_INSECURE 1
#endif

// 函数声明
void init_dify_chat(void);
void dify_chat_loop(void);

#endif // DIFY_CHAT_H