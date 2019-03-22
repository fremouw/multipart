#ifndef _MULTIPART_H_
#define _MULTIPART_H_

#include <esp_system.h>
#include <stdio.h>

//
// [RFC2388] suggested that multiple files for a single form field be
// transmitted using a nested "multipart/mixed" part.  This usage is
// deprecated.
//
// Note: the (nested) multipart/mixed part is not supported.

#define MULTIPART_MAX_BOUNDARY_LEN 70   // Max boundary is 70 chars (RFC 2046) + \0
#define MULTIPART_MAX_POST_NAME_LEN 32
#define MULTIPART_MAX_POST_FILENAME_LEN 32
#define MULTIPART_MAX_CONTENT_TYPE_LEN 64

typedef struct {
    char b[MULTIPART_MAX_BOUNDARY_LEN + 1];
    size_t b_len;
} multipart_parse_context_t;

typedef struct {
    char name[MULTIPART_MAX_POST_NAME_LEN + 1];
    char filename[MULTIPART_MAX_POST_FILENAME_LEN + 1];
    char content_type[MULTIPART_MAX_CONTENT_TYPE_LEN + 1];
    char* data;
    size_t data_len;
} multipart_post_t;

/**
  * @brief     Init the multipart parser
  *
  *            Only call this function once
  *
  * @param     context  the context as returned by multipart_init
  * @param     header  raw header, multipart/form-data; boundary=----<theboundary>
  *
  * @return
  *    - ESP_OK: succeed
  *    - ESP_FAIL: if it's not able to find the boundary
  */
esp_err_t multipart_init(multipart_parse_context_t* context, const char* header);

/**
  * @brief     Parse one part of the post data
  *
  *            When data contains multiple parts (or fields), iterate until next is NULL. All data needs to be buffered before 
  *            parsing.
  *
  * @param     context  the context as returned by multipart_init
  * @param     data  raw post data
  * @param     data  parsed post data
  * @param     next  pointer to next part
  *
  * @return
  *    - ESP_OK: succeed
  *    - ESP_FAIL: if data is invalid
  */
esp_err_t multipart_parse(const multipart_parse_context_t* context, const char* data, multipart_post_t* post, char** next);

#endif // _MULTIPART_H_