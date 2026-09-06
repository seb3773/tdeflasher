#include <dlfcn.h>
#include <cstdarg>
#include <curl/curl.h>

static void *s_curl_handle = nullptr;
static CURL* (*s_fn_init)(void) = nullptr;
static void (*s_fn_cleanup)(CURL*) = nullptr;
static CURLcode (*s_fn_setopt)(CURL*, CURLoption, ...) = nullptr;
static CURLcode (*s_fn_perform)(CURL*) = nullptr;
static CURLcode (*s_fn_getinfo)(CURL*, CURLINFO, ...) = nullptr;

static bool load_curl() {
    if (s_curl_handle) return true;
    s_curl_handle = dlopen("libcurl.so.4", RTLD_LAZY | RTLD_GLOBAL);
    if (!s_curl_handle) s_curl_handle = dlopen("libcurl.so", RTLD_LAZY | RTLD_GLOBAL);
    if (!s_curl_handle) s_curl_handle = dlopen("libcurl.so.3", RTLD_LAZY | RTLD_GLOBAL);
    if (!s_curl_handle) return false;

    s_fn_init = (CURL* (*)(void))dlsym(s_curl_handle, "curl_easy_init");
    s_fn_cleanup = (void (*)(CURL*))dlsym(s_curl_handle, "curl_easy_cleanup");
    s_fn_setopt = (CURLcode (*)(CURL*, CURLoption, ...))dlsym(s_curl_handle, "curl_easy_setopt");
    s_fn_perform = (CURLcode (*)(CURL*))dlsym(s_curl_handle, "curl_easy_perform");
    s_fn_getinfo = (CURLcode (*)(CURL*, CURLINFO, ...))dlsym(s_curl_handle, "curl_easy_getinfo");

    return (s_fn_init && s_fn_cleanup && s_fn_setopt && s_fn_perform && s_fn_getinfo);
}

extern "C" {

CURL *curl_easy_init(void) {
    if (!load_curl() || !s_fn_init) return nullptr;
    return s_fn_init();
}

void curl_easy_cleanup(CURL *curl) {
    if (s_fn_cleanup && curl) {
        s_fn_cleanup(curl);
    }
}

CURLcode curl_easy_setopt(CURL *curl, CURLoption option, ...) {
    if (!load_curl() || !s_fn_setopt) return CURLE_FAILED_INIT;
    va_list args;
    va_start(args, option);
    void *val = va_arg(args, void*);
    va_end(args);
    return s_fn_setopt(curl, option, val);
}

CURLcode curl_easy_perform(CURL *curl) {
    if (!load_curl() || !s_fn_perform) return CURLE_FAILED_INIT;
    return s_fn_perform(curl);
}

CURLcode curl_easy_getinfo(CURL *curl, CURLINFO info, ...) {
    if (!load_curl() || !s_fn_getinfo) return CURLE_FAILED_INIT;
    va_list args;
    va_start(args, info);
    void *val = va_arg(args, void*);
    va_end(args);
    return s_fn_getinfo(curl, info, val);
}

}
